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

/* static struct rs_process_info *start_storage_process(uid_t uid) { */
/*   struct rs_process_info *storage_process = malloc(sizeof(struct rs_process_info)); */
/*   if(storage_process == NULL) { */
/*     log_error("malloc() failed: %s", strerror(errno)); */
/*     return NULL; */
/*   } */
/*   storage_process->uid = uid; */
/*   if(start_process(storage_process, storage_main) != 0) { */
/*     free(storage_process); */
/*     return NULL; */
/*   } */
/*   return storage_process; */
/* } */

static char *my_strtok(char *string, char *delim, char **saveptr) {
  char *ptr = string ? string : *saveptr, *dp;
  char *start = ptr;
  if(*delim == 0) { // empty delimiter => return entire string.
    log_debug("returning start: %s", start);
    return start;
  }
  for(;*ptr != 0; ptr++) {
    for(dp = delim; *dp != 0; dp++) {
      if(*ptr == *dp) {
        if(*ptr == '\n') {
          log_debug("found newline, previous char is: %d (%d)", *(ptr - 1), '\r');
        }
        *ptr++ = 0;
        *saveptr = ptr;
        return start;
      }
    }
  }
  return NULL;
}

/* int dispatch_request(struct evbuffer *buffer, int fd) { */
/*   size_t buf_len = evbuffer_get_length(buffer); */
/*   char *buf = malloc(buf_len + 1); */
/*   if(buf == NULL) { */
/*     log_error("malloc() failed: %s", strerror(errno)); */
/*     return -1; */
/*   } */
/*   evbuffer_remove(buffer, buf, buf_len); */
/*   buf[buf_len] = 0; // terminate string. */

/*   // parse everything */

/*   log_debug("Got data buf (%d): %s", buf_len, buf); */

/*   char *saveptr; */
/*   char *verb = my_strtok(buf, " ", &saveptr); */
/*   saveptr++; // skip initial slash */
/*   char *username = my_strtok(NULL, "/ ", &saveptr); */
/*   char *file_path = my_strtok(NULL, " ", &saveptr); */
/*   char *http_version = my_strtok(NULL, "\r", &saveptr); */
/*   saveptr++; // skip \n */
/*   char *rest = my_strtok(NULL, "", &saveptr); */

/*   log_debug("verb: %s\nusername: %s\nfile_path: %s\nhttp_version: %s\nrest: %s", verb, username, file_path, http_version, rest); */
/*   if(verb == NULL || username == NULL || file_path == NULL || */
/*      http_version == NULL) { */
/*     //log_debug("verb: %s, username: %s, file_path: %s, http_version: %s, rest: %s", verb, username, file_path, http_version); */
/*     return -1; */
/*   } */

/*   if(strncmp(http_version, "HTTP/1.", 7) != 0) { */
/*     log_error("Invalid request (HTTP version specified as \"%s\")", http_version); */
/*     return -1; */
/*   } */

/*   /\* log_debug("Parsed something. Let's see:\n  verb: %s\n  username: %s\n  path: %s\n  http version: %s\n  rest: %s", *\/ */
/*   /\*           verb, username, file_path, http_version, rest); *\/ */

/*   uid_t uid = user_get_uid(username); */

/*   if(uid == -1) { */
/*     // TODO: send 404 response here */
/*     log_error("User not found: %s", username); */
/*     free(buf); */
/*     return -1; */
/*   } else if(uid == -2) { */
/*     // TODO: send 500 response here */
/*     log_error("Failed to get uid for user: %s", username); */
/*     free(buf); */
/*     return -1; */
/*   } */

/*   // TODO: add check for UID > MIN_UID */
/*   //   (we don't want to fork storage workers for system users) */

/*   // rewrite first line to only include file path */

/*   int new_len = sprintf(buf, "%s /%s %s\r\n%s", verb, file_path, http_version, rest); */
/*   log_debug("reallocating to %d bytes", new_len + 1); */
/*   buf = realloc(buf, ++new_len); */
/*   if(buf == NULL) { */
/*     log_error("BUG: realloc() failed to shrink buffer from %d to %d bytes: %s", */
/*               buf_len + 1, new_len + 1, strerror(errno)); */
/*   } */
/*   buf_len = new_len; */

/*   struct rs_process_info *storage_process = process_find_uid(uid); */
/*   if(storage_process == NULL) { */
/*     storage_process = start_storage_process(uid); */
/*     if(storage_process == NULL) { */
/*       log_error("Failed to start storage process!"); */
/*       free(buf); */
/*       return -1; */
/*     } */
/*   } */
/*   forward_to_process(storage_process, fd, buf, buf_len); */

/*   free(buf); */

/*   return 0; */
/* } */
