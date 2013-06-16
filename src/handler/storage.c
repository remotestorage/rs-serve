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
 */

static char *escape_name(const char *name);
static char *make_etag(struct stat *stat_buf);
static char *make_disk_path(char *user, char *path, char **storage_root);
static evhtp_res serve_directory(evhtp_request_t *request, char *disk_path,
                                 struct stat *stat_buf);
static int serve_file_head(evhtp_request_t *request_t, char *disk_path,
                           struct stat *stat_buf,const char *mime_type);
static int serve_file(evhtp_request_t *request, const char *disk_path,
                      struct stat *stat_buf);
static evhtp_res handle_get_or_head(evhtp_request_t *request, int include_body);
static int content_type_to_xattr(int fd, const char *content_type);


evhtp_res storage_handle_head(evhtp_request_t *request) {
  return handle_get_or_head(request, 0);
}

evhtp_res storage_handle_get(evhtp_request_t *request) {
  return handle_get_or_head(request, 1);
}

evhtp_res storage_handle_put(evhtp_request_t *request) {
  char *storage_root = NULL;
  char *disk_path = make_disk_path(request->uri->path->match_start,
                                   request->uri->path->match_end,
                                   &storage_root);
  if(disk_path == NULL) {
    return 500;
  }
  char *disk_path_copy = strdup(disk_path);
  if(disk_path_copy == NULL) {
    log_error("strdup() failed: %s", strerror(errno));
    free(disk_path);
    free(storage_root);
    return 500;
  }
  char *dir_path = dirname(disk_path_copy);
  char *saveptr = NULL;
  char *dir_name;
  int dirfd = open(storage_root, O_RDONLY), prevfd;
  if(dirfd == -1) {
    log_error("failed to open() storage path (\"%s\"): %s", storage_root, strerror(errno));
    free(disk_path);
    free(disk_path_copy);
    free(storage_root);
    return 500;
  }
  struct stat dir_stat;
  for(dir_name = strtok_r(dir_path, "/", &saveptr);
      dir_name != NULL;
      dir_name = strtok_r(NULL, "/", &saveptr)) {
    if(fstatat(dirfd, dir_name, &dir_stat, 0) == 0) {
      if(! S_ISDIR(dir_stat.st_mode)) {
        // exists, but not a directory
        log_error("Can't PUT to %s, found a non-directory parent.", request->uri->path->full);
        close(dirfd);
        free(disk_path);
        free(disk_path_copy);
        free(storage_root);
        return 400;
      } else {
        // directory exists
      }
    } else {
      if(mkdirat(dirfd, dir_name, S_IRWXU | S_IRWXG) != 0) {
        log_error("mkdirat() failed: %s", strerror(errno));
        close(dirfd);
        free(disk_path);
        free(disk_path_copy);
        free(storage_root);
        return 500;
      }
    }
    prevfd = dirfd;
    dirfd = openat(prevfd, dir_name, O_RDONLY);
    close(prevfd);
    if(dirfd == -1) {
      log_error("failed to openat() next directory (\"%s\"): %s",
                dir_name, strerror(errno));
      free(disk_path);
      free(disk_path_copy);
      free(storage_root);
      return 500;
    }
  }

  free(disk_path_copy);
  free(storage_root);
  close(dirfd);

  // open (and possibly create) file
  int fd = open(disk_path, O_NONBLOCK | O_CREAT | O_WRONLY | O_TRUNC,
                RS_FILE_CREATE_MODE);

  if(fd == -1) {
    log_error("open() failed to open file \"%s\": %s", disk_path, strerror(errno));
    free(disk_path);
    return 500;
  }

  evbuffer_write(request->buffer_in, fd);

  char *content_type = "application/octet-stream; charset=binary";
  evhtp_kv_t *content_type_header = evhtp_headers_find_header(request->headers_in, "Content-Type");

  if(content_type_header != NULL) {
    content_type = content_type_header->val;
  }
  
  if(content_type_to_xattr(fd, content_type) != 0) {
    log_error("Setting xattr for content type failed. Ignoring.");
  }

  close(fd);

  struct stat stat_buf;
  memset(&stat_buf, 0, sizeof(struct stat));
  if(stat(disk_path, &stat_buf) != 0) {
    log_error("failed to stat() file after writing: %s", strerror(errno));
    free(disk_path);
    return 500;
  }

  char *etag_string = make_etag(&stat_buf);

  ADD_RESP_HEADER_CP(request, "Content-Type", content_type);
  ADD_RESP_HEADER_CP(request, "ETag", etag_string);

  free(etag_string);
  free(disk_path);

  return EVHTP_RES_OK;
}

evhtp_res storage_handle_delete(evhtp_request_t *request) {
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
static evhtp_res serve_directory(evhtp_request_t *request, char *disk_path, struct stat *stat_buf) {
  size_t disk_path_len = strlen(disk_path);
  struct evbuffer *buf = evbuffer_new();
  if(buf == NULL) {
    log_error("evbuffer_new() failed: %s", strerror(errno));
    return 500;
  }
  DIR *dir = opendir(disk_path);
  if(dir == NULL) {
    log_error("opendir() failed: %s", strerror(errno));
    return 500;
  }
  struct dirent *entryp = malloc(offsetof(struct dirent, d_name) +
                                 pathconf(disk_path, _PC_NAME_MAX) + 1);
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
    char full_path[disk_path_len + entry_len + 1];
    sprintf(full_path, "%s%s", disk_path, entryp->d_name);
    stat(full_path, &file_stat_buf);

    log_debug("Listing add entry %s", entryp->d_name);
    
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
  evbuffer_add(buf, "}\n", 2);

  char *etag = make_etag(stat_buf);
  if(etag == NULL) {
    log_error("make_etag() failed");
    free(entryp);
    closedir(dir);
    return 500;
  }

  ADD_RESP_HEADER(request, "Content-Type", "application/json; charset=UTF-8");
  ADD_RESP_HEADER_CP(request, "ETag", etag);

  // FIXME: why do I need to use *_chunk_* here???
  evhtp_send_reply_chunk_start(request, EVHTP_RES_OK);
  evhtp_send_reply_chunk(request, buf);
  evhtp_send_reply_chunk_end(request);
  free(etag);
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
  if(fsetxattr(fd, "user.charset", charset, strlen(charset) + 1, 0) != 0) {
    log_error("fsetxattr() failed: %s", strerror(errno));
    free(content_type_copy);
    return -1;
  }
  log_debug("fsetxattr (charset) done");
  free(content_type_copy);
  return 0;
}


static int serve_file_head(evhtp_request_t *request, char *disk_path, struct stat *stat_buf, const char *mime_type) {
  log_debug("serve_file_head for path %s", disk_path);
  int free_mime_type = 0;
  // mime type is either passed in ... (such as for directory listings)
  if(mime_type == NULL) {
    // ... or detected based on xattr
    mime_type = content_type_from_xattr(disk_path);
    if(mime_type == NULL) {
      // ... or guessed by libmagic
      log_debug("mime type not given, detecting...");
      mime_type = magic_file(magic_cookie, disk_path);
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
  char *length_string = malloc(24);
  if(length_string == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return 500;
  }
  snprintf(length_string, 24, "%ld", stat_buf->st_size);
  char *etag_string = make_etag(stat_buf);
  if(etag_string == NULL) {
    log_error("make_etag() failed");
    free(length_string);
    return 500;
  }

  ADD_RESP_HEADER(request, "Content-Type", mime_type);
  ADD_RESP_HEADER_CP(request, "Content-Length", length_string);
  ADD_RESP_HEADER_CP(request, "ETag", etag_string);

  log_debug("HAVE SET CONTENT LENGTH: %s", length_string);

  evhtp_send_reply_chunk_start(request, 200);

  free(etag_string);
  free(length_string);
  if(free_mime_type) {
    free((char*)mime_type);
  }
  return 0;
}

// serve a file body for the given request
static int serve_file(evhtp_request_t *request, const char *disk_path, struct stat *stat_buf) {
  int fd = open(disk_path, O_RDONLY | O_NONBLOCK);
  if(fd < 0) {
    log_error("open() failed: %s", strerror(errno));
    return 500;
  }
  struct evbuffer *buf = evbuffer_new();
  while(evbuffer_read(buf, fd, 4096) != 0) {
    evhtp_send_reply_chunk(request, buf);
  }
  evbuffer_free(buf);
  evhtp_send_reply_chunk_end(request);
  return 0;
}

static char *make_disk_path(char *user, char *path, char **storage_root) {

  // FIXME: use passwd->pwdir instead of /home/{user}/

  // calculate maximum length of path
  int pathlen = ( strlen(user) + strlen(path) +
                  6 + // "/home/"
                  1 + // another slash
                  RS_HOME_SERVE_ROOT_LEN );
  char *disk_path = malloc(pathlen);
  if(disk_path == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return NULL;
  }
  if(storage_root) {
    *storage_root = malloc( 6 + RS_HOME_SERVE_ROOT_LEN + 1 + strlen(user) );
    if(*storage_root == NULL) {
      log_error("malloc() failed: %s", strerror(errno));
      free(disk_path);
      return NULL;
    }
    sprintf(*storage_root, "/home/%s/%s", user, RS_HOME_SERVE_ROOT);
  }
  // remove all /.. segments
  // (we don't try to resolve them, but instead treat them as garbage)
  char *traverse_pos = NULL;
  while((traverse_pos = strstr(path, "/..")) != NULL) {
    int restlen = strlen(traverse_pos + 3);
    memmove(traverse_pos, traverse_pos + 3, restlen);
    traverse_pos[restlen] = 0;
  }
  // build path
  sprintf(disk_path, "/home/%s/%s%s", user, RS_HOME_SERVE_ROOT, path);
  return disk_path;
}
                      

static evhtp_res handle_get_or_head(evhtp_request_t *request, int include_body) {
  char *disk_path = make_disk_path(request->uri->path->match_start,
                                   request->uri->path->match_end, NULL);
  if(disk_path == NULL) {
    return 500;
  }

  log_debug("HANDLE GET OR HEAD, DISK PATH: %s", disk_path);

  // stat
  struct stat stat_buf;
  if(stat(disk_path, &stat_buf) != 0) {
    if(errno != ENOENT) {
      log_error("stat() failed for path \"%s\": %s", disk_path, strerror(errno));
      return 500;
    } else {
      return 404;
    }
  }
  // check for directory
  if(request->uri->path->file == NULL) {
    // directory requested
    if(! S_ISDIR(stat_buf.st_mode)) {
      // not a directory.
      return 404;
    }
    // directory found
    if(include_body) {
      return serve_directory(request, disk_path, &stat_buf);
    } else {
      return serve_file_head(request, disk_path, &stat_buf, "application/json");
    }
  } else {
    // file requested
    if(S_ISDIR(stat_buf.st_mode)) {
      // found, but is a directory
      return 404;
    }
    // file found
    int head_result = serve_file_head(request, disk_path, &stat_buf, NULL);
    if(head_result != 0) {
      return head_result;
    }
    if(include_body) {
      return serve_file(request, disk_path, &stat_buf);
    } else {
      return 0;
    }
  }
}
