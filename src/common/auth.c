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

void print_db_error(const DB_ENV *env, const char *errpfx, const char *msg) {
  fprintf(stderr, "DB ERROR: %s: %s\n", errpfx, msg);
}

void open_authorizations(char *mode) {
  uint32_t flags;

  if(db_create(&auth_db, NULL, 0) != 0) {
    fprintf(stderr, "db_create() failed\n");
    abort(); // FIXME!
  }

  flags = DB_CREATE;

  /* if(mode[1] != 'w') { */
  /*   flags |= DB_RDONLY; */
  /* } */
  
  if(auth_db->open(auth_db, NULL, RS_AUTH_DB_PATH, NULL, DB_HASH, flags, 0) != 0) {
    fprintf(stderr, "auth_db->open() failed\n");
    abort(); // FIXME!
  }

  auth_db->set_errcall(auth_db, print_db_error);
  // TODO: set_errcall etc.
}

void close_authorizations() {
  auth_db->close(auth_db, 0);
  auth_db = NULL;
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
}

void print_authorization(struct rs_authorization *auth) {
  printf("User: %s, Token: %s\n", auth->username, auth->token);
  struct rs_scope *scope;
  int i;
  for(i=0;i<auth->scopes.count;i++) {
    scope = auth->scopes.ptr[i];
    printf(" - %s (%s)\n", *scope->name == 0 ? "(root)" : scope->name, scope->write ? "read-write" : "read-only");
  }
}

void list_authorizations() {
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
    case DB_NOTFOUND: msg = "DB_NOTFOUND"; break;
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
    print_authorization(auth);
    memset(&db_key, 0, sizeof(DBT));
  } while(cursor->get(cursor, &db_key, &db_value, DB_NEXT) != DB_NOTFOUND);


  /* struct rs_authorization auth; */
  /* char *buf = NULL; */
  /* size_t buflen = 0; */
  /* while(read_authorization(&auth, &buf, &buflen) != NULL) { */
  /*   print_authorization(&auth); */
  /*   //free_scopes(auth.scopes); */
  /* } */
  /* if(buf) { */
  /*   free(buf); */
  /* } */
}

// below here naive, flat-file based implementation.

FILE *auth_fp = NULL;

static void free_scopes(struct rs_scope *scope) {
  /* struct rs_scope *tmp; */
  /* while(scope) { */
  /*   tmp = scope; */
  /*   scope = scope->next; */
  /*   free(tmp); */
  /* } */
}

/* void open_authorizations(char *mode) { */
/*   auth_fp = fopen(RS_AUTH_FILE_PATH, mode); */
/*   if(auth_fp == NULL) { */
/*     fprintf(stderr, "Failed to open authorization file \"%s\": %s\n", */
/*             RS_AUTH_FILE_PATH, strerror(errno)); */
/*     exit(EXIT_FAILURE); */
/*   } */
/* } */

/* void close_authorizations() { */
/*   fclose(auth_fp); */
/*   auth_fp = NULL; */
/* } */

/* int add_authorization(struct rs_authorization *auth) { */
/*   fseek(auth_fp, 0, SEEK_END); */
/*   fprintf(auth_fp, "%s|%s|", auth->username, auth->token); */
/*   struct rs_scope *scope; */
/*   for(scope = auth->scopes; scope != NULL; scope = scope->next) { */
/*     fprintf(auth_fp, "%s:%c", scope->name, scope->write ? 'w' : 'r'); */
/*     if(scope->next) { */
/*       fputc(',', auth_fp); */
/*     } */
/*   } */
/*   fputc('\n', auth_fp); */
/*   return 0; */
/* } */

struct rs_authorization *parse_authorization(struct rs_authorization *auth, char *buf) {
  /* char *saveptr; */
  /* auth->username = strtok_r(buf, "|", &saveptr); */
  /* auth->token = strtok_r(NULL, "|", &saveptr); */
  /* auth->scopes = NULL; */
  /* struct rs_scope *scope = NULL; */
  /* for(;;) { */
  /*   char *scope_name; */
  /*   if(*saveptr == ':') { */
  /*     scope_name = ""; */
  /*     saveptr++; */
  /*   } else { */
  /*     scope_name = strtok_r(NULL, ":", &saveptr); */
  /*   } */
  /*   if(scope_name) { */
  /*     scope = malloc(sizeof(struct rs_scope)); */
  /*     if(scope == NULL) { */
  /*       perror("malloc() failed"); */
  /*       free_scopes(auth->scopes); */
  /*       return NULL; */
  /*     } */
  /*     scope->name = scope_name; */
  /*     scope->write = (*saveptr == 'w' ? 1 : 0); */
  /*     while(*saveptr && *saveptr != ',') saveptr++; */
  /*     if(*saveptr) saveptr++; */
  /*     scope->next = auth->scopes; */
  /*     auth->scopes = scope; */
  /*   } else { */
  /*     return auth; */
  /*   } */
  /* } */
}

struct rs_authorization *read_authorization(struct rs_authorization *auth, char **buf, size_t *buflen) {
  size_t linelen;
  for(;;) {
    if((linelen = getline(buf, buflen, auth_fp)) == -1) {
      if(! feof(auth_fp)) {
        perror("getline() failed");
      }
      return NULL;
    }
    if(**buf != '\n') { // empty line
      break;
    }
  }
  return parse_authorization(auth, *buf);
}

/* void list_authorizations() { */
/*   struct rs_authorization auth; */
/*   char *buf = NULL; */
/*   size_t buflen = 0; */
/*   while(read_authorization(&auth, &buf, &buflen) != NULL) { */
/*     print_authorization(&auth); */
/*     //free_scopes(auth.scopes); */
/*   } */
/*   if(buf) { */
/*     free(buf); */
/*   } */
/* } */

int remove_authorization(struct rs_authorization *auth) {
  int uname_len = strlen(auth->username);
  int token_len = strlen(auth->token);
  char *buf = NULL;
  size_t buflen = 0, linelen;
  while((linelen = getline(&buf, &buflen, auth_fp)) != -1) {
    if(strncmp(buf, auth->username, uname_len) == 0 &&
       buf[uname_len] == '|' &&
       strncmp(buf + uname_len + 1, auth->token, token_len) == 0 &&
       buf[uname_len + 1 + token_len] == '|') {
      fseek(auth_fp, ftell(auth_fp) - linelen, SEEK_SET);
      int j;
      for(j=0;j<linelen;j++) {
        fwrite("\n", 1, 1, auth_fp);
      }
      fflush(auth_fp);
      return 0;
    }
  }
  return -1;
}

/* struct rs_authorization *lookup_authorization(const char *username, const char *token) { */
/*   int uname_len = strlen(username); */
/*   int token_len = strlen(token); */
/*   char *buf = NULL; */
/*   size_t buflen = 0, linelen; */
/*   rewind(auth_fp); */
/*   while((linelen = getline(&buf, &buflen, auth_fp)) != -1) { */
/*     if(strncmp(buf, username, uname_len) == 0 && */
/*        buf[uname_len] == '|' && */
/*        strncmp(buf + uname_len + 1, token, token_len) == 0 && */
/*        buf[uname_len + 1 + token_len] == '|') { */
/*       struct rs_authorization *auth = malloc(sizeof(struct rs_authorization)); */
/*       if(parse_authorization(auth, buf) == NULL) { */
/*         free(auth); */
/*         return NULL; */
/*       } else { */
/*         return auth; */
/*       } */
/*     } */
/*   } */
/*   return NULL; */
/* } */
