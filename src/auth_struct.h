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

#ifndef RS_AUTH_STRUCT_H
#define RS_AUTH_STRUCT_H

struct rs_auth_scope {
  char *name;
  int write;
  struct rs_auth_scope *next;
};

struct rs_authorization {
  char *token;
  struct rs_auth_scope *scope;
};

void free_auth_scope(struct rs_auth_scope *scope);
struct rs_auth_scope *make_auth_scope(const char *scope_string, struct rs_auth_scope *next);
void free_auth(struct rs_authorization *auth);
struct rs_authorization *make_authorization(const char *scope_string);

#endif /* !RS_AUTH_STRUCT_H */
