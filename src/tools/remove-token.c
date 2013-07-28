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

#define _GNU_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <db.h>

#include "common/auth.h"

static void print_usage(char *progname) {
  fprintf(stderr, "Usage: %s <user> <token>\n", progname);
}

int main(int argc, char **argv) {
  if(argc < 3) {
    print_usage(argv[0]);
    exit(127);
  }
  open_authorizations("r+");
  struct rs_authorization auth;
  auth.username = argv[1];
  auth.token = argv[2];
  int success = remove_authorization(&auth);
  close_authorizations();
  fprintf(stderr, (success == DB_NOTFOUND) ? "Token not found!\n" : (success == 0 ? "Token removed.\n" : "Error removing token!\n"));
  return success;
}
