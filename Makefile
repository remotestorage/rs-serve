CFLAGS=${shell pkg-config libevent --cflags} -ggdb -Wall
LDFLAGS=${shell pkg-config libevent --libs} -lmagic

OBJECTS=src/main.o src/common.o src/storage.o src/auth.o src/handler.o src/webfinger.o src/config.o

default: all

all: rs-serve

rs-serve: $(OBJECTS)
	@echo "[LD] $@"
	@$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.c
	@echo "[CC] $@"
	@$(CC) -c $< -o $@ $(CFLAGS)

clean:
	@echo "[CLEAN]"
	@rm -f rs-serve
	@rm -f $(OBJECTS)
	@rm -f *~ src/*~

leakcheck: all
	scripts/leakcheck.sh

.PHONY: default all clean leakcheck
