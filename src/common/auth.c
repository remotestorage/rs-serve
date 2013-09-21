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

#define _GNU_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <db.h>

#include <sys/stat.h>

#include "config.h"
#include "common/auth.h"

#include <assert.h>

void pack_authorization(DBT *dest, struct rs_authorization *src);
int unpack_authorization(struct rs_authorization *dest, DBT *src);

DB *auth_db;
DB_ENV *auth_db_env;

void print_db_error(const DB_ENV *env, const char *errpfx, const char *msg) {
  fprintf(stderr, "DB ERROR: %s: %s\n", errpfx, msg);
}

void open_authorizations(const char *mode) {
  if(auth_db) return;
  uint32_t db_env_flags, db_flags;

  db_env_flags = DB_CREATE | DB_INIT_CDB | DB_INIT_MPOOL;

  if(db_env_create(&auth_db_env, 0) != 0) {
    fprintf(stderr, "db_env_create() failed\n");
    abort(); // FIXME!
  }

  if(auth_db_env->open(auth_db_env, RS_AUTH_DB_PATH, db_env_flags, 0)) {
    fprintf(stderr, "auth_db_env->open() failed\n");
    abort(); // FIXME!
    }

  if(db_create(&auth_db, auth_db_env, 0) != 0) {
    fprintf(stderr, "db_create() failed\n");
    abort(); // FIXME!
  }

  db_flags = DB_CREATE;

  /* if(mode[1] != 'w') { */
  /*   db_flags |= DB_RDONLY; */
  /* } */
  
  if(auth_db->open(auth_db, NULL, NULL, "authorizations", DB_HASH, db_flags, 0) != 0) {
    fprintf(stderr, "auth_db->open() failed\n");
    abort(); // FIXME!
  }

  auth_db->set_errcall(auth_db, print_db_error);
}

void close_authorizations() {
  if(! auth_db) return;
  auth_db->close(auth_db, 0);
  auth_db = NULL;
}

int remove_authorization(struct rs_authorization *auth) {
  uint32_t keylen = strlen(auth->username) + strlen(auth->token) + 1;
  char *key = malloc(keylen + 1);
  if(key == NULL) {
    perror("Failed to allocate memory");
    return -1;
  }
  sprintf(key, "%s|%s", auth->username, auth->token);
  DBT db_key, db_value;
  memset(&db_key, 0, sizeof(db_key));
  memset(&db_value, 0, sizeof(db_value));
  db_key.data = key;
  db_key.size = keylen;
  db_key.ulen = keylen + 1;
  int result = auth_db->del(auth_db, NULL, &db_key, 0);
  if(result != 0) {
    if(result != DB_NOTFOUND) {
      fprintf(stderr, "auth_db->del() failed\n");
    }
    return result;
  }
  return 0;
}


struct rs_authorization *lookup_authorization(const char *username, const char *token) {
  uint32_t keylen = strlen(username) + strlen(token) + 1;
  char *key = malloc(keylen + 1);
  if(key == NULL) {
    perror("Failed to allocate memory");
    return NULL;
  }
  sprintf(key, "%s|%s", username, token);
  DBT db_key, db_value;
  memset(&db_key, 0, sizeof(db_key));
  memset(&db_value, 0, sizeof(db_value));
  db_key.data = key;
  db_key.size = keylen;
  db_key.ulen = keylen + 1;
  db_key.flags = DB_DBT_MALLOC;
  int get_result = auth_db->get(auth_db, NULL, &db_key, &db_value, 0);
  char *msg;
  struct rs_authorization *auth;
  if(get_result == 0) {
    auth = malloc(sizeof(struct rs_authorization));
    if(auth == NULL) {
      perror("Failed to allocate memory");
      return NULL;
    }
    unpack_authorization(auth, &db_value);
    return auth;
  } else if(get_result == DB_NOTFOUND) {
    return NULL;
  } else {
    switch(get_result) {
    case DB_KEYEMPTY: msg = "DB_KEYEMPTY"; break;
    case DB_BUFFER_SMALL: msg = "DB_BUFFER_SMALL"; break;
    case DB_LOCK_DEADLOCK: msg = "DB_LOCK_DEADLOCK"; break;
    case DB_LOCK_NOTGRANTED: msg = "DB_LOCK_NOTGRANTED"; break;
    case DB_REP_HANDLE_DEAD: msg = "DB_REP_HANDLE_DEAD"; break;
    case DB_REP_LEASE_EXPIRED: msg = "DB_REP_LEASE_EXPIRED"; break;
    case DB_REP_LOCKOUT: msg = "DB_REP_LOCKOUT"; break;
    case DB_SECONDARY_BAD: msg = "DB_SECONDARY_BAD"; break;
    case EINVAL: msg = "EINVAL"; break;
    default: msg = "(unknown error)";
    }
    fprintf(stderr, "auth_db->get() failed: %s (%d)\n", msg, get_result);
    abort();
  }
}

void pack_authorization(DBT *dest, struct rs_authorization *src) {
  size_t username_len = strlen(src->username), token_len = strlen(src->token);
  uint32_t size = username_len + 1 + token_len + 1;
  uint32_t offset = 0, scope_len;
  for(int i=0;i<src->scopes.count;i++) {
    size += strlen(src->scopes.ptr[i]->name) + 2;
  }
  size += sizeof(uint32_t); // scopes count
  dest->data = malloc(size);
  if(dest->data == NULL) {
    perror("Failed to allocate memory");
    return;
  }
  //char *written
  // username_len + 1       - username \0
  // token_len + 1          - token \0
  // 4                      - scopes count
  // src->scopes.count * [
  //   scope_len + 1        - scope name \0
  //   1                    - scope write (1|0)
  // ]
  memcpy(dest->data + offset, src->username, username_len + 1); // username
  //written = dest->data + offset;
  offset += username_len + 1;
  //printf("after username, offset is %d (%s)\n", offset, written);
  memcpy(dest->data + offset, src->token, token_len + 1); // token
  //written = dest->data + offset;
  offset += token_len + 1;
  //printf("after token, offset is %d (%s)\n", offset, written);
  memcpy(dest->data + offset, &src->scopes.count, 4);
  offset += 4;
  //printf("after scope count (%d), offset is %d\n", src->scopes.count, offset);
  for(int i=0;i<src->scopes.count;i++) { // scopes
    scope_len = strlen(src->scopes.ptr[i]->name);
    memcpy(dest->data + offset, src->scopes.ptr[i]->name, scope_len + 1); // name
    //written = dest->data + offset;
    offset += scope_len + 1;
    //printf("after scope name, offset is %d (%s)\n", offset, written);
    memcpy(dest->data + offset, &src->scopes.ptr[i]->write, 1); // write
    offset += 1;
    //printf("after write flag, offset is %d (flag: %d)\n", offset, 0xFF & *(char*)(dest->data + offset - 1));
  }
  assert(offset == size);
  dest->ulen = size;
  dest->size = size;
}

static char *read_string(const char *source, uint32_t size, uint32_t *offset_ptr) {
  int saved_offset, len;
  char *dest = NULL;
  uint32_t offset = *offset_ptr;
  for(saved_offset = offset; offset < size; offset++) {
    //printf("offset: %d, size: %d, byte: 0x%x ('%c')\n", offset, size, source[offset], source[offset]);
    if(source[offset] == 0) {
      len = offset - saved_offset;
      dest = malloc(len + 1);
      if(! dest) {
        perror("Failed to allocate memory");
        return NULL;
      }
      memcpy(dest, source + saved_offset, len + 1);
      offset++;
      break;
    }
  }
  *offset_ptr = offset;
  return dest;
}

int unpack_authorization(struct rs_authorization *dest, DBT *src) {
  uint32_t size = src->size;
  size = src->size;
  uint32_t offset = 0;
  //printf("size is: %d\n", size);
  dest->username = read_string(src->data, size, &offset);
  if(! dest->username) {
    fprintf(stderr, "unpack: no username found\n");
    return 1;
  }
  //printf("unpack got username: %s (offset: %d)\n", dest->username, offset);
  dest->token = read_string(src->data, size, &offset);
  if(! dest->token) {
    fprintf(stderr, "unpack: no token found\n");
    return 1;
  }
  //printf("unpack got token: %s (offset: %d)\n", dest->token, offset);
  dest->scopes.count = *((uint32_t*) (src->data + offset));
  dest->scopes.ptr = malloc(sizeof(struct rs_scope*) * dest->scopes.count);
  if(dest->scopes.ptr == NULL) {
    perror("Failed to allocate memory");
    return -1;
  }
  offset += sizeof(uint32_t);
  //printf("unpack got scope count: %d (offset: %d)\n", dest->scopes.count, offset);
  //memset(dest->scopes.ptr, sizeof(struct rs_scope*) * dest->scopes.count, 0);
  char *scope_name;
  struct rs_scope *scope;
  int i;
  for(i=0;i<dest->scopes.count;i++) {
    scope_name = read_string(src->data, size, &offset);
    //printf("got scope string: %s (offset: %d)\n", scope_name, offset);
    scope = malloc(sizeof(struct rs_scope));
    if(! scope) {
      perror("Failed to allocate memory");
      return -1;
    }
    scope->name = scope_name;
    scope->write = ((char*)src->data)[offset++];
    //printf("got scope write flag: %d (offset: %d)\n", scope->write, offset);
    dest->scopes.ptr[i] = scope;
  }
  
  return 0;
}

int add_authorization(struct rs_authorization *auth) {
  uint32_t keylen = strlen(auth->username) + strlen(auth->token) + 1;
  char *key = malloc(keylen + 1);
  if(key == NULL) {
    perror("Failed to allocate memory");
    return -1;
  }
  sprintf(key, "%s|%s", auth->username, auth->token);
  DBT db_key, db_value;
  memset(&db_key, 0, sizeof(DBT));
  memset(&db_value, 0, sizeof(DBT));
  db_key.data = key;
  db_key.size = keylen;
  db_key.ulen = keylen + 1;
  pack_authorization(&db_value, auth);  if(db_value.ulen == 0) {
    fprintf(stderr, "value.ulen == 0\n");
    return -1;
  }
  int put_result = auth_db->put(auth_db, NULL, &db_key, &db_value, 0);
  fprintf(stderr, "PUT result: %d\n", put_result);
  return 0;
}

void print_authorization(struct rs_authorization *auth) {
  printf("{\n  \"user\": \"%s\",\n  \"token\": \"%s\",\n  \"scopes\": {", auth->username, auth->token);
  struct rs_scope *scope;
  int i;
  for(i=0;i<auth->scopes.count;i++) {
    if(i != 0) {
      printf(",");
    }
    scope = auth->scopes.ptr[i];
    printf("\n    \"%s\": \"%s\"", scope->name, scope->write ? "rw" : "r");
  }
  printf("\n  }\n}");
}

void print_authorizations(const char *username) {
  DBC *cursor;
  int cursor_result = auth_db->cursor(auth_db, NULL, &cursor, 0);
  switch(cursor_result) {
  case 0: break;
  case DB_REP_HANDLE_DEAD:
    fprintf(stderr, "DB->cursor() returned DB_REP_HANDLE_DEAD\n");
    exit(EXIT_FAILURE);
  case DB_REP_LOCKOUT:
    fprintf(stderr, "DB->cursor() returned DB_REP_LOCKOUT\n");
    exit(EXIT_FAILURE);
  case EINVAL:
    fprintf(stderr, "DB->cursor() returned EINVAL\n");
    exit(EXIT_FAILURE);
  default:
    fprintf(stderr, "DB->cursor() returned unknown error (%d)\n", cursor_result);
    exit(EXIT_FAILURE);
  }
  DBT db_key;
  DBT db_value;
  memset(&db_key, 0, sizeof(DBT));
  memset(&db_value, 0, sizeof(DBT));
  int get_result = cursor->get(cursor, &db_key, &db_value, DB_FIRST);
  if(get_result != 0) {
    char *msg;
    switch(get_result) {
    case DB_NOTFOUND: return;
    case DB_BUFFER_SMALL: msg = "DB_BUFFER_SMALL"; break;
    case DB_LOCK_DEADLOCK: msg = "DB_LOCK_DEADLOCK"; break;
    case DB_LOCK_NOTGRANTED: msg = "DB_LOCK_NOTGRANTED"; break;
    case DB_REP_HANDLE_DEAD: msg = "DB_REP_HANDLE_DEAD"; break;
    case DB_REP_LEASE_EXPIRED: msg = "DB_REP_LEASE_EXPIRED"; break;
    case DB_REP_LOCKOUT: msg = "DB_REP_LOCKOUT"; break;
    case DB_SECONDARY_BAD: msg = "DB_SECONDARY_BAD"; break;
    case EINVAL: msg = "EINVAL"; break;
    default: msg = "(unknown error)";
    }
    fprintf(stderr, "cursor->get() failed: %s (%d)\n", msg, get_result);
    abort();
  }
  struct rs_authorization *auth;
  auth = malloc(sizeof(struct rs_authorization));
  if(auth == NULL) {
    perror("Failed to allocate memory");
    return;
  }

  int i = 0;
  printf("[");
  do {
    unpack_authorization(auth, &db_value);
    if(username == NULL || strcmp(auth->username, username) == 0) {
      if(i != 0) {
        printf(", ");
      }
      print_authorization(auth);
      i++;
    }
    memset(&db_key, 0, sizeof(DBT));
  } while(cursor->get(cursor, &db_key, &db_value, DB_NEXT) != DB_NOTFOUND);
  printf("]\n");
}

void free_authorization(struct rs_authorization *auth) {
  if(auth->scopes.ptr) {
    uint32_t n = auth->scopes.count, i;
    for(i=0;i<n;i++) {
      if(auth->scopes.ptr[i]->name) {
        //fprintf(stderr, "Will free %s (%p)\n", auth->scopes.ptr[i]->name, auth->scopes.ptr[i]->name);
        free(auth->scopes.ptr[i]->name);
      }
      free(auth->scopes.ptr[i]);
    }
    free(auth->scopes.ptr);
  }
  if(auth->username) free(auth->username);
  if(auth->token) free(auth->token);
}

void list_authorizations(const char *username, void (*cb)(struct rs_authorization*, void*), void *ctx) {
  DBC *cursor;
  int cursor_result = auth_db->cursor(auth_db, NULL, &cursor, 0);
  switch(cursor_result) {
  case 0: break;
  case DB_REP_HANDLE_DEAD:
    fprintf(stderr, "DB->cursor() returned DB_REP_HANDLE_DEAD\n");
    exit(EXIT_FAILURE);
  case DB_REP_LOCKOUT:
    fprintf(stderr, "DB->cursor() returned DB_REP_LOCKOUT\n");
    exit(EXIT_FAILURE);
  case EINVAL:
    fprintf(stderr, "DB->cursor() returned EINVAL\n");
    exit(EXIT_FAILURE);
  default:
    fprintf(stderr, "DB->cursor() returned unknown error (%d)\n", cursor_result);
    exit(EXIT_FAILURE);
  }
  DBT db_key;
  DBT db_value;
  memset(&db_key, 0, sizeof(DBT));
  memset(&db_value, 0, sizeof(DBT));
  int get_result = cursor->get(cursor, &db_key, &db_value, DB_FIRST);
  if(get_result != 0) {
    char *msg;
    switch(get_result) {
    case DB_NOTFOUND: return;
    case DB_BUFFER_SMALL: msg = "DB_BUFFER_SMALL"; break;
    case DB_LOCK_DEADLOCK: msg = "DB_LOCK_DEADLOCK"; break;
    case DB_LOCK_NOTGRANTED: msg = "DB_LOCK_NOTGRANTED"; break;
    case DB_REP_HANDLE_DEAD: msg = "DB_REP_HANDLE_DEAD"; break;
    case DB_REP_LEASE_EXPIRED: msg = "DB_REP_LEASE_EXPIRED"; break;
    case DB_REP_LOCKOUT: msg = "DB_REP_LOCKOUT"; break;
    case DB_SECONDARY_BAD: msg = "DB_SECONDARY_BAD"; break;
    case EINVAL: msg = "EINVAL"; break;
    default: msg = "(unknown error)";
    }
    fprintf(stderr, "cursor->get() failed: %s (%d)\n", msg, get_result);
    abort();
  }
  struct rs_authorization *auth;
  auth = malloc(sizeof(struct rs_authorization));
  if(auth == NULL) {
    perror("Failed to allocate memory");
    return;
  }

  do {
    unpack_authorization(auth, &db_value);
    if(username == NULL || strcmp(auth->username, username) == 0) {
      cb(auth, ctx);
    }
    free_authorization(auth);
    memset(&db_key, 0, sizeof(DBT));
  } while(cursor->get(cursor, &db_key, &db_value, DB_NEXT) != DB_NOTFOUND);

  free(auth);
}
