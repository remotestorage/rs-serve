/*
 * trie.c / trie.h
 * (c) 2013 ggrin // FIXME!!!
 * (c) 2013 Niklas E. Cathor
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TRIE_H_
#define _TRIE_H_

typedef struct trie_node TrieNode;

/**
 * new_trie()
 *
 * Constructs a new Trie root node.
 *
 * If allocation of memory for the node fails, returns NULL.
 */
TrieNode *new_trie();

/**
 * trie_insert()
 *
 * Inserts given `value' for given `key' in the Trie identified by `parent'.
 * Usually `parent' will be a Trie root, constructed with new_trie().
 *
 * Returns zero if insertion succeeded. If at any point memory allocation fails,
 * returns -1.
 */
int trie_insert(TrieNode *parent, const char *key, void *value);

/**
 * trie_search()
 *
 * Searches through the Trie identified by `parent' for the value stored
 * under given `key'.
 * Usually `parent' will be a Trie root, constructed with new_trie().
 * 
 * Returns either a pointer to the value, or NULL if there is nothing stored
 * under the given key.
 */
void *trie_search(TrieNode *parent, const char *key);

#endif
