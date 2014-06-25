#include "container.h"
#include "consdiff.h"

int
main(int argc, char **argv)
{
  smartlist_t *cons1 = smartlist_new();
  smartlist_t *diff = smartlist_new();
  if (argc != 3) {
    fprintf(stderr, "Usage: %s file diff\n", argv[0]);
    return 1;
  }
  char *cons1_str = read_file_to_str(argv[1], 0, NULL);
  char *diff_str = read_file_to_str(argv[2], 0, NULL);

  tor_split_lines(cons1, cons1_str, strlen(cons1_str));
  tor_split_lines(diff, diff_str, strlen(diff_str));
  smartlist_t *cons2 = consdiff_apply_diff(cons1, diff);
  if (cons2 == NULL) {
    fprintf(stderr, "Something went wrong.\n");
  } else {
    SMARTLIST_FOREACH_BEGIN(cons2, char*, line) {
      printf("%s\n", line);
      tor_free(line);
    } SMARTLIST_FOREACH_END(line);
    smartlist_free(cons2);
  }

  tor_free(cons1_str);
  tor_free(diff_str);

  smartlist_free(cons1);
  smartlist_free(diff);

  return 0;
}

// vim: et sw=2
