CFLAGS=${shell pkg-config libevent_openssl --cflags} -ggdb -Wall --std=c99 $(INCLUDES)
LDFLAGS=${shell pkg-config libevent_openssl --libs} ${shell pkg-config libssl --libs} -lmagic -lattr -lpthread -ldb
INCLUDES=-Isrc -Ilib/evhtp/ -Ilib/evhtp/htparse -Ilib/evhtp/evthr -Ilib/evhtp/oniguruma/

TOOLS = tools/add-token tools/remove-token tools/list-tokens tools/lookup-token
TOOLS_LDFLAGS = -ldb

BASE_OBJECTS=src/config.o
COMMON_OBJECTS=src/common/log.o src/common/user.o src/common/auth.o src/common/json.o src/common/attributes.o
HANDLER_OBJECTS=src/handler/storage.o src/handler/auth.o src/handler/webfinger.o src/handler/dispatch.o
PROCESS_OBJECTS=src/process/main.o
OBJECTS=$(BASE_OBJECTS) $(COMMON_OBJECTS) $(PROCESS_OBJECTS) $(HANDLER_OBJECTS)
HEADERS=src/rs-serve.h src/config.h src/common/auth.h src/common/json.h src/common/log.h src/common/user.h src/handler/auth.h src/handler/dispatch.h src/handler/storage.h src/handler/webfinger.h

STATIC_LIBS=lib/evhtp/build/libevhtp.a

SUBMODULES=lib/evhtp/

TESTS=test/unit/common/auth

default: all

all: rs-serve tools tests

rs-serve: $(STATIC_LIBS) $(OBJECTS) $(HEADERS)
	@echo "[LD] $@"
	@$(CC) -o $@ $(OBJECTS) $(STATIC_LIBS) $(LDFLAGS)

%.o: %.c
	@echo "[CC] ${shell echo $@ | sed 's/src\///' | sed 's/\.o//'}"
	@$(CC) -c $< -o $@ $(CFLAGS)

tools: $(TOOLS)

.PHONY: tools

tools/%: src/tools/%.o src/common/auth.o $(HEADERS)
	@echo "[LD] $@"
	@$(CC) -o $@ $< src/common/auth.o $(TOOLS_LDFLAGS)

clean:
	@echo "[CLEAN]"
	@rm -f rs-serve $(TOOLS) $(TESTS)
	@find src/ -name '*.o' -exec rm '{}' ';'
	@find -name '*~' -exec rm '{}' ';'
	@find -name '*.swp' -exec rm '{}' ';'

notes:
	@grep -rE 'TODO:|FIXME:' src/

.PHONY: notes

install: all
# install rs-esrve
	@echo "[INSTALL] rs-serve"
	@install -s rs-serve /usr/bin
# install tools
	@echo "[INSTALL] rs-list-tokens"
	@install -s tools/list-tokens /usr/bin/rs-list-tokens
	@echo "[INSTALL] rs-lookup-token"
	@install -s tools/lookup-token /usr/bin/rs-lookup-token
	@echo "[INSTALL] rs-add-token"
	@install -s tools/add-token /usr/bin/rs-add-token
	@echo "[INSTALL] rs-remove-token"
	@install -s tools/remove-token /usr/bin/rs-remove-token
# create working dir
	@echo "[MKDIR] /var/lib/rs-serve/"
	@mkdir -p /var/lib/rs-serve
# create 'authorizations' dir (root of the BDB environment used to store authorization tokens)
	@echo "[MKDIR] /var/lib/rs-serve/authorizations/"
	@mkdir /var/lib/rs-serve/authorizations
# install init script
	@echo "[INSTALL] /etc/init.d/rs-serve"
	@install -m 0755 init-script.sh /etc/init.d/rs-serve
ifeq (${shell test -f /etc/default/rs-serve >/dev/null 2>&1 ; echo $$?}, 0)
	@echo "[EXISTS] /etc/default/rs-serve"
else
	@echo "[INSTALL] /etc/default/rs-serve"
	@install init-script-defaults /etc/default/rs-serve
endif
ifeq (${shell type update-rc.d >/dev/null 2>&1 ; echo $$?}, 0)
	@echo "[UPDATE-RC.D]"
	@update-rc.d rs-serve defaults
else
	@echo "(can't update /etc/rcN.d, no idea how that works on your system)"
endif

tests: $(TESTS)

test/unit/common/auth: test/unit/common/auth.o src/common/auth.o
	@echo "[LD] test/unit/common/auth"
	@$(CC) $< -o $@ $(LDFLAGS) src/common/auth.o
	@echo "[TEST] common/auth"
	@test/unit/common/auth

.PHONY: $(TESTS)

leakcheck: all
	scripts/leakcheck.sh

limit_check: all
	scripts/limitcheck.sh 5000

.PHONY: default all clean leakcheck

## DEPENDENT LIBS

lib/evhtp/build/libevhtp.a: lib/evhtp/
	@echo "[DEPS] libevhtp"
	@cd lib/evhtp/build && cmake .. && make

## SUBMODULES

submodules: $(SUBMODULES)

clean-submodules:
	@echo "[CLEAN SUBMODULES]"
	@rm -rf $(SUBMODULES)

$(SUBMODULES):
	@echo "[SUBMODULE] $@"
	@git submodule update --init $@

.PHONY: submodules clean-submodules $(SUBMODULES)

## JAVASCRIPT BINDINGS

bindings: auth/bindings/auth.cc src/common/auth.c $(HEADERS)
	@echo "[BINDINGS] auth"
	@cd auth/bindings && node-gyp configure && node-gyp build
	@echo "[DEPS] unixlib"
	@cd auth/backend/deps/unixlib && node-gyp configure && node-gyp build

.PHONY: bindings
