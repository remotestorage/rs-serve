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

#ifndef RS_JSON_H
#define RS_JSON_H

// this should suffice for webfinger.
#define JSON_MAX_DEPTH 5

struct json;
typedef size_t (*json_write_f)(char *buf, size_t count, void *arg);

struct json *new_json(json_write_f writer, void *arg);
void free_json(struct json *json);
void json_start_object(struct json *json);
void json_end_object(struct json *json);
void json_start_array(struct json *json);
void json_end_array(struct json *json);
void json_write_string(struct json *json, const char *string);
void json_write_key(struct json *json, const char *key);
void json_write_key_val(struct json *json, const char *key, const char *val);

#endif

