#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"

typedef struct {
  int added;
  int deleted;
  int start1;
  int start2;
} change_t;

typedef struct {
  smartlist_t *list;
  int offset;
  int len;
} smartlist_slice_t;

INLINE smartlist_slice_t* smartlist_slice(smartlist_t *list, int offset, int len)
{
  smartlist_slice_t *slice = tor_malloc(sizeof(smartlist_slice_t));
  slice->list = list;
  slice->offset = offset;
  slice->len = len;
  return slice;
}

INLINE int smartlist_slice_string_pos(smartlist_slice_t *slice, char *string)
{
  int i, end = slice->offset + slice->len;
  char *el;
  for (i = slice->offset; i < end; ++i) {
    el = smartlist_get(slice->list, i);
    if (!strcmp(el, string)) return i;
  }
  return -1;
}

INLINE int max(int a, int b)
{
  return (a > b) ? a : b;
}

INLINE int line_eq(const char *line1, smartlist_t *list2, int i2)
{
  const char *line2 = smartlist_get(list2, i2);
  if (line1 == line2) return 0;
  return !strcmp(line1, line2);
}

INLINE int* lcs_lens(smartlist_slice_t *slice1, smartlist_slice_t *slice2, int direction)
{
  int i, j, si, sj;
  int *result = tor_malloc(sizeof(int) * (slice2->len+1));
  for (j = 0; j < slice2->len+1; ++j) result[j] = 0;
  int *prev = tor_malloc(sizeof(int) * (slice2->len+1));
  const char *line1;
  si = slice1->offset;
  if (direction == -1) si += (slice1->len-1);
  for (i = 0; i < slice1->len; ++i, si+=direction) {
    for (j = 0; j < slice2->len+1; ++j) prev[j] = result[j];
    line1 = smartlist_get(slice1->list, si);
    sj = slice2->offset;
    if (direction == -1) sj += (slice2->len-1);
    for (j = 0; j < slice2->len; ++j, sj+=direction) {
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

void diff_recurse(smartlist_slice_t *slice1, smartlist_slice_t *slice2,
    char *changed1, char *changed2)
{
  int j, end;
  if (slice1->len == 0) {
    end = slice2->offset + slice2->len;
    for (j = slice2->offset; j < end; ++j) {
      changed2[j] = 1;
    }

  } else if (slice2->len == 0) {
    end = slice1->offset + slice1->len;
    for (j = slice1->offset; j < end; ++j) {
      changed1[j] = 1;
    }

  } else if (slice1->len == 1) {
    char *line_common = smartlist_get(slice1->list, slice1->offset);
    int pos_common = smartlist_slice_string_pos(slice2, line_common);
    end = slice2->offset + slice2->len;
    for (j = slice2->offset; j < end; ++j) {
      if (j == pos_common) continue;
      changed2[j] = 1;
    }

  } else if (slice2->len == 1) {
    char *line_common = smartlist_get(slice2->list, slice2->offset);
    int pos_common = smartlist_slice_string_pos(slice1, line_common);
    end = slice1->offset + slice1->len;
    for (j = slice1->offset; j < end; ++j) {
      if (j == pos_common) continue;
      changed1[j] = 1;
    }

  } else {

    int mid = slice1->offset+(slice1->len/2);
    smartlist_slice_t *top = smartlist_slice(slice1->list,
        slice1->offset, mid-slice1->offset);
    smartlist_slice_t *bot = smartlist_slice(slice1->list,
        mid, (slice1->offset+slice1->len)-mid);

    int *lens_top = lcs_lens(top, slice2, 1);
    int *lens_bot = lcs_lens(bot, slice2, -1);
    int k=0, max_sum=-1;
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

    diff_recurse(top, left, changed1, changed2);
    diff_recurse(bot, right, changed1, changed2);
    tor_free(top);
    tor_free(bot);
    tor_free(left);
    tor_free(right);
  }
}

change_t* make_change(int start1, int start2, int end1, int end2)
{
  change_t *change = tor_malloc(sizeof(change_t));
  change->start1 = start1;
  change->start2 = start2;
  change->added = end2 - start2;
  change->deleted = end1 - start1;
  return change;
}

smartlist_t* calc_diff(smartlist_t *cons1, smartlist_t *cons2)
{
  int len1 = smartlist_len(cons1);
  int len2 = smartlist_len(cons2);
  char *changed1 = tor_malloc_zero(sizeof(char) * len1+1);
  char *changed2 = tor_malloc_zero(sizeof(char) * len2+1);
  smartlist_slice_t *cons1_sl = smartlist_slice(cons1, 0, len1);
  smartlist_slice_t *cons2_sl = smartlist_slice(cons2, 0, len2);
  diff_recurse(cons1_sl, cons2_sl, changed1, changed2);
  tor_free(cons1_sl);
  tor_free(cons2_sl);
  smartlist_t *changes = smartlist_new();
  int i1=0, i2=0, start1, start2;

  while (i1 < len1 || i2 < len2) {
    if (changed1[i1] || changed2[i2]) {
      start1 = i1, start2 = i2;

      while (changed1[i1]) i1++;
      while (changed2[i2]) i2++;

	  smartlist_add(changes, make_change(start1, start2, i1, i2));
	}
    if (i1 < len1) i1++;
    if (i2 < len2) i2++;
  }
  tor_free(changed1);
  tor_free(changed2);

  smartlist_t *result = smartlist_new();

  int i, j, end;
  char *line;
  for (i = smartlist_len(changes)-1; i >= 0; --i) {
    change_t *change = smartlist_get(changes, i);
    tor_assert(change->added > 0 || change->deleted > 0);
    if (change->added == 0) {
      tor_assert(change->deleted > 0);

      if (change->deleted == 1) {
        tor_asprintf(&line, "%id", change->start1+1);
      } else {
        tor_asprintf(&line, "%i,%id", change->start1+1, change->start1+change->deleted);
      }
      smartlist_add(result, line);

    } else if (change->deleted == 0) {
      tor_assert(change->added > 0);

      tor_asprintf(&line, "%ia", change->start1);
      smartlist_add(result, line);

      end = change->start2+change->added;
      for (j = change->start2; j < end; ++j) {
        line = smartlist_get(cons2, j);
        smartlist_add(result, tor_strdup(line));
      }

      smartlist_add(result, tor_strdup("."));

    } else {
      if (change->deleted == 1) {
        tor_asprintf(&line, "%ic", change->start1+1);
      } else {
        tor_asprintf(&line, "%i,%ic", change->start1+1, change->start1+change->deleted);
      }
      smartlist_add(result, line);

      end = change->start2+change->added;
      for (j = change->start2; j < end; ++j) {
        line = smartlist_get(cons2, j);
        smartlist_add(result, tor_strdup(line));
      }

      smartlist_add(result, tor_strdup("."));
    }

    tor_free(change);
  }
  smartlist_free(changes);

  return result;
}

int main(int argc, char **argv)
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
  smartlist_t *diff = calc_diff(orig, new);
  SMARTLIST_FOREACH_BEGIN(diff, char*, line) {
    printf("%s\n", line);
    tor_free(line);
  } SMARTLIST_FOREACH_END(line);

  tor_free(cons1);
  tor_free(cons2);

  smartlist_free(orig);
  smartlist_free(new);
  smartlist_free(diff);

  return 0;
}

// vim: et sw=2
