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

TrieNode *auth_token_store = NULL;

void init_auth_store(void) {
  auth_token_store = new_trie();
  if(auth_token_store == NULL) {
    perror("Failed to create auth store");
    exit(EXIT_FAILURE);
  }
}

void cleanup_auth_store(void) {
  destroy_trie(auth_token_store);
  auth_token_store = NULL;
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

void print_authorization(void *_auth, void *_fp) {
  FILE *fp = (FILE*) _fp;
  struct rs_authorization *auth = (struct rs_authorization*) _auth;
  struct rs_auth_scope *scope;
  fprintf(fp, "Token: %s\n", auth->token);
  for(scope = auth->scope; scope != NULL; scope = scope->next) {
    fprintf(fp, "  Scope: \"%s\", Mode: %s\n", scope->name, scope->write ? "rw" : "r");
  }
}

void print_authorizations(FILE *fp) {
  iterate_trie(auth_token_store, print_authorization, (void*)fp);
}
