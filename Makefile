TOR_INC := src/common src/or src/config src/ext .
TOR_INC := $(foreach d,$(TOR_INC),-I../tor/$(d))

TOR_LIB := -lpthread -lseccomp -lm ../tor/src/or/tor

CFLAGS := -g $(CFLAGS) $(TOR_INC) $(TOR_LIB)

all: client server tests

client: client.c
	$(CC) $(CFLAGS) client.c -o client

c_test1: client c_test1_in c_test1_diff
	./client c_test1_in c_test1_out < c_test1_diff
	diff c_test1_expected c_test1_out

server: server.c
	$(CC) $(CFLAGS) server.c -o server

tests: c_test1

clean:
	rm -f client server *_out

.PHONY: all clean tests
