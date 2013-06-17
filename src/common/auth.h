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

#ifndef RS_COMMON_AUTH_H
#define RS_COMMON_AUTH_H

#include <sys/types.h>

struct rs_scope {
  char *name;
  int len;
  char write;

  struct rs_scope *next;
};

struct rs_authorization {
  const char *username;
  const char *token;
  struct rs_scope *scopes;
};

void open_authorizations(char *mode);
void close_authorizations();
off_t find_auth_line(const char *username, const char *token);
int add_authorization(struct rs_authorization *auth);
int remove_authorization(struct rs_authorization *auth);
void list_authorizations();
void print_authorization(struct rs_authorization *auth);
struct rs_authorization *lookup_authorization(const char *username, const char *token);

#endif
