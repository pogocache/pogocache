ifeq ($(CC), emcc)
NOURING := 1
NOOPENSSL := 1
CFLAGS := -Wall -Wextra -Werror -O3 $(CFLAGS)
LDFLAGS := -sASSERTIONS=1 -sINITIAL_MEMORY=64MB \
    -sEXPORTED_FUNCTIONS='["_malloc","_free","_exec_command","_add_time","_main"]' \
    -sEXPORTED_RUNTIME_METHODS='[ \
        "ccall","cwrap","stringToUTF8","UTF8ToString" \
    ]' \
    $(LDFLAGS)

CNODEFFLAGS=1
OUTEXT=.html
endif

ifndef CNODEFFLAGS
ifdef CCSANI
SFLAGS := -O0 -g3 -Wall -Wextra -Werror -fsanitize=address 
SFLAGS += -fno-omit-frame-pointer -DCCSANI
CFLAGS := $(SFLAGS) $(CFLAGS)
else
CFLAGS := -Wall -Wextra -Werror -g -O3 -flto=auto $(CFLAGS)
endif
endif

CLIBS += -lm

UNAME_S := $(shell uname -s)

ifndef NOGITINFOGEN
GITHASH_S := $(shell git rev-parse --short HEAD)
GITVERS_S := $(shell git describe --tags | sed 's/^v//' | xargs)
_IGNORE_ := $(shell echo char GITHASH[] = \"$(GITHASH_S)\"\; > gitinfo.h)
_IGNORE_ := $(shell echo char GITVERS[] = \"$(GITVERS_S)\"\; >> gitinfo.h)
endif

ifdef NOMIMALLOC
CFLAGS += -DNOMIMALLOC
else
../deps/mimalloc/out/release/libmimalloc.a:
	../deps/build-mimalloc.sh
DEPS += ../deps/mimalloc/out/release/libmimalloc.a
CLIBS += ../deps/mimalloc/out/release/libmimalloc.a
endif

ifdef NOJEMALLOC
CFLAGS += -DNOJEMALLOC
else
../deps/jemalloc/lib/libjemalloc.a:
	../deps/build-jemalloc.sh
DEPS += ../deps/jemalloc/lib/libjemalloc.a
CLIBS += ../deps/jemalloc/lib/libjemalloc.a
endif


ifeq ($(UNAME_S), Linux)
ifdef NOURING
CFLAGS += -DNOURING
else
../deps/liburing/src/liburing.a:
	../deps/build-uring.sh
DEPS += ../deps/liburing/src/liburing.a
CLIBS += ../deps/liburing/src/liburing.a
endif
endif

ifdef NOOPENSSL
CFLAGS += -DNOOPENSSL
else
../deps/openssl/libssl.a:
	../deps/build-openssl.sh
DEPS += ../deps/openssl/libssl.a
CLIBS += ../deps/openssl/libssl.a ../deps/openssl/libcrypto.a
ifdef USEFULLOPENSSLHEADER
CFLAGS += -I../deps/openssl/include
endif
endif

ifndef NOSTATIC
LIBC := $(shell ldd --version 2>&1 | head -n1 | grep -o musl || true)
ifeq ($(LIBC),musl)
CFLAGS += -static
endif
endif

all: ../pogocache

OBJS += sys.o cmds.o util.o buf.o stats.o conn.o args.o uring.o
OBJS += memcache.o postgres.o tls.o save.o parse.o lz4.o
OBJS += net.o xmalloc.o main.o pogocache.o resp.o http.o 
OBJS += hashmap.o monitor.o

../pogocache: $(DEPS) $(OBJS)
	$(CC) $(CFLAGS) -o ../pogocache$(OUTEXT) $(LDFLAGS) $(OBJS) $(CLIBS)

clean:
	rm -rf *.o *.dSYM *.out *.a gitinfo.h ../pogocache

distclean: 
	rm -rf ../deps/openssl/ ../deps/liburing/ ../deps/mimalloc/ ../deps/*.tar.gz

gitinfo:
