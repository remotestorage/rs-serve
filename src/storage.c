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

void storage_options(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_get(struct evhttp_request *request, int sendbody) {
  const char *path = EXTRACT_PATH(request);
  size_t path_len = strlen(path);

  int disk_path_len = path_len + RS_STORAGE_ROOT_LEN;
  char disk_path[disk_path_len + 1];
  sprintf(disk_path, "%s%s", RS_STORAGE_ROOT, path);
  struct stat stat_buf;
  struct evbuffer *buf = evbuffer_new();
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);

  if(stat(disk_path, &stat_buf) != 0) {
    // stat failed
    if(errno != ENOENT) {
      fprintf(stderr, "while getting %s, ", disk_path);
      perror("stat() failed");
    }
    evhttp_send_error(request, HTTP_NOTFOUND, NULL);
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
            char rev[100];
            int entry_len;
            int first = 1;
            // FIXME: missing lots of error checking.
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

              evbuffer_add(buf, "\"", 1);
              // FIXME: escape quotes!!!
              evbuffer_add(buf, entryp->d_name, entry_len);
              if(S_ISDIR(file_stat_buf.st_mode)) {
                evbuffer_add(buf, "/", 1);
              }
              evbuffer_add(buf, "\":", 2);

              snprintf(rev, 99, "%lld", ((long long)file_stat_buf.st_mtime) * 1000);
              evbuffer_add(buf, rev, strlen(rev));
            }
            evbuffer_add(buf, "}", 1);
            evhttp_send_reply(request, HTTP_OK, NULL, buf);
          }
          free(entryp);
          free(dir);
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
        evhttp_send_reply(request, HTTP_OK, NULL, NULL);
      }
    } else {
      // found, but not a regular file (FIXME!)
      evhttp_send_error(request, HTTP_NOTFOUND, NULL);
    }
  }

  evbuffer_free(buf);
}

void storage_put(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_delete(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}
