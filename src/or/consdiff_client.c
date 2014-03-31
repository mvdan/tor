#include <stdlib.h>
#include <stdio.h>

#define LINE_LENGTH 256
#define BASE 10

/*
 *
 * Assumptions:
 *
 * - We'll never have a line larger than 255 chars
 * - <n1> will not fill the entire line
 * - <n1> will be a valid number
 * - The diff will not have mistakes/errors/glitches
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
	char *endptr;

	while (fgets(diff_line, sizeof(diff_line), stdin)) {

		long l1 = strtol(diff_line, &endptr, BASE);
		if (endptr == diff_line) {
			printf("Missing range: %s", diff_line);

		} else if (*endptr == 'd') {

			// get lines up to line to delete
			while (orig_cur < l1) {
				fgets(orig_line, sizeof(orig_line), fd1);
				fputs(orig_line, fd2);
				orig_cur++;
			}

			// discard a single line
			fgets(orig_line, sizeof(orig_line), fd1);

		} else if (*endptr == ',') {
			printf("Too many range arguments: %s", diff_line);

		} else if (*endptr == '\n') {
			printf("Missing command: %s", diff_line);

		} else {
			printf("Unimplemented command: %s", diff_line);
		}
	}
	while (fgets(orig_line, sizeof(orig_line), fd1)) {
		fputs(orig_line, fd2);
		orig_cur++;
	}

	fclose(fd1);
	fclose(fd2);

	return 0;
}
