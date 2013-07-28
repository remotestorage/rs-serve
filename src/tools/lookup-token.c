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
#include "common/auth.h"

int main(int argc, char **argv) {
  if(argc != 3) {
    printf("Usage: %s <username> <token>\n", argv[0]);
    return 127;
  }
  struct rs_authorization *auth;
  open_authorizations("r");
  auth = lookup_authorization(argv[1], argv[2]);
  close_authorizations();
  if(auth) {
    print_authorization(auth);
    return 0;
  } else {
    fprintf(stderr, "No authorization found!\n");
    return 1;
  }
}
