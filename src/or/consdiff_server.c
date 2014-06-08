#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"

typedef struct {
  const char* content;
  /*size_t len;*/
  int action;
} diff_line_t;

typedef struct {
  smartlist_t *list;
  int offset;
  int len;
  int direction;
} smartlist_slice_t;

enum diffaction {
  ACTION_NONE,
  ACTION_ADD,
  ACTION_DELETE
};

INLINE smartlist_slice_t* smartlist_slice(smartlist_t *list, int offset, int len)
{
  smartlist_slice_t *slice = tor_malloc(sizeof(smartlist_slice_t));
  slice->list = list;
  slice->offset = offset;
  slice->len = len;
  slice->direction = 1;
  return slice;
}

INLINE smartlist_slice_t* smartlist_slice_invert(smartlist_slice_t *slice)
{
  smartlist_slice_t *slice_inv = tor_malloc(sizeof(smartlist_slice_t));
  slice_inv->list = slice->list;
  slice_inv->offset = (slice->offset+(slice->direction*slice->len))-1;
  slice_inv->len = slice->len;
  slice_inv->direction = -1*(slice->direction);
  return slice_inv;
}

INLINE int smartlist_slice_contains_string(smartlist_slice_t *slice,
    const char *element) {
  int i, si;
  si = slice->offset;
  for (i = 0; i < slice->len; ++i, si+=slice->direction) {
    const char *s_el = smartlist_get(slice->list, si);
    if (strcmp(s_el, element) == 0) {
      return 1;
    }
  }
  return 0;
}

INLINE int max(int a, int b)
{
  return (a > b) ? a : b;
}

INLINE int line_eq(const char *line1, smartlist_t *list2, int i2)
{
  const char *line2 = smartlist_get(list2, i2);
  if (line1 == line2) return 0;
  return strcmp(line1, line2) == 0;
}

int* lcs_lens(smartlist_slice_t *slice1, smartlist_slice_t *slice2)
{
  int i, j, si, sj;
  tor_assert(slice1->direction == slice2->direction);
  int *result = tor_malloc(sizeof(int) * (slice2->len+1));
  for (j = 0; j < slice2->len+1; ++j) result[j] = 0;
  int *prev = tor_malloc(sizeof(int) * (slice2->len+1));
  const char *line1;
  si = slice1->offset;
  for (i = 0; i < slice1->len; ++i, si+=slice1->direction) {
    for (j = 0; j < slice2->len+1; ++j) prev[j] = result[j];
    line1 = smartlist_get(slice1->list, si);
    sj = slice2->offset;
    for (j = 0; j < slice2->len; ++j, sj+=slice2->direction) {
      if (line_eq(line1, slice2->list, sj)) {
        result[j + 1] = prev[j] + 1;
      } else {
        result[j + 1] = max(result[j], prev[j + 1]);
      }
    }
  }
  tor_free(prev);
  return result;
}

void print_slice(smartlist_slice_t *slice) {
  int i, si;
  si = slice->offset;
  for (i = 0; i < slice->len; ++i, si+=slice->direction) {
    printf("%s\n", (const char*)smartlist_get(slice->list, si));
  }
  printf("\n");
}

smartlist_t *lines_action(smartlist_slice_t *slice, char *line_common, int action) {
  smartlist_t *list = smartlist_new();
  int i, si;
  si = slice->offset;
  for (i = 0; i < slice->len; ++i, si+=slice->direction) {
    char *line = smartlist_get(slice->list, si);
    diff_line_t *diff_line = tor_malloc(sizeof(diff_line_t));
    diff_line->content = line;
    if (line_common != NULL && strcmp(line, line_common) == 0) {
      diff_line->action = ACTION_NONE;
    } else {
      diff_line->action = action;
    }
    smartlist_add(list, diff_line);
  }
  return list;
}

smartlist_t* lcs(smartlist_slice_t *slice1, smartlist_slice_t *slice2) {
  /*print_slice(slice1);*/
  /*print_slice(slice2);*/

  if (slice1->len == 0) {
    return lines_action(slice2, NULL, ACTION_ADD);
  }

  if (slice2->len == 0) {
    return lines_action(slice1, NULL, ACTION_DELETE);
  }

  if (slice1->len == 1) {
    char *line_common = smartlist_get(slice1->list, slice1->offset);
    return lines_action(slice2, line_common, ACTION_ADD);
  }

  if (slice2->len == 1) {
    char *line_common = smartlist_get(slice2->list, slice2->offset);
    return lines_action(slice1, line_common, ACTION_DELETE);
  }

  int mid = slice1->offset+(slice1->len/2);
  smartlist_slice_t *top = smartlist_slice(slice1->list,
      slice1->offset, mid-slice1->offset);
  smartlist_slice_t *bot = smartlist_slice(slice1->list,
      mid, (slice1->offset+slice1->len)-mid);

  int *lens_top = lcs_lens(top, slice2);
  smartlist_slice_t *bot_inv = smartlist_slice_invert(bot);
  smartlist_slice_t *slice2_inv = smartlist_slice_invert(slice2);
  int *lens_bot = lcs_lens(bot_inv, slice2_inv);
  int j, k=0, max_sum=-1;
  for (j = 0; j < slice2->len+1; ++j) {
    int sum = lens_top[j] + lens_bot[slice2->len-j];
    if (sum > max_sum) {
      k = j;
      max_sum = sum;
    }
  }
  tor_free(lens_top);
  tor_free(lens_bot);

  smartlist_slice_t *left = smartlist_slice(slice2->list,
      slice2->offset, k);
  smartlist_slice_t *right = smartlist_slice(slice2->list,
      slice2->offset+k, slice2->len-k);

  smartlist_t *lcs1 = lcs(top, left);
  smartlist_t *lcs2 = lcs(bot, right);
  smartlist_add_all(lcs1, lcs2);
  tor_free(top);
  tor_free(bot);
  tor_free(left);
  tor_free(right);
  tor_free(bot_inv);
  tor_free(slice2_inv);
  smartlist_free(lcs2);
  return lcs1;
}

int main(int argc, char **argv)
{
  smartlist_t *orig = smartlist_new();
  smartlist_t *new = smartlist_new();
  if (argc < 3) {
    fprintf(stderr, "Usage: %s file1 file2\n", argv[0]);
    return 1;
  }
  char *cons1 = read_file_to_str(argv[1], 0, NULL);
  char *cons2 = read_file_to_str(argv[2], 0, NULL);

  smartlist_slice_t *orig_s = smartlist_slice(orig, 0,
      tor_split_lines(orig, cons1, strlen(cons1)));
  smartlist_slice_t *new_s = smartlist_slice(new, 0,
      tor_split_lines(new, cons2, strlen(cons2)));
  smartlist_t *result = lcs(orig_s, new_s);

  SMARTLIST_FOREACH_BEGIN(result, diff_line_t*, diff_line) {
    switch(diff_line->action) {
      case ACTION_NONE:
        printf(" %s\n", diff_line->content);
        break;
      case ACTION_ADD:
        printf("+%s\n", diff_line->content);
        break;
      case ACTION_DELETE:
        printf("-%s\n", diff_line->content);
        break;
    }
  } SMARTLIST_FOREACH_END(diff_line);

  SMARTLIST_FOREACH_BEGIN(result, diff_line_t*, diff_line) {
    tor_free(diff_line);
  } SMARTLIST_FOREACH_END(diff_line);

  tor_free(cons1);
  tor_free(cons2);

  tor_free(orig_s);
  tor_free(new_s);
  smartlist_free(orig);
  smartlist_free(new);
  smartlist_free(result);

  return 0;
}

// vim: et sw=2
