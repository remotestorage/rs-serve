/*
 * rs-serve - (c) 2013 Niklas E. Cathor
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "rs-serve.h"

/*
 * Storage Handler
 * ---------------
 *
 * Gets parsed requests and performs the requested actions / sends the requested
 * response.
 *
 * These functions are only called from storage processes.
 */

static char *escape_name(const char *name);
static char *make_etag(struct stat *stat_buf);
static int serve_directory(struct rs_request *request, struct stat *stat_buf);
static int serve_file_head(struct rs_request *request, struct stat *stat_buf,
                           const char *mime_type);
static int serve_file(struct rs_request *request, struct stat *stat_buf);
static int handle_get_or_head(struct rs_request *request, int include_body);
static int content_type_to_xattr(int fd, const char *content_type);


int storage_handle_head(struct rs_request *request) {
  return handle_get_or_head(request, 0);
}

int storage_handle_get(struct rs_request *request) {
  return handle_get_or_head(request, 1);
}

int storage_begin_put(struct rs_request *request) {
  char *path_copy = strdup(request->path);
  if(path_copy == NULL) {
    log_error("strdup() failed: %s", strerror(errno));
    return 500;
  }
  char *dir_path = dirname(path_copy);
  char *saveptr = NULL;
  char *dir_name;
  int dirfd = open("/", O_RDONLY), prevfd;
  if(dirfd == -1) {
    log_error("failed to open() root directory: %s", strerror(errno));
    free(path_copy);
    return 500;
  }
  struct stat dir_stat;
  for(dir_name = strtok_r(dir_path, "/", &saveptr);
      dir_name != NULL;
      dir_name = strtok_r(NULL, "/", &saveptr)) {
    if(fstatat(dirfd, dir_name, &dir_stat, 0) == 0) {
      if(! S_ISDIR(dir_stat.st_mode)) {
        // exists, but not a directory
        log_error("Can't PUT to %s, found a non-directory parent.", request->path);
        close(dirfd);
        free(path_copy);
        return 400;
      } else {
        // directory exists
      }
    } else {
      if(mkdirat(dirfd, dir_name, S_IRWXU | S_IRWXG) != 0) {
        log_error("mkdirat() failed: %s", strerror(errno));
        close(dirfd);
        free(path_copy);
        return 500;
      }
    }
    prevfd = dirfd;
    dirfd = openat(prevfd, dir_name, O_RDONLY);
    close(prevfd);
    if(dirfd == -1) {
      log_error("failed to openat() next directory (\"%s\"): %s",
                dir_name, strerror(errno));
      free(path_copy);
      return 500;
    }
  }

  char *expectation = NULL;
  struct rs_header *header;
  for(header = request->headers;
      header != NULL;
      header = header->next) {
    if(strcmp(header->key, "Expect") == 0) {
      expectation = header->value;
      break;
    }
  }

  // dirname() possibly made previous copy unusable
  strcpy(path_copy, request->path);
  char *file_name = basename(path_copy);

  // open (and possibly create) file
  int fd = openat(dirfd, file_name,
                  O_NONBLOCK | O_CREAT | O_WRONLY | O_TRUNC,
                  RS_FILE_CREATE_MODE);
  free(path_copy);
  close(dirfd);

  if(fd == -1) {
    log_error("openat() failed to open file \"%s\": %s", file_name, strerror(errno));
    return 500;
  }

  request->file_fd = fd;

  if(expectation != NULL) {
    if(strcasecmp(expectation, "100-continue") == 0) {
      send_response_head(request, 100, NULL);
    } else {
      log_error("Expectation \"%s\" cannot be met.", expectation);
      return 417;
    }
  }

  return 0;
}

int storage_end_put(struct rs_request *request) {
  struct stat stat_buf;
  log_debug("fstatting now");
  if(fstat(request->file_fd, &stat_buf) != 0) {
    log_error("fstat() after PUT failed: %s", strerror(errno));
    return 500;
  }
  log_debug("trying to find content-type header");
  struct rs_header *header;
  char *content_type = "application/octet-stream; charset=binary";
  for(header = request->headers;
      header != NULL;
      header = header->next) {
    if(strcmp(header->key, "Content-Type") == 0) {
      log_debug("got content type header value: %s", header->value);
      content_type = header->value;
      break;
    }
  }
  if(content_type_to_xattr(request->file_fd, content_type) != 0) {
    log_error("Setting xattr for content type failed. Ignoring.");
  }
  close(request->file_fd);
  int head_result = serve_file_head(request, &stat_buf, content_type);
  if(head_result != 0) {
    return head_result;
  }
  send_response_empty(request);
  return 0;
}

int storage_handle_delete(struct rs_request *request) {
  return 501;
}

static char *make_etag(struct stat *stat_buf) {
  char *etag = malloc(21);
  if(etag == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return NULL;
  }
  snprintf(etag, 20, "%lld", ((long long int)stat_buf->st_mtime) * 1000);
  return etag;
}

// escape backslashes (/) and double quotes (") to put the given string
// in quoted JSON strings.
static char *escape_name(const char *name) {
  int max_len = strlen(name) * 2, i = 0;
  char *escaped = malloc(max_len + 1);
  if(escaped == NULL) {
    perror("malloc() failed");
    return NULL;
  }
  const char *name_p;
  for(name_p = name; *name_p != 0; name_p++) {
    if(*name_p == '"' || *name_p == '\\') {
      escaped[i++] = '\\';
    }
    escaped[i++] = *name_p;
  }
  escaped[i++] = 0;
  escaped = realloc(escaped, i);
  return escaped;
}

// serve a directory response for the given request
static int serve_directory(struct rs_request *request, struct stat *stat_buf) {
  struct evbuffer *buf = evbuffer_new();
  if(buf == NULL) {
    log_error("evbuffer_new() failed: %s", strerror(errno));
    return 500;
  }
  DIR *dir = opendir(request->path);
  if(dir == NULL) {
    log_error("opendir() failed: %s", strerror(errno));
    return 500;
  }
  struct dirent *entryp = malloc(offsetof(struct dirent, d_name) +
                                 pathconf(request->path, _PC_NAME_MAX) + 1);
  struct dirent *resultp = NULL;
  if(entryp == NULL) {
    log_error("malloc() failed while creating directory pointer: %s",
              strerror(errno));
    return 500;
  }
  struct stat file_stat_buf;
  int entry_len;
  int first = 1;
  for(;;) {
    readdir_r(dir, entryp, &resultp);
    if(resultp == NULL) {
      break;
    }
    if(strcmp(entryp->d_name, ".") == 0 ||
       strcmp(entryp->d_name, "..") == 0) {
      // skip.
      continue;
    }
    if(first) {
      evbuffer_add(buf, "{", 1);
      first = 0;
    } else {
      evbuffer_add(buf, ",", 1);
    }
    entry_len = strlen(entryp->d_name);
    char full_path[request->path_len + entry_len + 1];
    sprintf(full_path, "%s%s", request->path, entryp->d_name);
    stat(full_path, &file_stat_buf);
    
    char *escaped_name = escape_name(entryp->d_name);
    if(! escaped_name) {
      // failed to allocate name
      free(entryp);
      free(dir);
      return 500;
    }
    evbuffer_add_printf(buf, "\"%s%s\":%lld", escaped_name,
                        S_ISDIR(file_stat_buf.st_mode) ? "/" : "",
                        ((long long)file_stat_buf.st_mtime) * 1000);
    free(escaped_name);
  }
  if(first) {
    // empty directory.
    evbuffer_add(buf, "{", 1);
  }
  evbuffer_add(buf, "}", 1);
  struct rs_header type_header = {
    .key = "Content-Type",
    .value = "application/json",
    .next = &RS_DEFAULT_HEADERS
  };
  struct rs_header etag_header = {
    .key = "ETag",
    .value = make_etag(stat_buf),
    .next = &type_header
  };
  if(etag_header.value == NULL) {
    log_error("make_etag() failed");
    free(entryp);
    closedir(dir);
    return 500;
  }
  send_response_head(request, 200, &etag_header);
  free(etag_header.value);
  send_response_body(request, buf);
  free(entryp);
  closedir(dir);
  return 0;
}

static char *get_xattr(const char *path, const char *key, int maxlen) {
  int len = 32;
  char *value = malloc(32);
  for(value = malloc(len);len<=maxlen;value = realloc(value, len+=16)) {
    if(value == NULL) {
      log_error("malloc() / realloc() failed: %s", strerror(errno));
      return NULL;
    }
    int actual_len = getxattr(path, key, value, len);
    log_debug("getxattr() for key %s returned actual length of %d bytes", key, actual_len);
    if(actual_len > 0) {
      return value;
    } else {
      if(errno == ERANGE) {
        // buffer too small.
        continue;
      } else if(errno == ENOATTR) {
        // attribute not set
        free(value);
        return NULL;
      } else if(errno == ENOTSUP) {
        // xattr not supported
        log_error("File system doesn't support extended attributes! You may want to use another one.");
        free(value);
        return NULL;
      } else {
        log_error("Unexpected error while getting %s attribute: %s", key, strerror(errno));
        free(value);
        return NULL;
      }
    }
  }
  log_error("%s attribute seems to be longer than %d bytes. That is simply unreasonable.", key, maxlen);
  free(value);
  return NULL;
}

static char *content_type_from_xattr(const char *path) {
  char *mime_type = get_xattr(path, "user.mime_type", 128);
  if(mime_type == NULL) {
    return NULL;
  }
  char *charset = get_xattr(path, "user.charset", 64);
  if(charset == NULL) {
    return mime_type;
  }
  int mt_len = strlen(mime_type);
  char *content_type = realloc(mime_type, mt_len + strlen(charset) + 10 + 1);
  if(content_type == NULL) {
    log_error("realloc() failed: %s", strerror(errno));
    free(charset);
    return mime_type;
  }
  log_debug("content_type now: %s (also charset is \"%s\")", content_type, charset);
  sprintf(content_type + mt_len, "; charset=%s", charset);
  free(charset);
  return content_type;
}

static int content_type_to_xattr(int fd, const char *content_type) {
  char *content_type_copy = strdup(content_type), *saveptr = NULL;
  if(content_type_copy == NULL) {
    log_error("strdup() failed: %s", strerror(errno));
    return -1;
  }
  char *mime_type = strtok_r(content_type_copy, ";", &saveptr);
  log_debug("extracted mime type: %s", mime_type);
  char *rest = strtok_r(NULL, "", &saveptr);
  char *charset_begin = NULL, *charset = NULL;
  if(rest) {
    charset_begin = strstr(rest, "charset=");
    if(charset_begin) {
      charset = charset_begin + 8;
      log_debug("extracted charset: %s", charset);
    }
  }
  if(charset == NULL) {
    // FIXME: should this rather be binary or us-ascii?
    charset = "UTF-8";
    log_debug("guessed charset: %s", charset);
  }
  if(fsetxattr(fd, "user.mime_type", mime_type, strlen(mime_type) + 1, 0) != 0) {
    log_error("fsetxattr() failed: %s", strerror(errno));
    free(content_type_copy);
    return -1;
  }
  log_debug("fsetxattr (mime_type) done");
  if(fsetxattr(fd, "user.charset", mime_type, strlen(charset) + 1, 0) != 0) {
    log_error("fsetxattr() failed: %s", strerror(errno));
    free(content_type_copy);
    return -1;
  }
  log_debug("fsetxattr (charset) done");
  free(content_type_copy);
  return 0;
}


static int serve_file_head(struct rs_request *request, struct stat *stat_buf, const char *mime_type) {
  log_debug("serve_file_head for path %s", request->path);
  int free_mime_type = 0;
  // mime type is either passed in ... (such as for directory listings)
  if(mime_type == NULL) {
    // ... or detected based on xattr
    mime_type = content_type_from_xattr(request->path);
    if(mime_type == NULL) {
      // ... or guessed by libmagic
      log_debug("mime type not given, detecting...");
      mime_type = magic_file(magic_cookie, request->path);
      if(mime_type == NULL) {
        // ... or defaulted to "application/octet-stream"
        log_error("magic failed: %s", magic_error(magic_cookie));
        mime_type = "application/octet-stream; charset=binary";
      }
    } else {
      // xattr detected mime type and allocated memory for it
      free_mime_type = 1;
    }
  }
  struct rs_header content_type_header = {
    .key = "Content-Type",
    // header won't be freed (it's completely stack based / constant),
    // so cast is valid here to satisfy type check.
    // (struct rs_header.value cannot be constant, because in the case of a request
    //  header it will be free()'d at some point)
    .value = (char*)mime_type,
    .next = &RS_DEFAULT_HEADERS
  };
  char *length_string = malloc(24);
  if(length_string == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return 500;
  }
  snprintf(length_string, 24, "%ld", stat_buf->st_size);
  struct rs_header length_header = {
    .key = "Content-Length",
    .value = length_string,
    .next = &content_type_header
  };
  char *etag_string = make_etag(stat_buf);
  if(etag_string == NULL) {
    log_error("make_etag() failed");
    free(length_string);
    return 500;
  }
  struct rs_header etag_header = {
    .key = "ETag",
    .value = etag_string,
    .next = &length_header
  };
  send_response_head(request, 200, &etag_header);
  free(etag_string);
  free(length_string);
  if(free_mime_type) {
    free((char*)mime_type);
  }
  return 0;
}

// serve a file body for the given request
static int serve_file(struct rs_request *request, struct stat *stat_buf) {
  int fd = open(request->path, O_RDONLY | O_NONBLOCK);
  if(fd < 0) {
    log_error("open() failed: %s", strerror(errno));
    return 500;
  }
  send_response_body_fd(request, fd);
  return 0;
}

static int handle_get_or_head(struct rs_request *request, int include_body) {
  // stat
  struct stat stat_buf;
  if(stat(request->path, &stat_buf) != 0) {
    if(errno != ENOENT) {
      log_error("stat() failed for path \"%s\": %s", request->path, strerror(errno));
      return 500;
    } else {
      return 404;
    }
  }
  // check for directory
  if(request->path[request->path_len - 1] == '/') {
    // directory requested
    if(! S_ISDIR(stat_buf.st_mode)) {
      // not a directory.
      return 404;
    }
    // directory found
    if(include_body) {
      return serve_directory(request, &stat_buf);
    } else {
      int head_result = serve_file_head(request, &stat_buf, "application/json");
      if(head_result != 0) {
        return head_result;
      }
      send_response_empty(request);
      return 0;
    }
  } else {
    // file requested
    if(S_ISDIR(stat_buf.st_mode)) {
      // found, but is a directory
      return 404;
    }
    // file found
    int head_result = serve_file_head(request, &stat_buf, NULL);
    if(head_result != 0) {
      return head_result;
    }
    if(include_body) {
      return serve_file(request, &stat_buf);
    } else {
      send_response_empty(request);
      return 0;
    }
  }
}
