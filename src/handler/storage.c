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
static int compare_version(struct stat *stat_buf, const char *expected);

evhtp_res storage_handle_head(evhtp_request_t *request) {
  return handle_get_or_head(request, 0);
}

evhtp_res storage_handle_get(evhtp_request_t *request) {
  log_debug("storage_handle_get()");
  return handle_get_or_head(request, 1);
}

evhtp_res storage_handle_put(evhtp_request_t *request) {
  log_debug("HANDLE PUT");

  if(request->uri->path->file == NULL) {
    // PUT to directories aren't allowed
    return 400;
  }

  char *storage_root = NULL;
  char *disk_path = make_disk_path(request->uri->path->match_start,
                                   request->uri->path->match_end,
                                   &storage_root);
  if(disk_path == NULL) {
    return 500;
  }

  // check if file exists (needed for preconditions and response code)
  struct stat stat_buf;
  memset(&stat_buf, 0, sizeof(struct stat));
  int exists = stat(disk_path, &stat_buf) == 0;

  // check preconditions
  do {

    // PUT and DELETE requests MAY have an 'If-Match' request header [HTTP], and
    // MUST fail with a 412 response code if that doesn't match the document's
    // current version.

    evhtp_header_t *if_match = evhtp_headers_find_header(request->headers_in, "If-Match");
    if(if_match && ((!exists) || compare_version(&stat_buf, if_match->val) != 0)) {
      return 412;
    }

    // A PUT request MAY have an 'If-None-Match:*' header [HTTP], in which
    // case it MUST fail with a 412 response code if the document already
    // exists.

    evhtp_header_t *if_none_match = evhtp_headers_find_header(request->headers_in, "If-None-Match");
    if(if_none_match && strcmp(if_none_match->val, "*") == 0 && exists) {
      return 412;
    }

  } while(0);

  // create parent directories
  do {

    char *path_copy = strdup(request->uri->path->match_end);
    if(path_copy == NULL) {
      log_error("strdup() failed: %s", strerror(errno));
      free(disk_path);
      free(storage_root);
      return 500;
    }
    char *dir_path = dirname(path_copy);
    char *saveptr = NULL;
    char *dir_name;
    int dirfd = open(storage_root, O_RDONLY), prevfd;
    if(dirfd == -1) {
      log_error("failed to open() storage path (\"%s\"): %s", storage_root, strerror(errno));
      free(disk_path);
      free(path_copy);
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
          free(path_copy);
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
          free(path_copy);
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
        free(path_copy);
        free(storage_root);
        return 500;
      }
    }

    free(path_copy);
    free(storage_root);
    close(dirfd);

  } while(0);

  // open (and possibly create) file
  int fd = open(disk_path, O_NONBLOCK | O_CREAT | O_WRONLY | O_TRUNC,
                RS_FILE_CREATE_MODE);

  if(fd == -1) {
    log_error("open() failed to open file \"%s\": %s", disk_path, strerror(errno));
    free(disk_path);
    return 500;
  }

  // write buffered data
  // TODO: open (and write) file earlier in the request, so it doesn't have to be buffered completely.
  evbuffer_write(request->buffer_in, fd);

  char *content_type = "application/octet-stream; charset=binary";
  evhtp_kv_t *content_type_header = evhtp_headers_find_header(request->headers_in, "Content-Type");

  if(content_type_header != NULL) {
    content_type = content_type_header->val;
  }
  
  // remember content type in extended attributes
  if(content_type_to_xattr(fd, content_type) != 0) {
    log_error("Setting xattr for content type failed. Ignoring.");
  }

  close(fd);

  // stat created file again to generate new etag
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

  return exists ? EVHTP_RES_OK : EVHTP_RES_CREATED;
}

evhtp_res storage_handle_delete(evhtp_request_t *request) {

  if(request->uri->path->file == NULL) {
    // DELETE to directories aren't allowed
    return 400;
  }

  char *storage_root = NULL;
  char *disk_path = make_disk_path(request->uri->path->match_start,
                                   request->uri->path->match_end,
                                   &storage_root);
  if(disk_path == NULL) {
    return 500;
  }

  struct stat stat_buf;
  if(stat(disk_path, &stat_buf) == 0) {

    if(S_ISDIR(stat_buf.st_mode)) {
      return 400;
    }

    // file exists, delete it.
    if(unlink(disk_path) == -1) {
      log_error("unlink() failed: %s", strerror(errno));
      return 500;
    }
    
    /* 
     * remove empty parents
     */
    char *path_copy = strdup(request->uri->path->match_end);
    if(path_copy == NULL) {
      log_error("strdup() failed to copy path: %s", strerror(errno));
      free(disk_path);
      return 500;
    }
    char *dir_path;
    int rootdirfd = open(storage_root, O_RDONLY);
    if(rootdirfd == -1) {
      log_error("failed to open() storage root: %s", strerror(errno));
      free(path_copy);
      free(disk_path);
      return 500;
    }
    int result;
    // skip leading slash
    char *relative_path = path_copy + 1;
    for(dir_path = dirname(relative_path);
        ! (dir_path[0] == '.' && dir_path[1] == 0); // reached root
        dir_path = dirname(dir_path)) {
      log_debug("unlinking %s (relative to %s)", dir_path, storage_root);
      result = unlinkat(rootdirfd, dir_path, AT_REMOVEDIR);
      if(result != 0) {
        if(errno == ENOTEMPTY || errno == EEXIST) {
          // non-empty directory reached
          break;
        } else {
          // other error occured
          log_error("(while trying to remove %s)\n", dir_path);
          log_error("unlinkat() failed to remove parent directory: %s", strerror(errno));
          free(path_copy);
          free(disk_path);
          return 500;
        }
      }
    }
    close(rootdirfd);
    free(path_copy);
  } else {
    // file doesn't exist, ignore it.
  }

  free(storage_root);

  return 200;
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

size_t json_buf_writer(char *buf, size_t count, void *arg) {
  return evbuffer_add((struct evbuffer*)arg, buf, count);
}

// serve a directory response for the given request
static evhtp_res serve_directory(evhtp_request_t *request, char *disk_path, struct stat *stat_buf) {
  size_t disk_path_len = strlen(disk_path);
  struct evbuffer *buf = request->buffer_out;
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

  struct json *json = new_json(json_buf_writer, buf);

  struct stat file_stat_buf;
  int entry_len;

  json_start_object(json);

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
    entry_len = strlen(entryp->d_name);
    char full_path[disk_path_len + entry_len + 1];
    sprintf(full_path, "%s%s", disk_path, entryp->d_name);
    stat(full_path, &file_stat_buf);

    char key_string[entry_len + 2];
    sprintf(key_string, "%s%s", entryp->d_name,
            S_ISDIR(file_stat_buf.st_mode) ? "/": "");
    char *val_string = make_etag(&file_stat_buf);

    json_write_key_val(json, key_string, val_string);

    free(val_string);
  }

  json_end_object(json);

  free_json(json);

  char *etag = make_etag(stat_buf);
  if(etag == NULL) {
    log_error("make_etag() failed");
    free(entryp);
    closedir(dir);
    return 500;
  }

  ADD_RESP_HEADER(request, "Content-Type", "application/json; charset=UTF-8");
  ADD_RESP_HEADER_CP(request, "ETag", etag);

  evhtp_send_reply(request, EVHTP_RES_OK);
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
  if(fsetxattr(fd, "user.charset", charset, strlen(charset) + 1, 0) != 0) {
    log_error("fsetxattr() failed: %s", strerror(errno));
    free(content_type_copy);
    return -1;
  }
  free(content_type_copy);
  return 0;
}


static int serve_file_head(evhtp_request_t *request, char *disk_path, struct stat *stat_buf, const char *mime_type) {
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

  log_info("setting Content-Type of %s: %s", request->uri->path->full, mime_type);
  ADD_RESP_HEADER_CP(request, "Content-Type", mime_type);
  ADD_RESP_HEADER_CP(request, "Content-Length", length_string);
  ADD_RESP_HEADER_CP(request, "ETag", etag_string);

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
  while(evbuffer_read(request->buffer_out, fd, 4096) != 0);
  evhtp_send_reply(request, EVHTP_RES_OK);
  return 0;
}

static char *make_disk_path(char *user, char *path, char **storage_root) {

  // FIXME: use passwd->pwdir instead of /home/{user}/

  // calculate maximum length of path
  int pathlen = ( strlen(user) + strlen(path) +
                  6 + // "/home/"
                  1 + // another slash
                  RS_HOME_SERVE_ROOT_LEN );
  char *disk_path = malloc(pathlen + 1);
  if(disk_path == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return NULL;
  }
  if(storage_root) {
    *storage_root = malloc( 7 + RS_HOME_SERVE_ROOT_LEN + strlen(user) + 1);
    if(*storage_root == NULL) {
      log_error("malloc() failed: %s", strerror(errno));
      free(disk_path);
      return NULL;
    }
    sprintf(*storage_root, "/home/%s/%s", user, RS_HOME_SERVE_ROOT);
  }
  // remove all /.. segments
  // (we don't try to resolve them, but instead treat them as garbage)
  char *pos = NULL;
  while((pos = strstr(path, "/..")) != NULL) {
    int restlen = strlen(pos + 3);
    memmove(pos, pos + 3, restlen);
    pos[restlen] = 0;
  }
  // remove all duplicate slashes (nasty things may be done with them at times)
  while((pos = strstr(path, "//")) != NULL) {
    int restlen = strlen(pos + 2);
    memmove(pos, pos + 2, restlen);
    pos[restlen] = 0;
  }
  // build path
  sprintf(disk_path, "/home/%s/%s%s", user, RS_HOME_SERVE_ROOT, path);
  return disk_path;
}

static evhtp_res handle_get_or_head(evhtp_request_t *request, int include_body) {

  log_debug("HANDLE GET / HEAD (body: %s)", include_body ? "true" : "false");

  char *storage_root = NULL;
  char *disk_path = make_disk_path(request->uri->path->match_start,
                                   request->uri->path->match_end,
                                   &storage_root);
  if(disk_path == NULL) {
    return 500;
  }

  free(storage_root);

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

static int compare_version(struct stat *stat_buf, const char *expected) {
  char version_string[32];
  sprintf(version_string, "%lld", ((long long int) stat_buf->st_mtime) * 1000);
  return strcmp(version_string, expected);
}
