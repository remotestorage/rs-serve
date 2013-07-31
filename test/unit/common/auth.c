
#define _GNU_SOURCE

#include <db.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/auth.h"

#define SUITE(desc) {                           \
    printf("\nSuite: %s\n", desc);              \
  }
#define TEST(desc, run) {                       \
    printf("  Test: %s ", desc);                \
    run();                                      \
    printf(" OK.\n\n");                         \
  }
#define FAIL_ASSERTION(a, b) {                      \
    printf("\nAssertion failed: %s != %s (%s:%d)\n", a, b, __FILE__, __LINE__);  \
    abort();                                        \
  }
#define ASSERT_S(a, b)                          \
  if(strcmp((a), (b)) == 0) {                   \
    printf(".");                                \
  } else {                                      \
    FAIL_ASSERTION(__STRING(a), __STRING(b));   \
  }
#define ASSERT_N(a, b)                          \
  if((a) == (b)) {                              \
    printf(".");                                \
  } else {                                      \
    FAIL_ASSERTION(__STRING(a), __STRING(b));   \
  }

// privates.
void pack_authorization(DBT *dest, struct rs_authorization *src);
int unpack_authorization(struct rs_authorization *dest, DBT *src);

void test_pack_unpack() {
  struct rs_scope contacts = { .name = "contacts", .write = 1 };
  struct rs_scope root = { .name = "", .write = 0 };
  struct rs_scope *scope_ptr[2] = { &contacts, &root };
  struct rs_authorization src_auth = {
    .username = "foo",
    .token = "bar",
    .scopes = {
      .count = 2,
      .ptr = scope_ptr
    }
  };
  DBT dbt;
  memset(&dbt, 0, sizeof(DBT));
  pack_authorization(&dbt, &src_auth);
  struct rs_authorization dest_auth;
  unpack_authorization(&dest_auth, &dbt);
  ASSERT_S(dest_auth.username, "foo");
  ASSERT_S(dest_auth.token, "bar");
  ASSERT_N(dest_auth.scopes.count, 2);
  ASSERT_S(dest_auth.scopes.ptr[0]->name, "contacts");
  ASSERT_N(dest_auth.scopes.ptr[0]->write, 1);
  ASSERT_S(dest_auth.scopes.ptr[1]->name, "");
  ASSERT_N(dest_auth.scopes.ptr[1]->write, 0);
}

void test_store_lookup() {
}

int main(int argc, char **argv) {
  SUITE("Authorization model");
  TEST("packing / unpacking", test_pack_unpack);
  TEST("store / lookup", test_store_lookup);
}

