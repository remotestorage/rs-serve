CFLAGS=${shell pkg-config libevent --cflags} -ggdb -Wall -Werror --std=c99 -pedantic
LDFLAGS=${shell pkg-config libevent --libs} -lmagic

OBJECTS=src/main.o src/common.o src/storage.o src/auth.o src/handler.o src/webfinger.o src/config.o src/ui.o src/auth_struct.o src/session.o src/csrf_protection.o src/trie.o src/auth_store.o

default: all

all: rs-serve

rs-serve: $(OBJECTS)
	@echo "[LD] $@"
	@$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.c
	@echo "[CC] ${shell echo $@ | sed 's/src\///' | sed 's/\.o//'}"
	@$(CC) -c $< -o $@ $(CFLAGS)

clean:
	@echo "[CLEAN]"
	@rm -f rs-serve
	@rm -f $(OBJECTS)
	@find -name '*~' -exec rm '{}' ';'
	@find -name '*.swp' -exec rm '{}' ';'

test: all
	@test/run.sh

leakcheck: all
	scripts/leakcheck.sh

limit_check: all
	scripts/limitcheck.sh 5000

.PHONY: default all clean leakcheck
