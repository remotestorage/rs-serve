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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "common/auth.h"

static void print_usage(char *progname) {
  fprintf(stderr, "Usage: %s <user> <token> <scope1> [<scope2> ... <scopeN>]\n",
          progname);
}

int main(int argc, char **argv) {
  if(argc < 4) {
    print_usage(argv[0]);
    exit(127);
  }
  open_authorizations("a");
  struct rs_authorization auth;
  auth.username = argv[1];
  auth.token = argv[2];
  char *scope_string;
  auth.scopes.count = argc - 3;
  auth.scopes.ptr = malloc(sizeof(struct rs_scope) * auth.scopes.count);
  if(! auth.scopes.ptr) {
    perror("Failed to allocate memory");
    exit(EXIT_FAILURE);
  }
  int i;
  for(i=3;i<argc;i++) {
    scope_string = argv[i];
    struct rs_scope *scope = malloc(sizeof(struct rs_scope));
    char *sptr = scope_string;
    while(*sptr != ':') sptr++;
    *sptr++ = 0;
    scope->name = scope_string;
    if(strcmp(scope->name, "root") == 0) {
      // "root" scope is a special case.
      scope->name = "";
    }
    scope->write = (strcmp(sptr, "rw") == 0) ? 1 : 0;
    auth.scopes.ptr[i-3] = scope;
  }
  add_authorization(&auth);
  print_authorization(&auth);
  printf("\n");
  close_authorizations();
}
