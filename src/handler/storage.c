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

// serve a file response for the given request
int serve_file(struct rs_request *request) {
  return 501;
}

int storage_handle_head(struct rs_request *request) {
  return 501;
}

int storage_handle_get(struct rs_request *request) {
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
    return serve_directory(request);
  } else {
    // file requested
    if(S_ISDIR(stat_buf.st_mode)) {
      // found, but is a directory
      return 404;
    }
    // file found
    return serve_file(request);
  }
}

int storage_handle_put(struct rs_request *request) {
  return 501;
}

int storage_handle_delete(struct rs_request *request) {
  return 501;
}
