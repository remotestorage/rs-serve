CFLAGS=${shell pkg-config libevent --cflags}
LDFLAGS=${shell pkg-config libevent --libs}

OBJECTS=src/main.o src/common.o src/storage.o src/auth.o

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

.PHONY: default all clean