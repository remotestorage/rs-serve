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

#define EXTRACT_PATH(request) (evhttp_request_get_uri(request) + RS_STORAGE_PATH_LEN)

static char *make_disk_path(struct parsed_path *parsed_path, int *disk_path_len) {
  char *disk_path = NULL;
  if(RS_SERVE_HOMES) {
    // TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
    // 1) get user home
    // 2) append RS_SERVE_HOMES_DIR
    // 3) append scope
    // 4) append rest
  } else {
    // TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
    // 1) start with RS_REAL_STORAGE_ROOT
    // 2) append scope
    // 3) append rest
    /*    *disk_path_len = path_len + RS_REAL_STORAGE_ROOT_LEN;
    disk_path = malloc(*disk_path_len + 1);
    if(disk_path == NULL) {
      perror("malloc() failed while allocating disk path");
      return NULL;
    }
    sprintf(disk_path, "%s%s", RS_REAL_STORAGE_ROOT, path);*/
  }
  return disk_path;
}

/**
 * escape_name(name)
 *
 * Escape all occurances of quotes ('"') and backslashes ('\') to make the
 * resulted string usable in a directory listing.
 *
 * Allocates a new string that needs to be free()d by the caller.
 */
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

/**
 * storage_options(request)
 *
 * Handle a OPTIONS request by sending an empty response with appropriate
 * CORS headers.
 */
void storage_options(struct evhttp_request *request) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
  add_cors_headers(headers);

  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_get(struct evhttp_request *request, int sendbody) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
  add_cors_headers(headers);

  int error_response;
  struct parsed_path *parsed_path = parse_path(EXTRACT_PATH(request),
                                               &error_response);
  if(parsed_path == NULL) {
    evhttp_send_error(request, error_response, NULL);
    return;
  }

  if(authorize_request(request, parsed_path, 0) != 0) {
    free_parsed_path(parsed_path);
    return;
  }

  int disk_path_len;
  char *disk_path = make_disk_path(parsed_path, &disk_path_len);
  if(disk_path == NULL) {
    // failed to allocate disk path
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }

  free_parsed_path(parsed_path);

  struct stat stat_buf;
  struct evbuffer *buf = evbuffer_new();

  if(buf == NULL) {
    perror("evbuffer_new() failed");
    free(disk_path);
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }

  if(stat(disk_path, &stat_buf) != 0) {
    // stat failed
    if(errno != ENOENT) {
      fprintf(stderr, "while getting %s, ", disk_path);
      perror("stat() failed");
    }
    if(path[path_len - 1] == '/') {
      // empty dir listing.
      evhttp_add_header(headers, "Content-Type", "application/json");
      evbuffer_add(buf, "{}", 2);
      evhttp_send_reply(request, HTTP_OK, NULL, buf);
    } else {
      evhttp_send_error(request, HTTP_NOTFOUND, NULL);
    }
  } else if(path[path_len - 1] == '/') {
    // directory requested
    if(S_ISDIR(stat_buf.st_mode)) {
      // directory found
      evhttp_add_header(headers, "Content-Type", "application/json");
      if(sendbody) {
        // GET response
        DIR *dir = opendir(disk_path);
        if(dir == NULL) {
          perror("opendir() failed");
          evhttp_send_error(request, HTTP_INTERNAL, NULL);
        } else {
          struct dirent *entryp = malloc(offsetof(struct dirent, d_name) +
                                         pathconf(disk_path, _PC_NAME_MAX) + 1);
          struct dirent *resultp = NULL;
          if(entryp == NULL) {
            perror("malloc() failed while creating directory pointer");
            evhttp_send_error(request, HTTP_INTERNAL, NULL);
          } else {
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

              char *escaped_name = escape_name(entryp->d_name);
              if(! escaped_name) {
                // failed to allocate name
                evhttp_send_error(request, HTTP_INTERNAL, NULL);
                free(entryp);
                free(dir);
                free(disk_path);
                return;
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
            evhttp_send_reply(request, HTTP_OK, NULL, buf);
          }
          free(entryp);
          closedir(dir);
        }
      } else {
        // HEAD response
        evhttp_send_reply(request, HTTP_OK, NULL, NULL);
      }
    } else {
      // found, but not a directory (FIXME!)
      evhttp_send_error(request, HTTP_NOTFOUND, NULL);
    }
  } else {
    // file requested
    if(S_ISREG(stat_buf.st_mode)) {
      const char *mime_type = magic_file(magic_cookie, disk_path);
      if(mime_type == NULL) {
        fprintf(stderr, "magic failed: %s\n", magic_error(magic_cookie));
      } else {
        evhttp_add_header(headers, "Content-Type", mime_type);
      }
      // file found
      if(sendbody) {
        // GET response
        evbuffer_add_file(buf, open(disk_path, O_NONBLOCK), 0, stat_buf.st_size);
        evhttp_send_reply(request, HTTP_OK, NULL, buf);
      } else {
        // HEAD response
        char len[100];
        // not sure how to format off_t correctly, so casting to longest int.
        snprintf(len, 99, "%lld", (long long int)stat_buf.st_size);
        evhttp_add_header(headers, "Content-Length", len);
        // TODO: add ETag
        evhttp_send_reply(request, HTTP_OK, NULL, NULL);
      }
    } else {
      // found, but not a regular file (FIXME!)
      evhttp_send_error(request, HTTP_NOTFOUND, NULL);
    }
  }

  free(disk_path);
  evbuffer_free(buf);
}

/**
 * storage_put(request)
 *
 * Handle a PUT request. If the request URI carries a trailing slash, the path
 * is interpreted as a directory and thus BADREQUEST is sent.
 * Otherwise a file and possibly it's parent directories are created and the
 * request body is written there.
 *
 * The mode of newly created files depends on RS_FILE_CREATE_MODE (and the
 * current umask).
 */
void storage_put(struct evhttp_request *request) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
  add_cors_headers(headers);

  const char *path = EXTRACT_PATH(request);
  int path_len = strlen(path);

  if(validate_path(request, path) != 0) {
    return;
  }

  char *scope = extract_scope(path);
  if(scope == NULL) {
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }

  if(authorize_request(request, scope, 1) != 0) {
    free(scope);
    return;
  }
  free(scope);

  if(path[path_len - 1] == '/') {
    // no PUT on directories.
    evhttp_send_error(request, HTTP_BADREQUEST, NULL);
  } else {
    struct evbuffer *buf = evhttp_request_get_input_buffer(request);

    // create parent(s)
    char *path_copy = strdup(path);
    char *dir_path = dirname(path_copy);
    char *saveptr = NULL;
    char *dir_name;
    // directory fd for reference
    int dirfd = open(RS_REAL_STORAGE_ROOT, O_RDONLY), prevfd;
    if(dirfd == -1) {
      perror("failed to open() storage root");
      evhttp_send_error(request, HTTP_BADREQUEST, NULL);
      free(path_copy);
      return;
    }
    struct stat dir_stat;
    for(dir_name = strtok_r(dir_path, "/", &saveptr);
        dir_name != NULL;
        dir_name = strtok_r(NULL, "/", &saveptr)) {
      if(fstatat(dirfd, dir_name, &dir_stat, 0) == 0) {
        if(! S_ISDIR(dir_stat.st_mode)) {
          // exists, but not a directory.
          fprintf(stderr, "Can't PUT to %s, found a non-directory parent.\n", path);
          evhttp_send_error(request, HTTP_BADREQUEST, NULL);
          free(path_copy);
          return;
        }
      } else {
        // directory doesn't exist, create it.
        if(mkdirat(dirfd, dir_name, S_IRWXU | S_IRWXG) != 0) {
          perror("mkdirat() failed");
          evhttp_send_error(request, HTTP_INTERNAL, NULL);
          free(path_copy);
          return;
        }
      }
      prevfd = dirfd;
      dirfd = openat(prevfd, dir_name, O_RDONLY);
      close(prevfd);
    }

    // dirname() possibly made previous copy unusable
    strcpy(path_copy, path);
    char *file_name = basename(path_copy);

    // open (and possibly create) file
    int fd = openat(dirfd, file_name,
                    O_NONBLOCK | O_CREAT | O_WRONLY | O_TRUNC,
                    RS_FILE_CREATE_MODE);
    free(path_copy);
    close(dirfd);
    if(fd == -1) {
      perror("openat() failed");
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
    } else if(evbuffer_write(buf, fd) == -1) {
      perror("evbuffer_write() failed");
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
    } else {
      // writing succeeded
      // TODO: add ETag
      evhttp_send_reply(request, HTTP_OK, NULL, NULL);
    }
  }
}

void storage_delete(struct evhttp_request *request) {
  /* struct evkeyvalq *headers = evhttp_request_get_output_headers(request); */
  /* add_cors_headers(headers); */

  /* const char *path = EXTRACT_PATH(request); */
  /* int path_len = strlen(path); */

  /* if(validate_path(request, path) != 0) { */
  /*   return; */
  /* } */

  /* char *scope = extract_scope(path); */
  /* if(scope == NULL) { */
  /*   evhttp_send_error(request, HTTP_INTERNAL, NULL); */
  /*   return; */
  /* } */

  /* if(authorize_request(request, scope, 1) != 0) { */
  /*   free(scope); */
  /*   return; */
  /* } */
  /* free(scope); */

  /* if(path[path_len - 1] == '/') { */
  /*   // no DELETE on directories. */
  /*   evhttp_send_error(request, HTTP_BADREQUEST, NULL); */
  /*   return; */
  /* } */

  /* int disk_path_len; */
  /* char *disk_path = make_disk_path(path, path_len, &disk_path_len); */

  /* if(disk_path == NULL) { */
  /*   evhttp_send_error(request, HTTP_INTERNAL, NULL); */
  /*   return; */
  /* } */

  /* struct stat stat_buf; */
  /* if(stat(disk_path, &stat_buf) == 0) { */
  /*   // file exists, delete it. */
  /*   unlink(disk_path); */

  /*   // remove empty parents */
  /*   char *path_copy = strdup(path); */
  /*   if(path_copy == NULL) { */
  /*     perror("strdup() failed to copy path"); */
  /*     evhttp_send_error(request, HTTP_INTERNAL, NULL); */
  /*     free(disk_path); */
  /*     return; */
  /*   } */
  /*   char *dir_path; */
  /*   int rootdirfd = open(RS_REAL_STORAGE_ROOT, O_RDONLY); */
  /*   if(rootdirfd == -1) { */
  /*     perror("failed to open() storage root"); */
  /*     evhttp_send_error(request, HTTP_INTERNAL, NULL); */
  /*     free(path_copy); */
  /*     free(disk_path); */
  /*     return; */
  /*   } */
  /*   int result; */
  /*   // skip leading slash */
  /*   char *relative_path = path_copy + 1; */
  /*   for(dir_path = dirname(relative_path); */
  /*       dir_path[1] != 0; // reached root */
  /*       dir_path = dirname(dir_path)) { */
  /*     result = unlinkat(rootdirfd, dir_path, AT_REMOVEDIR); */
  /*     if(result != 0) { */
  /*       if(errno == ENOTEMPTY || errno == EEXIST) { */
  /*         // non-empty directory reached */
  /*         break; */
  /*       } else { */
  /*         // other error occured */
  /*         fprintf(stderr, "(while trying to remove %s)\n", dir_path); */
  /*         perror("unlinkat() failed to remove parent directory"); */
  /*         evhttp_send_error(request, HTTP_INTERNAL, NULL); */
  /*         free(path_copy); */
  /*         free(disk_path); */
  /*         return; */
  /*       } */
  /*     } */
  /*   } */
  /*   close(rootdirfd); */
  /*   free(path_copy); */
  /* } else { */
  /*   // file doesn't exist, ignore it. */
  /* } */
  /* evhttp_send_reply(request, HTTP_OK, NULL, NULL); */
  /* free(disk_path); */
}
