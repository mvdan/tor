#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LINE_LENGTH 256
#define BASE 10

/*
 *
 * Assumptions:
 *
 * - We'll never have a line larger than 255 chars
 * - <n1>[,<n2>] will not fill the entire line
 * - <n1> and <n2> will be valid numbers
 * - <n2> >= <n1>
 * - The diff will not have any other mistakes/errors/glitches
 * - ...
 *
 */

int main(int argc, char* argv[]) {

	if (argc <= 2) return 1;

	FILE *fd1 = fopen(argv[1], "r");
	FILE *fd2 = fopen(argv[2], "w+");
	char diff_line[LINE_LENGTH];
	long orig_cur = 0;
	char orig_line[LINE_LENGTH];

	while (fgets(diff_line, sizeof(diff_line), stdin)) {

		char *endptr1, *endptr2;
		long l1 = strtol(diff_line, &endptr1, BASE);
		long l2;

		if (endptr1 == diff_line) {
			printf("Missing range: %s", diff_line);
			continue;

		} else if (*endptr1 == ',') {
			l2 = strtol(endptr1+1, &endptr2, BASE);
			if (endptr2 == endptr1+1) {
				printf("Missing range: %s", diff_line);
				continue;
			}

		} else {
			// We'll take <n1> as <n1>,<n1> for simplicity
			endptr2 = endptr1;
			l2 = l1;
		}

		if (strcmp(endptr2, "d\n") == 0) {

			// get lines up to line to delete
			while (orig_cur < l1) {
				fgets(orig_line, sizeof(orig_line), fd1);
				fputs(orig_line, fd2);
				orig_cur++;
			}

			// discards <n2> - <n1> lines
			long i;
			for (i = l1; i <= l2; ++i) {
				fgets(orig_line, sizeof(orig_line), fd1);
			}

		} else if (*endptr2 == ',') {
			printf("Too many range arguments: %s", diff_line);

		} else if (*endptr2 == '\n') {
			printf("Missing command: %s", diff_line);

		} else {
			printf("Unimplemented command: %s", diff_line);
		}
	}

	// write the rest of the lines
	while (fgets(orig_line, sizeof(orig_line), fd1)) {
		fputs(orig_line, fd2);
		orig_cur++;
	}

	fclose(fd1);
	fclose(fd2);

	return 0;
}
