
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "common/auth.h"

static void print_usage(char *progname) {
  fprintf(stderr, "Usage: %s <user> <token>\n", progname);
}

int main(int argc, char **argv) {
  if(argc < 3) {
    print_usage(argv[0]);
    exit(127);
  }
  open_authorizations("r+");
  struct rs_authorization auth;
  auth.username = argv[1];
  auth.token = argv[2];
  auth.scopes = NULL;
  remove_authorization(&auth);
  close_authorizations();
}
