#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"

#define RANGE_BASE 10

smartlist_t *
apply_diff(smartlist_t *cons1, smartlist_t *diff)
{
  int i, diff_len = smartlist_len(diff);
  int j = smartlist_len(cons1);
  smartlist_t *cons2 = smartlist_new();

  for (i=0; i<diff_len; ++i) {
    const char *diff_line = smartlist_get(diff, i);
    /*fprintf(stderr, "%s\n", (char*)smartlist_get(diff, i));*/
    char *endptr1, *endptr2;
    int start, end;
    start = (int)strtol(diff_line, &endptr1, RANGE_BASE);

    /* Missing range start. */
    if (endptr1 == diff_line) goto error_cleanup;

    /* Two-item range */
    if (*endptr1 == ',') {
        end = (int)strtol(endptr1+1, &endptr2, RANGE_BASE);
        /* Missing range end. */
        if (endptr2 == endptr1+1) goto error_cleanup;
        /* Incoherent range. */
        if (end <= start) goto error_cleanup;

    /* We'll take <n1> as <n1>,<n1> for simplicity. */
    } else {
        endptr2 = endptr1;
        end = start;
    }

    /* Action is longer than one char. */
    if (*(endptr2+1) != '\0') goto error_cleanup;
    char action = *endptr2;

    for (; j > end; --j) {
      char *cons_line = smartlist_get(cons1, j-1);
      smartlist_add(cons2, tor_strdup(cons_line));
    }

    /* Ignore removed lines. */
    if (action == 'c' || action == 'd') {
      while (--j >= start) ;
    }

    /* Add new lines. */
    /* In reverse order, since it will all be reversed at the end. */
    if (action == 'a' || action == 'c') {
      int added_end = i++;
      const char *added_line = smartlist_get(diff, i);
      while (strcmp(added_line, "."))
        added_line = smartlist_get(diff, ++i);
      int added_i = i-1;
      while (added_i > added_end) {
        added_line = smartlist_get(diff, added_i--);
        smartlist_add(cons2, tor_strdup(added_line));
      }
    }

  }
  for (; j > 0; --j) {
    char *cons_line = smartlist_get(cons1, j-1);
    smartlist_add(cons2, tor_strdup(cons_line));
  }
  smartlist_reverse(cons2);
  return cons2;

error_cleanup:

  SMARTLIST_FOREACH_BEGIN(cons2, char*, line) {
    tor_free(line);
  } SMARTLIST_FOREACH_END(line);

  smartlist_free(cons2);

  return NULL;
}

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
  smartlist_t *cons2 = apply_diff(cons1, diff);
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
