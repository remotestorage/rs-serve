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
  open_authorizations("r+");
  struct rs_authorization auth;
  auth.username = argv[1];
  auth.token = argv[2];
  auth.scopes = NULL;
  char *scope_string;
  int i;
  for(i=3;i<argc;i++) {
    scope_string = argv[i];
    printf("scope string: %s\n", scope_string);
    struct rs_scope *scope = malloc(sizeof(struct rs_scope));
    char *sptr = scope_string;
    while(*sptr != ':') sptr++;
    *sptr++ = 0;
    scope->name = scope_string;
    scope->len = strlen(scope->name);
    if(strcmp(scope->name, "root") == 0) {
      // "root" scope is a special case.
      scope->name = "";
      scope->len = 0;
    }
    scope->write = (strcmp(sptr, "rw") == 0) ? 1 : 0;
    printf("(i.e. name: %s, len: %d, write: %d)\n",
           scope->name, scope->len, scope->write);
    scope->next = auth.scopes;
    auth.scopes = scope;
  }
  add_authorization(&auth);
  close_authorizations();
}
