#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"

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
  size_t a_len = sizeof(int) * (slice2->len+1);
  int *result = tor_malloc_zero(a_len);
  int *prev = tor_malloc(a_len);
  const char *line1;
  si = slice1->offset;
  if (direction == -1) si += (slice1->len-1);
  for (i = 0; i < slice1->len; ++i, si+=direction) {
    memcpy(prev, result, a_len);
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

smartlist_t* calc_diff(smartlist_t *cons1, smartlist_t *cons2)
{
  int len1 = smartlist_len(cons1);
  int len2 = smartlist_len(cons2);
  char *changed1 = tor_malloc_zero(sizeof(char) * len1);
  char *changed2 = tor_malloc_zero(sizeof(char) * len2);
  smartlist_slice_t *cons1_sl = smartlist_slice(cons1, 0, len1);
  smartlist_slice_t *cons2_sl = smartlist_slice(cons2, 0, len2);

  diff_recurse(cons1_sl, cons2_sl, changed1, changed2);
  tor_free(cons1_sl);
  tor_free(cons2_sl);

  int i, i1=len1-1, i2=len2-1;
  int start1, start2, end1, end2;
  int added, deleted;

  smartlist_t *result = smartlist_new();
  while (i1 > 0 || i2 > 0) {
    if ((i1 >= 0 && changed1[i1]) || (i2 >= 0 && changed2[i2])) {
      end1 = i1, end2 = i2;

      while ((i1 >= 0 && changed1[i1])) i1--;
      while ((i2 >= 0 && changed2[i2])) i2--;

      start1 = i1+1;
      start2 = i2+1;
      added = end2-i2;
      deleted = end1-i1;
      if (added == 0) {
        if (deleted == 1) smartlist_add_asprintf(result, "%id", start1+1);
        else smartlist_add_asprintf(result, "%i,%id", start1+1, start1+deleted);

      } else if (deleted == 0) {
        smartlist_add_asprintf(result, "%ia", start1);

        for (i = start2; i <= end2; ++i)
          smartlist_add(result, tor_strdup(smartlist_get(cons2, i)));

        smartlist_add_asprintf(result, ".");

      } else {
        if (deleted == 1) smartlist_add_asprintf(result, "%ic", start1+1);
        else smartlist_add_asprintf(result, "%i,%ic", start1+1, start1+deleted);

        for (i = start2; i <= end2; ++i)
          smartlist_add(result, tor_strdup(smartlist_get(cons2, i)));

        smartlist_add_asprintf(result, ".");
      }
	}

    if (i1 >= 0) i1--;
    if (i2 >= 0) i2--;

  }
  tor_free(changed1);
  tor_free(changed2);

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
