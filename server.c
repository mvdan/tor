#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"

/** Data structure to define a slice of a smarltist */
typedef struct {
  /** Smartlist that this slice is made from */
  smartlist_t *list;
  /** Starting position of the smartlist */
  int offset;
  /** Number of elements in the slice */
  int len;
} smartlist_slice_t;

/** Create (allocate) a new slice from a smartlist.
 */
INLINE smartlist_slice_t *
smartlist_slice(smartlist_t *list, int offset, int len)
{
  smartlist_slice_t *slice = tor_malloc(sizeof(smartlist_slice_t));
  slice->list = list;
  slice->offset = offset;
  slice->len = len;
  return slice;
}

/** Like smartlist_string_pos, but limited to the bounds of the slice.
 */
INLINE int
smartlist_slice_string_pos(smartlist_slice_t *slice, const char *string)
{
  int i, end = slice->offset + slice->len;
  for (i = slice->offset; i < end; ++i) {
    const char *el = smartlist_get(slice->list, i);
    if (!strcmp(el, string)) return i;
  }
  return -1;
}

/** Helper: Compute the longest common substring lengths for the two slices.
 * Used as part of the diff generation to find the column at which to split
 * slice2 (divide and conquer) while still having the optimal solution.
 * If direction is -1, the navigation is reversed. Otherwise it should be 1.
 */
INLINE int *
lcs_lens(smartlist_slice_t *slice1, smartlist_slice_t *slice2, int direction)
{
  int i, j, si, sj;
  size_t a_size = sizeof(int) * (slice2->len+1);

  /* Resulting lcs lengths. */
  int *result = tor_malloc_zero(a_size);
  /* Copy of the lcs lengths from the last iteration. */
  int *prev = tor_malloc(a_size);

  si = slice1->offset;
  if (direction == -1) si += (slice1->len-1);
  for (i = 0; i < slice1->len; ++i, si+=direction) {

    /* Store the last results. */
    memcpy(prev, result, a_size);
    const char *line1 = smartlist_get(slice1->list, si);

    sj = slice2->offset;
    if (direction == -1) sj += (slice2->len-1);
    for (j = 0; j < slice2->len; ++j, sj+=direction) {

      const char *line2 = smartlist_get(slice2->list, sj);
      /* If the lines are equal, the lcs is one line longer. */
      if (!strcmp(line1, line2))
        result[j + 1] = prev[j] + 1;
      /* If not, see what lcs parent path is longer. */
      else
        result[j + 1] = MAX(result[j], prev[j + 1]);

    }
  }
  tor_free(prev);
  return result;
}

/** Helper: Trim any number of lines that are equally at the start or the end
 * of both slices.
 */
INLINE void
trim_slices(smartlist_slice_t *slice1, smartlist_slice_t *slice2)
{
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

/** Helper: Set all the appropriate changed booleans to true. The first slice
 * must be of length 0 or 1. All the lines of slice1 and slice2 which are not
 * present in the other slice will be set to changed in their bool array.
 * The two changed bool arrays are passed in the same order as the slices.
 */
INLINE void
set_changed(bitarray_t *changed1, bitarray_t *changed2,
    smartlist_slice_t *slice1, smartlist_slice_t *slice2)
{
  int toskip = -1;
  if (slice1->len == 1) {
    const char *line_common = smartlist_get(slice1->list, slice1->offset);
    toskip = smartlist_slice_string_pos(slice2, line_common);
    if (toskip == -1) bitarray_set(changed1, slice1->offset);
  }
  int i, end = slice2->offset + slice2->len;
  for (i = slice2->offset; i < end; ++i)
    if (i != toskip) bitarray_set(changed2, i);
}

/**
 * Helper: Work out all the changed booleans for all the lines in the two
 * slices, saving them in the corresponding changed arrays. This recursive
 * function will keep on splitting slice1 by half and splitting up slice2 by
 * the column that lcs_lens deems appropriate. Once any of the two slices gets
 * small enough, set_changed will be used to finally store that portion of the
 * result.
 */
void
calc_changes(smartlist_slice_t *slice1, smartlist_slice_t *slice2,
    bitarray_t *changed1, bitarray_t *changed2)
{
  trim_slices(slice1, slice2);

  if (slice1->len == 0) {
    set_changed(changed1, changed2, slice1, slice2);

  } else if (slice2->len == 0) {
    set_changed(changed2, changed1, slice2, slice1);

  } else if (slice1->len == 1) {
    set_changed(changed1, changed2, slice1, slice2);

  } else if (slice2->len == 1) {
    set_changed(changed2, changed1, slice2, slice1);

  /* Keep on splitting the slices in two. */
  } else {

    /* Split the first slice in half. */
    int mid = slice1->offset+(slice1->len/2);
    smartlist_slice_t *top = smartlist_slice(slice1->list,
        slice1->offset, mid-slice1->offset);
    smartlist_slice_t *bot = smartlist_slice(slice1->list,
        mid, (slice1->offset+slice1->len)-mid);

    /* 'k' will be the column that we find is optimal thanks to the lcs
     * lengths that lcs_lens reported.
     */
    int *lens_top = lcs_lens(top, slice2, 1);
    int *lens_bot = lcs_lens(bot, slice2, -1);
    int i, k=0, max_sum=-1;
    for (i = 0; i < slice2->len+1; ++i) {
      int sum = lens_top[i] + lens_bot[slice2->len-i];
      if (sum > max_sum) {
        k = i;
        max_sum = sum;
      }
    }
    tor_free(lens_top);
    tor_free(lens_bot);

    /* Split the second slice by the column 'k'. */
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

/** Helper: Get the identity hash from a router line, assuming that the line
 * at least appears to be a router line and thus starts with "r ".
 */
const char *
get_id_hash(const char *r_line)
{
  r_line += strlen("r ");
  const char *hash = strchr(r_line, ' ');
  if (hash == NULL) return NULL;
  hash++;
  const char *hash_end = strchr(hash, ' ');
  if (hash_end == NULL) return NULL;
  if (hash_end-hash < 27) return NULL;
  return hash;
}

/** Helper: Check that a line is a valid router entry. We must at least be
 * able to fetch a proper identity hash from it for it to be valid.
 */
INLINE int
is_valid_router_entry(const char *line)
{
  if (strncmp("r ", line, 2) != 0) return 0;
  return (get_id_hash(line) != NULL);
}

/** Helper: Find the next router line starting at the current position.
 */
INLINE int
next_router(smartlist_t *cons, int cur)
{
  int len = smartlist_len(cons);
  cur++;
  if (cur >= len) return len;
  const char *line = smartlist_get(cons, cur);
  while (!is_valid_router_entry(line)) {
    cur++;
    if (cur >= len) return len;
    line = smartlist_get(cons, cur);
  }
  return cur;
}

/** Helper: compare two identity hashes which may be of different lengths.
 */
INLINE int
hashcmp(const char *hash1, const char *hash2)
{
  if (hash1 == NULL || hash2 == NULL) return -1;
  int len1 = strchr(hash1, ' ')-hash1;
  int len2 = strchr(hash2, ' ')-hash2;
  return strncmp(hash1, hash2, MAX(len1, len2));
}

/** Generate an ed diff as a smartlist from two consensuses, also given as
 * smartlists. Will return NULL if the diff could not be generated, which can
 * only happen if any lines the script had to add matched ".".
 */
smartlist_t *
gen_diff(smartlist_t *cons1, smartlist_t *cons2)
{
  int len1 = smartlist_len(cons1);
  int len2 = smartlist_len(cons2);
  bitarray_t *changed1 = bitarray_init_zero(len1);
  bitarray_t *changed2 = bitarray_init_zero(len2);
  int i1=0, i2=0;

  const char *hash1 = NULL;
  const char *hash2 = NULL;

  /* While we havent't reached the end of both consensuses...
   * We always reach both ends at some point. The first thing that the loop
   * does is advance each of the line positions if they haven't reached the
   * end. Later on, TODO
   */
  while (i1 < len1 || i2 < len2) {
    int start1 = i1, start2 = i2;

    /* Advance each of the two navigation positions by one router entry if
     * possible.
     */
    if (i1 < len1) {
      i1 = next_router(cons1, i1);
      if (i1 != len1) hash1 = get_id_hash(smartlist_get(cons1, i1));
    }

    if (i2 < len2) {
      i2 = next_router(cons2, i2);
      if (i2 != len2) hash2 = get_id_hash(smartlist_get(cons2, i2));
    }

    /* Keep on advancing the lower (by identity hash sorting) position until
     * we have two matching positions or the end of both consensues.
     */
    int cmp = hashcmp(hash1, hash2);
    while (i1 < len1 && i2 < len2 && cmp != 0) {
      if (i1 < len1 && cmp < 0) {
        i1 = next_router(cons1, i1);
        if (i1 == len1) {
          i2 = len2;
          break;
        }
        hash1 = get_id_hash(smartlist_get(cons1, i1));
      }
      if (i2 < len2 && cmp > 0) {
        i2 = next_router(cons2, i2);
        if (i2 == len2) {
          i1 = len1;
          break;
        }
        hash2 = get_id_hash(smartlist_get(cons2, i2));
      }
      cmp = hashcmp(hash1, hash2);
    }

    /* Make slices out of these chunks (up to the common router entry) and
     * calculate the changes for them.
     */
    smartlist_slice_t *cons1_sl = smartlist_slice(cons1, start1, i1-start1);
    smartlist_slice_t *cons2_sl = smartlist_slice(cons2, start2, i2-start2);
    calc_changes(cons1_sl, cons2_sl, changed1, changed2);
    tor_free(cons1_sl);
    tor_free(cons2_sl);

  }


  /* Navigate the changes in reverse order and generate one ed command for
   * each chunk of changes.
   */
  i1=len1-1, i2=len2-1;
  smartlist_t *result = smartlist_new();
  while (i1 > 0 || i2 > 0) {

    /* We are at a point were no changed bools are true, so just keep going. */
    if (!(i1 >= 0 && bitarray_is_set(changed1, i1)) &&
        !(i2 >= 0 && bitarray_is_set(changed2, i2))) {
      if (i1 >= 0) i1--;
      if (i2 >= 0) i2--;
      continue;
    }

    int end1 = i1, end2 = i2;

    /* Grab all contiguous changed lines */
    while (i1 >= 0 && bitarray_is_set(changed1, i1)) i1--;
    while (i2 >= 0 && bitarray_is_set(changed2, i2)) i2--;

    int start1 = i1+1, start2 = i2+1;
    int added = end2-i2, deleted = end1-i1;

    if (added == 0) {
      if (deleted == 1) smartlist_add_asprintf(result, "%id", start1+1);
      else smartlist_add_asprintf(result, "%i,%id", start1+1, start1+deleted);

    } else {
      if (deleted == 0)
        smartlist_add_asprintf(result, "%ia", start1);
      else if (deleted == 1)
        smartlist_add_asprintf(result, "%ic", start1+1);
      else
        smartlist_add_asprintf(result, "%i,%ic", start1+1, start1+deleted);

      int i;
      for (i = start2; i <= end2; ++i) {
        const char *line = smartlist_get(cons2, i);
        /* One of the added lines is ".", so cleanup and error. */
        if (!strcmp(line, ".")) goto error_cleanup;
        smartlist_add(result, tor_strdup(line));
      }
      smartlist_add_asprintf(result, ".");
    }
  }

  bitarray_free(changed1);
  bitarray_free(changed2);

  return result;

error_cleanup:

  bitarray_free(changed1);
  bitarray_free(changed2);

  SMARTLIST_FOREACH_BEGIN(result, char*, line) {
    tor_free(line);
  } SMARTLIST_FOREACH_END(line);

  smartlist_free(result);

  return NULL;
}

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
  smartlist_t *diff = gen_diff(orig, new);
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
