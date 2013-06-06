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

/**
 * validate_path(request, path)
 *
 * Protect from directory traversal attacks and verify that path begins with a
 * forward slash ('/').
 *
 * Returns zero when path is valid, else returns -1 and sends a BADREQUEST response.
 */
static int validate_path(const char *path) {
  if(*path != '/') {
    return 0;
  } else if(strstr(path, "../") == NULL) {
    return 0;
  } else {
    return -1;
  }
}

static char *extract_path_part(const char *path, const char **rest) {
  char *part = NULL;
  int i;
  for(i=1; // skip initial slash
      path[i] != 0 && path[i] != '/';
      i++);
  if(path[i] != '/') {
    // file directly below root requested, root scope required.
    part = strdup("");
    *rest = path;
  } else {
    part = strndup(path + 1, i - 1);
    *rest = path + i;
  }
  log_debug("extract_path_part(\"%s\") -> \"%s\" (rest: \"%s\")", path, part, *rest);
  return part;
}

struct parsed_path *parse_path(const char *path, int *error_response) {
  const char *orig_path = path;
  if(validate_path(path) != 0) {
    *error_response = HTTP_BADREQUEST;
    return NULL;
  }
  struct parsed_path *parsed_path = malloc(sizeof(struct parsed_path));
  if(parsed_path == NULL) {
    perror("failed to allocate memory for scope");
    *error_response = HTTP_INTERNAL;
    return NULL;
  }
  memset(parsed_path, 0, sizeof(struct parsed_path));
  parsed_path->user = extract_path_part(path, &path);
  if(parsed_path->user == NULL) {
    free(parsed_path);
    *error_response = HTTP_INTERNAL;
    return NULL;
  }
  // no user given
  if(*parsed_path->user == 0) {
    free(parsed_path->user);
    free(parsed_path);
    *error_response = HTTP_BADREQUEST;
    return NULL;
  }
  parsed_path->scope = extract_path_part(path, &path);
  if(parsed_path->scope == NULL) {
    free(parsed_path->user);
    free(parsed_path);
    *error_response = HTTP_INTERNAL;
    return NULL;
  }
  parsed_path->rest = path;
  log_debug("parsed path { user: \"%s\", scope: \"%s\", rest: \"%s\" } from path \"%s\"",
            parsed_path->user, parsed_path->scope, parsed_path->rest, orig_path);
  return parsed_path;
}

void free_parsed_path(struct parsed_path *pp) {
  if(pp->user) {
    free(pp->user);
  }
  if(pp->scope) {
    free(pp->scope);
  }
  // (note that pp->rest may *not* be free()d)
  free(pp);
}
