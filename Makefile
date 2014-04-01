all: tor_sample tests

tor_sample: tor_sample.c
	$(CC) tor_sample.c -std=c99 -o tor_sample

test1_out: tor_sample test1_in test1_diff
	./tor_sample test1_in test1_out < test1_diff
	diff test1_expected test1_out

tests: test1_out

clean:
	rm -f tor_sample *_out

.PHONY: all clean tests
