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

void free_auth_scope(struct rs_auth_scope *scope) {
  if(scope->name) {
    free(scope->name);
  }
  if(scope->next) {
    free_auth_scope(scope->next);
  }
  free(scope);
}

struct rs_auth_scope *make_auth_scope(const char *scope_string, struct rs_auth_scope *next) {
  struct rs_auth_scope *scope = malloc(sizeof(struct rs_auth_scope));
  if(scope == NULL) {
    perror("malloc() failed to allocate new auth scope");
    return NULL;
  }
  memset(scope, 0, sizeof(struct rs_auth_scope));

  int i;
  for(i=0; scope_string[i] != 0 && scope_string[i] != ':'; i++);
  scope->name = strndup(scope_string, i);
  if(! scope->name) {
    perror("strndup() failed");
    free_auth_scope(scope);
    return NULL;
  }

  // "root" scope is coerced into the empty string
  if(strcmp(scope->name, "root") == 0) {
    scope->name = realloc(scope->name, 1);
    *scope->name = 0;
  }

  if(strcmp(scope_string + i, ":rw") == 0) {
    scope->write = 1;
  } else {
    scope->write = 0;
  }

  scope->next = next;

  return scope;
}

void free_auth(struct rs_authorization *auth) {
  if(auth->scope) {
    free_auth_scope(auth->scope);
  }
  if(auth->token) {
    free(auth->token);
  }
  free(auth);
}
