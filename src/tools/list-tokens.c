
#include <fcntl.h>
#include "common/auth.h"

int main() {
  open_authorizations("r");
  list_authorizations();
  close_authorizations();
}
