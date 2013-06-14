
#ifndef RS_COMMON_AUTH_H
#define RS_COMMON_AUTH_H

#include <sys/types.h>

struct rs_scope {
  char *name;
  int len;
  char write;

  struct rs_scope *next;
};

struct rs_authorization {
  const char *username;
  const char *token;
  struct rs_scope *scopes;
};

void open_authorizations(char *mode);
void close_authorizations();
off_t find_auth_line(const char *username, const char *token);
int add_authorization(struct rs_authorization *auth);
void remove_authorization(struct rs_authorization *auth);
void list_authorizations();
struct rs_authorization *lookup_authorization(const char *username, const char *token);

#endif

