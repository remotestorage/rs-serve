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
#include "trie.h"

TrieNode *auth_token_store;

void init_auth_store(void) {
  auth_token_store = new_trie();
  if(auth_token_store == NULL) {
    perror("Failed to create auth store");
    exit(EXIT_FAILURE);
  }
}

void cleanup_auth_store(void) {
  destroy_trie(auth_token_store);
}

int store_authorization(char *bearer_token, char *scope_string) {
  struct rs_authorization *auth = make_authorization(scope_string);
  if(auth == NULL) {
    return 1;
  }

  auth->token = bearer_token;

  if(trie_insert(auth_token_store, bearer_token, auth) == 0) {
    return 0;
  } else {
    return 1;
  }
}

struct rs_authorization *find_authorization(const char *bearer_token) {
  return (struct rs_authorization*) trie_search(auth_token_store, bearer_token);
}
