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

#ifndef RS_AUTH_STORE_H
#define RS_AUTH_STORE_H

void init_auth_store(void);
void cleanup_auth_store(void);
int store_authorization(char *bearer_token, char *scope_string);
struct rs_authorization *find_authorization(const char *bearer_token);

#endif

