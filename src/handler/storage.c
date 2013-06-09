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
static int serve_directory(struct rs_request *request) {
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
  struct rs_header header = {
    .key = "Content-Type",
    .value = "application/json",
    .next = NULL
  };
  send_response_head(request, 200, &header);
  send_response_body(request, buf);
  free(entryp);
  closedir(dir);
  return 0;
}

int serve_file_head(struct rs_request *request, struct stat *stat_buf, const char *mime_type) {
  if(mime_type == NULL) {
    log_debug("mime type not given, detecting...");
    mime_type = magic_file(magic_cookie, request->path);
    if(mime_type == NULL) {
      log_error("magic failed: %s", magic_error(magic_cookie));
      mime_type = "application/octet-stream";
    }
  }
  struct rs_header content_type_header = {
    .key = "Content-Type",
    // header won't be freed (it's completely stack based / constant),
    // so cast is valid here to satisfy type check.
    // (struct rs_header.value cannot be constant, because in the case of a request
    //  header it will be free()'d at some point)
    .value = (char*)mime_type,
    .next = NULL
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
  send_response_head(request, 200, &length_header);
  free(length_string);
  return 0;
}

// serve a file body for the given request
int serve_file(struct rs_request *request, struct stat *stat_buf) {
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
      return serve_directory(request);
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

int storage_handle_head(struct rs_request *request) {
  return handle_get_or_head(request, 0);
}

int storage_handle_get(struct rs_request *request) {
  return handle_get_or_head(request, 1);
}

int storage_handle_put(struct rs_request *request) {
  return 501;
}

int storage_handle_delete(struct rs_request *request) {
  return 501;
}
