#include "container.h"
#include "consdiff.h"

int
main(int argc, char **argv)
{
  smartlist_t *orig = smartlist_new();
  smartlist_t *new = smartlist_new();
  if (argc != 3) {
    fprintf(stderr, "Usage: %s file1 file2\n", argv[0]);
    return 1;
  }
  char *cons1 = read_file_to_str(argv[1], 0, NULL);
  char *cons2 = read_file_to_str(argv[2], 0, NULL);

  tor_split_lines(orig, cons1, strlen(cons1));
  tor_split_lines(new, cons2, strlen(cons2));
  smartlist_t *diff = consdiff_gen_diff(orig, new);
  if (diff == NULL) {
    fprintf(stderr, "Something went wrong.\n");
  } else {
    SMARTLIST_FOREACH_BEGIN(diff, char*, line) {
      printf("%s\n", line);
      tor_free(line);
    } SMARTLIST_FOREACH_END(line);
    smartlist_free(diff);
  }

  tor_free(cons1);
  tor_free(cons2);

  smartlist_free(orig);
  smartlist_free(new);

  return 0;
}

// vim: et sw=2
