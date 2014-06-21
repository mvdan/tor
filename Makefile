TORDIR ?= $(HOME)/git/tor

TOR_INC := src/common src/or src/config src/ext .
TOR_INC := $(foreach d,$(TOR_INC),-I$(TORDIR)/$(d))

TOR_LIB := -lpthread -lseccomp -lm $(TORDIR)/src/or/tor

CFLAGS := -O2 -Wall -Wextra -g $(CFLAGS) $(TOR_INC)

all: client server

consdiff.o: consdiff.c
	$(CC) $(CFLAGS) -c consdiff.c

client: consdiff.o client.c
	$(CC) $(CFLAGS) $(TOR_LIB) consdiff.o client.c -o client

c_test1: client test/c_test1_orig test/c_test1_diff
	./client test/c_test1_orig test/c_test1_out < test/c_test1_diff
	diff test/c_test1_expected test/c_test1_out

server: consdiff.o server.c
	$(CC) $(CFLAGS) $(TOR_LIB) consdiff.o server.c -o server

s_test1: server test/s_test1_orig test/s_test1_new
	./server test/s_test1_orig test/s_test1_new > test/s_test1_out
	diff test/s_test1_expected test/s_test1_out

test: c_test1 s_test1

clean:
	rm -f client server *.o test/*_out

.PHONY: all clean tests c_test1 s_test1
