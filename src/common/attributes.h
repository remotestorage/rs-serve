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

#ifndef RS_COMMON_ATTRIBUTES_H
#define RS_COMMON_ATTRIBUTES_H

char *get_xattr(const char *path, const char *key, size_t maxlen);
char *get_meta_attr(const char *path, const char *key, size_t maxlen);

int set_xattr(const char *path, const char *key, const char *value, size_t len);
int set_meta_attr(const char *path, const char *key, const char *value, size_t len);

int content_type_to_xattr(const char *path, const char *content_type);
char *content_type_from_xattr(const char *path);

char *get_etag(const char *disk_path);

// Macro: get_meta(path, key, maxlen)
// Get meta information with given key about file given by path.
// `key' must be a static string.
#define get_meta(path, key, maxlen)             \
  (RS_USE_XATTR ?                               \
   get_xattr(path, "user." key, maxlen) :       \
   get_meta_attr(path, key, maxlen))

#define set_meta(path, key, value, len)         \
  (RS_USE_XATTR ?                               \
   set_xattr(path, "user." key, value, len) :   \
   set_meta_attr(path, key, value, len))

#endif
