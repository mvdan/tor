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

INLINE int smartlist_slice_string_pos(smartlist_slice_t *slice, const char *string)
{
  int i, end = slice->offset + slice->len;
  const char *el;
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
  size_t a_size = sizeof(int) * (slice2->len+1);
  int *result = tor_malloc_zero(a_size);
  int *prev = tor_malloc(a_size);
  const char *line1;
  si = slice1->offset;
  if (direction == -1) si += (slice1->len-1);
  for (i = 0; i < slice1->len; ++i, si+=direction) {
    memcpy(prev, result, a_size);
    line1 = smartlist_get(slice1->list, si);
    sj = slice2->offset;
    if (direction == -1) sj += (slice2->len-1);
    for (j = 0; j < slice2->len; ++j, sj+=direction) {
      if (line_eq(line1, slice2->list, sj))
        result[j + 1] = prev[j] + 1;
      else
        result[j + 1] = max(result[j], prev[j + 1]);
    }
  }
  tor_free(prev);
  return result;
}

INLINE void trim_slices(smartlist_slice_t *slice1, smartlist_slice_t *slice2) {
  const char *line1 = smartlist_get(slice1->list, slice1->offset);
  const char *line2 = smartlist_get(slice2->list, slice2->offset);

  while (slice1->len>0 && slice2->len>0 && !strcmp(line1, line2)) {
    slice1->offset++; slice1->len--;
    slice2->offset++; slice2->len--;
    line1 = smartlist_get(slice1->list, slice1->offset);
    line2 = smartlist_get(slice2->list, slice2->offset);
  }

  int i1 = (slice1->offset+slice1->len)-1;
  int i2 = (slice2->offset+slice2->len)-1;
  line1 = smartlist_get(slice1->list, i1);
  line2 = smartlist_get(slice2->list, i2);

  while (slice1->len>0 && slice2->len>0 && !strcmp(line1, line2)) {
    i1--; slice1->len--;
    i2--; slice2->len--;
    line1 = smartlist_get(slice1->list, i1);
    line2 = smartlist_get(slice2->list, i2);
  }

}

void calc_changes(smartlist_slice_t *slice1, smartlist_slice_t *slice2,
    char *changed1, char *changed2)
{
  trim_slices(slice1, slice2);
  int i, end;

  if (slice1->len == 0) {
    end = slice2->offset + slice2->len;
    for (i = slice2->offset; i < end; ++i) changed2[i] = 1;

  } else if (slice2->len == 0) {
    end = slice1->offset + slice1->len;
    for (i = slice1->offset; i < end; ++i) changed1[i] = 1;

  } else if (slice1->len == 1) {
    const char *line_common = smartlist_get(slice1->list, slice1->offset);
    int pos_common = smartlist_slice_string_pos(slice2, line_common);
    end = slice2->offset + slice2->len;
    if (pos_common == -1) {
      changed1[slice1->offset] = 1;
      for (i = slice2->offset; i < end; ++i) changed2[i] = 1;
    } else {
      for (i = slice2->offset; i < end; ++i)
        if (i != pos_common) changed2[i] = 1;
    }

  } else if (slice2->len == 1) {
    const char *line_common = smartlist_get(slice2->list, slice2->offset);
    int pos_common = smartlist_slice_string_pos(slice1, line_common);
    end = slice1->offset + slice1->len;
    if (pos_common == -1) {
      changed2[slice2->offset] = 1;
      for (i = slice1->offset; i < end; ++i) changed1[i] = 1;
    } else {
      for (i = slice1->offset; i < end; ++i)
        if (i != pos_common) changed1[i] = 1;
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
    for (i = 0; i < slice2->len+1; ++i) {
      int sum = lens_top[i] + lens_bot[slice2->len-i];
      if (sum > max_sum) {
        k = i;
        max_sum = sum;
      }
    }
    tor_free(lens_top);
    tor_free(lens_bot);

    smartlist_slice_t *left = smartlist_slice(slice2->list,
        slice2->offset, k);
    smartlist_slice_t *right = smartlist_slice(slice2->list,
        slice2->offset+k, slice2->len-k);

    calc_changes(top, left, changed1, changed2);
    calc_changes(bot, right, changed1, changed2);
    tor_free(top);
    tor_free(bot);
    tor_free(left);
    tor_free(right);
  }
}

INLINE int next_router(smartlist_t *cons, int cur) {
  const char *line = smartlist_get(cons, ++cur);
  int len = smartlist_len(cons);
  while (cur < len && strncmp("r ", line, 2))
    line = smartlist_get(cons, ++cur);
  return cur;
}

INLINE const char* get_hash(const char *line) {
  const char *c=line+strlen("r ")+1;
  while (*c != ' ') c++;
  return ++c;
}

INLINE int hashcmp(const char *hash1, const char *hash2) {
  if (hash1 == NULL && hash2 == NULL) return 0;
  return strncmp(hash1, hash2, 27);
}

smartlist_t* gen_diff(smartlist_t *cons1, smartlist_t *cons2)
{
  int len1 = smartlist_len(cons1);
  int len2 = smartlist_len(cons2);
  char *changed1 = tor_malloc_zero(sizeof(char) * len1);
  char *changed2 = tor_malloc_zero(sizeof(char) * len2);
  int i, i1=0, i2=0;
  int start1, start2;

  const char *line1 = smartlist_get(cons1, i1);
  const char *line2 = smartlist_get(cons2, i2);
  const char *hash1 = NULL;
  const char *hash2 = NULL;

  while (i1 < len1 || i2 < len2) {
    start1 = i1;
    start2 = i2;

    if (i1 < len1) {
      i1 = next_router(cons1, i1);
      if (i1 != len1) {
        line1 = smartlist_get(cons1, i1);
        hash1 = get_hash(line1);
      }
    }

    if (i2 < len2) {
      i2 = next_router(cons2, i2);
      if (i2 != len2) {
        line2 = smartlist_get(cons2, i2);
        hash2 = get_hash(line2);
      }
    }

    int cmp = hashcmp(hash1, hash2);
    while (cmp != 0) {
      while (i1 < len1 && cmp < 0) {
        i1 = next_router(cons1, i1);
        if (i1 == len1) break;
        line1 = smartlist_get(cons1, i1);
        hash1 = get_hash(line1);
        cmp = hashcmp(hash1, hash2);
      }
      while (i2 < len2 && cmp > 0) {
        i2 = next_router(cons2, i2);
        if (i2 == len2) break;
        line2 = smartlist_get(cons2, i2);
        hash2 = get_hash(line2);
        cmp = hashcmp(hash1, hash2);
      }
      if (i1 == len1 || i2 == len2) break;
    }

    smartlist_slice_t *cons1_sl = smartlist_slice(cons1, start1, i1-start1);
    smartlist_slice_t *cons2_sl = smartlist_slice(cons2, start2, i2-start2);
    calc_changes(cons1_sl, cons2_sl, changed1, changed2);
    tor_free(cons1_sl);
    tor_free(cons2_sl);

  }

  i1=len1-1, i2=len2-1;
  int end1, end2;
  int added, deleted;

  smartlist_t *result = smartlist_new();
  while (i1 > 0 || i2 > 0) {
    if ((i1 >= 0 && changed1[i1]) || (i2 >= 0 && changed2[i2])) {
      end1 = i1, end2 = i2;

      while (i1 >= 0 && changed1[i1]) i1--;
      while (i2 >= 0 && changed2[i2]) i2--;

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
  smartlist_t *diff = gen_diff(orig, new);
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
