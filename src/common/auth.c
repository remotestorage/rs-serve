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

#include <sys/stat.h>

#include "config.h"
#include "common/auth.h"

FILE *auth_fp = NULL;

static void free_scopes(struct rs_scope *scope) {
  struct rs_scope *tmp;
  while(scope) {
    tmp = scope;
    scope = scope->next;
    free(tmp);
  }
}

void open_authorizations(char *mode) {
  auth_fp = fopen(RS_AUTH_FILE_PATH, mode);
  if(auth_fp == NULL) {
    fprintf(stderr, "Failed to open authorization file \"%s\": %s\n",
            RS_AUTH_FILE_PATH, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void close_authorizations() {
  fclose(auth_fp);
  auth_fp = NULL;
}

int add_authorization(struct rs_authorization *auth) {
  fseek(auth_fp, 0, SEEK_END);
  fprintf(auth_fp, "%s|%s|", auth->username, auth->token);
  struct rs_scope *scope;
  for(scope = auth->scopes; scope != NULL; scope = scope->next) {
    fprintf(auth_fp, "%s:%c", scope->name, scope->write ? 'w' : 'r');
    if(scope->next) {
      fputc(',', auth_fp);
    }
  }
  fputc('\n', auth_fp);
  return 0;
}

struct rs_authorization *parse_authorization(struct rs_authorization *auth, char *buf) {
  char *saveptr;
  auth->username = strtok_r(buf, "|", &saveptr);
  auth->token = strtok_r(NULL, "|", &saveptr);
  auth->scopes = NULL;
  struct rs_scope *scope = NULL;
  for(;;) {
    char *scope_name;
    if(*saveptr == ':') {
      scope_name = "";
      saveptr++;
    } else {
      scope_name = strtok_r(NULL, ":", &saveptr);
    }
    if(scope_name) {
      scope = malloc(sizeof(struct rs_scope));
      if(scope == NULL) {
        perror("malloc() failed");
        free_scopes(auth->scopes);
        return NULL;
      }
      scope->name = scope_name;
      scope->write = (*saveptr == 'w' ? 1 : 0);
      while(*saveptr && *saveptr != ',') saveptr++;
      if(*saveptr) saveptr++;
      scope->next = auth->scopes;
      auth->scopes = scope;
    } else {
      return auth;
    }
  }
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

void print_authorization(struct rs_authorization *auth) {
  printf("User: %s, Token: %s\n", auth->username, auth->token);
  struct rs_scope *scope;
  for(scope = auth->scopes; scope != NULL; scope = scope->next) {
    printf(" - %s (%s)\n", *scope->name == 0 ? "(root)" : scope->name, scope->write ? "read-write" : "read-only");
  }
}

void list_authorizations() {
  struct rs_authorization auth;
  char *buf = NULL;
  size_t buflen = 0;
  while(read_authorization(&auth, &buf, &buflen) != NULL) {
    print_authorization(&auth);
    free_scopes(auth.scopes);
  }
  if(buf) {
    free(buf);
  }
}

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

struct rs_authorization *lookup_authorization(const char *username, const char *token) {
  int uname_len = strlen(username);
  int token_len = strlen(token);
  char *buf = NULL;
  size_t buflen = 0, linelen;
  rewind(auth_fp);
  while((linelen = getline(&buf, &buflen, auth_fp)) != -1) {
    if(strncmp(buf, username, uname_len) == 0 &&
       buf[uname_len] == '|' &&
       strncmp(buf + uname_len + 1, token, token_len) == 0 &&
       buf[uname_len + 1 + token_len] == '|') {
      struct rs_authorization *auth = malloc(sizeof(struct rs_authorization));
      if(parse_authorization(auth, buf) == NULL) {
        free(auth);
        return NULL;
      } else {
        return auth;
      }
    }
  }
  return NULL;
}
