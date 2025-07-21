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
GITHASH_S := $(shell git rev-parse --short HEAD)
GITVERS_S := $(shell git describe --tags | sed 's/^v//' | xargs)
_IGNORE_ := $(shell echo char GITHASH[] = \"$(GITHASH_S)\"\; > gitinfo.h)
_IGNORE_ := $(shell echo char GITVERS[] = \"$(GITVERS_S)\"\; >> gitinfo.h)

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
CFLAGS += "-I../deps/openssl/include"
endif
endif

all: ../pogocache

OBJS += sys.o cmds.o util.o buf.o stats.o conn.o args.o uring.o
OBJS += memcache.o postgres.o tls.o save.o parse.o lz4.o
OBJS += net.o xmalloc.o main.o pogocache.o resp.o http.o 
OBJS += hashmap.o

../pogocache: $(DEPS) $(OBJS)
	$(CC) $(CFLAGS) -o ../pogocache $(OBJS) $(CLIBS)

clean:
	rm -rf *.o *.dSYM *.out *.a gitinfo.h ../pogocache

distclean: 
	rm -rf ../deps/openssl/ ../deps/liburing/ ../deps/*.tar.gz
