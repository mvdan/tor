TORDIR ?= $(HOME)/git/tor

TOR_INC := src/common src/or src/config src/ext src/test .
TOR_INC := $(foreach d,$(TOR_INC),-I$(TORDIR)/$(d))

TOR_LIB := -lpthread -lseccomp -lm $(TORDIR)/src/or/tor

TEST_LIB := $(TORDIR)/src/test/test

CFLAGS := -O2 -Wall -Wextra -g $(CFLAGS) $(TOR_INC)

all: client server test

consdiff.o: consdiff.c
	$(CC) $(CFLAGS) -c consdiff.c

client: consdiff.o client.c
	$(CC) $(CFLAGS) $(TOR_LIB) consdiff.o client.c -o client

server: consdiff.o server.c
	$(CC) $(CFLAGS) $(TOR_LIB) consdiff.o server.c -o server

test: consdiff.c test_consdiff.c
	$(CC) $(CFLAGS) $(TEST_LIB) test_consdiff.c -o test

clean:
	rm -f client server test *.o

.PHONY: all clean
