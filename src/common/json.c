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

// this is by no means a complete json implementation.
// but it works for all our needs.

struct json {
  json_write_f write;
  void *arg;
  char ss[JSON_MAX_DEPTH]; // stack start
  char *sp; // stack pointer
};

#define JSON_PUSH_NESTING(json) do {                                    \
    if((json->sp - json->ss) + 1 >= JSON_MAX_DEPTH) {                   \
      log_error("BUG: JSON_MAX_DEPTH exceeded!");                       \
    } else {                                                            \
      json->sp++; *json->sp = 0;                                        \
    }                                                                   \
  } while(0)
#define JSON_POP_NESTING(json) do { json->sp--; } while(0)
#define JSON_NESTING_BEGUN(json) (*json->sp == 1)
#define JSON_NESTING_BEGINS(json) (*json->sp == 0)
#define JSON_BEGIN_NESTING(json) do { *json->sp = 1; } while(0);

static char *json_make_string(const char *string, int *len) {
  int max_len = strlen(string) * 2, i = 0;
  char *escaped = malloc(max_len + 3);
  if(escaped == NULL) {
    perror("malloc() failed");
    return NULL;
  }
  const char *str_p;
  escaped[i++] = '"';
  for(str_p = string; *str_p != 0; str_p++) {
    if(*str_p == '"' || *str_p == '\\') {
      escaped[i++] = '\\';
    }
    escaped[i++] = *str_p;
  }
  escaped[i++] = '"';
  escaped[i++] = 0;
  escaped = realloc(escaped, i);
  *len = i - 1;
  return escaped;
}

struct json *new_json(json_write_f writer, void *arg) {
  struct json *json = malloc(sizeof(struct json));
  if(json == NULL) {
    return NULL;
  }
  json->write = writer;
  json->arg = arg;
  json->sp = json->ss;
  return json;
}

void free_json(struct json *json) {
  free(json);
}

void json_start_array(struct json *json) {
  json->write("[", 1, json->arg);
  JSON_PUSH_NESTING(json);
  *json->sp=0;
}

void json_end_array(struct json *json) {
  json->write("]", 1, json->arg);
  JSON_POP_NESTING(json);
}

void json_start_object(struct json *json) {
  json->write("{", 1, json->arg);
  JSON_PUSH_NESTING(json);
}

void json_end_object(struct json *json) {
  json->write("}", 1, json->arg);
  JSON_POP_NESTING(json);
}

void json_write_string(struct json *json, const char *string) {
  int len;
  char *token = json_make_string(string, &len);
  json->write(token, len, json->arg);
  free(token);
}

void json_write_key(struct json *json, const char *key) {
  if(JSON_NESTING_BEGUN(json)) {
    json->write(",", 1, json->arg);
  } else if(JSON_NESTING_BEGINS(json)) {
    JSON_BEGIN_NESTING(json);
  }
  json_write_string(json, key);
  json->write(":", 1, json->arg);
}

void json_write_key_val(struct json *json, const char *key, const char *val) {
  json_write_key(json, key);
  json_write_string(json, val);
}
