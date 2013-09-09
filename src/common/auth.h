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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rs_scope {
  char *name;
  char write;
};

struct rs_scopes {
  uint32_t count;
  struct rs_scope **ptr;
};

struct rs_authorization {
  char *username;
  char *token;
  struct rs_scopes scopes;
};

void open_authorizations(const char *mode);
void close_authorizations();
int add_authorization(struct rs_authorization *auth);
int remove_authorization(struct rs_authorization *auth);
void list_authorizations(const char *username, void (*cb)(struct rs_authorization*, void*), void *ctx);
void print_authorizations(const char *username);
void print_authorization(struct rs_authorization *auth);
struct rs_authorization *lookup_authorization(const char *username, const char *token);

#ifdef __cplusplus
}
#endif

#endif

