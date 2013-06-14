CFLAGS=${shell pkg-config libevent_openssl --cflags} -ggdb -Wall --std=c99 $(INCLUDES)
LDFLAGS=${shell pkg-config libevent_openssl --libs} ${shell pkg-config libssl --libs} -lmagic -lattr
INCLUDES=-Isrc -Ilib/evhtp/ -Ilib/evhtp/htparse -Ilib/evhtp/evthr -Ilib/evhtp/oniguruma/

BASE_OBJECTS=src/config.o
COMMON_OBJECTS=src/common/log.o src/common/process.o src/common/request_response.o src/common/user.o src/common/auth.o
HANDLER_OBJECTS=src/handler/dispatch.o src/handler/storage.o src/handler/response.o src/handler/request.o src/handler/auth.o
PROCESS_OBJECTS=src/process/main.o src/process/storage.o
OBJECTS=$(BASE_OBJECTS) $(COMMON_OBJECTS) $(PROCESS_OBJECTS) $(HANDLER_OBJECTS)

STATIC_LIBS=lib/evhtp/build/libevhtp.a

SUBMODULES=lib/evhtp/

default: all

all: rs-serve tools

rs-serve: $(STATIC_LIBS) $(OBJECTS)
	@echo "[LD] $@"
	@$(CC) -o $@ $(OBJECTS) $(STATIC_LIBS) $(LDFLAGS)

%.o: %.c
	@echo "[CC] ${shell echo $@ | sed 's/src\///' | sed 's/\.o//'}"
	@$(CC) -c $< -o $@ $(CFLAGS)

tools: tools/add-token tools/remove-token tools/list-tokens

.PHONY: tools

tools/add-token: src/tools/add-token.o src/common/auth.o
	@echo "[LD] $@"
	@$(CC) -o $@ src/tools/add-token.o src/common/auth.o

tools/remove-token: src/tools/remove-token.o src/common/auth.o
	@echo "[LD] $@"
	@$(CC) -o $@ src/tools/remove-token.o src/common/auth.o

tools/list-tokens: src/tools/list-tokens.o src/common/auth.o
	@echo "[LD] $@"
	@$(CC) -o $@ src/tools/list-tokens.o src/common/auth.o

clean:
	@echo "[CLEAN]"
	@rm -f rs-serve tools/add-token tools/remove-token tools/list-tokens
	@find src/ -name '*.o' -exec rm '{}' ';'
	@find -name '*~' -exec rm '{}' ';'
	@find -name '*.swp' -exec rm '{}' ';'

install: rs-serve
	@echo "[INSTALL] rs-serve"
	@install -s rs-serve /usr/bin
	@echo "[INSTALL] /etc/init.d/rs-serve"
	@install -m 0755 init-script.sh /etc/init.d/rs-serve
	@echo "[INSTALL] /etc/default/rs-serve"
	@install init-script-defaults /etc/default/rs-serve
ifeq (${shell type update-rc.d >/dev/null 2>&1 ; echo $$?}, 0)
	@echo "[UPDATE-RC.D]"
	@update-rc.d rs-serve defaults
else
	@echo "(can't update /etc/rcN.d, no idea how that works on your system)"
endif

test: all
	@test/run.sh

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

.PHONY: submodules clean-submodules
