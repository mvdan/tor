#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"
#include "consdiff.h"

/** Create (allocate) a new slice from a smartlist. Assumes that the offset
 * and the consequent length are in the bounds of the smartlist.
 */
static smartlist_slice_t *
smartlist_slice(smartlist_t *list, int offset, int len)
{
  int list_len = smartlist_len(list);
  tor_assert(offset >= 0);
  /* If we are making a slice out of an empty list, ignore the 0<0 failure. */
  tor_assert(offset < list_len || list_len == 0);
  tor_assert(len >= 0 && offset+len <= list_len);

  smartlist_slice_t *slice = tor_malloc(sizeof(smartlist_slice_t));
  slice->list = list;
  slice->offset = offset;
  slice->len = len;
  return slice;
}

/** Like smartlist_string_pos, but limited to the bounds of the slice.
 */
static int
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
 * If direction is -1, the navigation is reversed. Otherwise it must be 1.
 * The length of the resulting integer array is that of the second slice plus
 * one.
 */
static int *
lcs_lens(smartlist_slice_t *slice1, smartlist_slice_t *slice2, int direction)
{
  tor_assert(direction == 1 || direction == -1);
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
static void
trim_slices(smartlist_slice_t *slice1, smartlist_slice_t *slice2)
{
  const char *line1 = NULL;
  const char *line2 = NULL;

  while (slice1->len>0 && slice2->len>0) {
    line1 = smartlist_get(slice1->list, slice1->offset);
    line2 = smartlist_get(slice2->list, slice2->offset);
    if (strcmp(line1, line2)) break;
    slice1->offset++; slice1->len--;
    slice2->offset++; slice2->len--;
  }

  int i1 = (slice1->offset+slice1->len)-1;
  int i2 = (slice2->offset+slice2->len)-1;
  line1 = NULL;
  line2 = NULL;

  while (slice1->len>0 && slice2->len>0) {
    line1 = smartlist_get(slice1->list, i1);
    line2 = smartlist_get(slice2->list, i2);
    if (strcmp(line1, line2)) break;
    i1--; slice1->len--;
    i2--; slice2->len--;
  }

}

/** Helper: Set all the appropriate changed booleans to true. The first slice
 * must be of length 0 or 1. All the lines of slice1 and slice2 which are not
 * present in the other slice will be set to changed in their bool array.
 * The two changed bool arrays are passed in the same order as the slices.
 */
static void
set_changed(bitarray_t *changed1, bitarray_t *changed2,
    smartlist_slice_t *slice1, smartlist_slice_t *slice2)
{
  tor_assert(slice1->len == 0 || slice1->len == 1);
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
 * result. It is assumed that the lengths of the changed bitarrays match those
 * of their full consensus smartlists.
 */
static void
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

/* This table is from crypto.c. The SP and PAD defines are different. */
#define X 255
#define SP X
#define PAD X
static const uint8_t base64_compare_table[256] = {
  X, X, X, X, X, X, X, X, X, SP, SP, SP, X, SP, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  SP, X, X, X, X, X, X, X, X, X, X, 62, X, X, X, 63,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, X, X, X, PAD, X, X,
  X, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, X, X, X, X, X,
  X, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
};

/** Helper: Get the identity hash from a router line, assuming that the line
 * at least appears to be a router line and thus starts with "r ".
 */
static const char *
get_id_hash(const char *r_line)
{
  r_line += strlen("r ");

  /* Skip the router name. */
  const char *hash = strchr(r_line, ' ');
  if (hash == NULL) return NULL;

  hash++;
  const unsigned char *hash_end = (unsigned char*)hash;
  /* Stop when the first non-base64 character is found. */
  while (base64_compare_table[*hash_end] != X) hash_end++;

  /* The minimum length is 27 characters. */
  if ((char*)hash_end-hash < 27) return NULL;
  return hash;
}

/** Helper: Check that a line is a valid router entry. We must at least be
 * able to fetch a proper identity hash from it for it to be valid.
 */
static int
is_valid_router_entry(const char *line)
{
  if (strncmp("r ", line, 2) != 0) return 0;
  return (get_id_hash(line) != NULL);
}

/** Helper: Find the next router line starting at the current position.
 * Assumes that cur is lower than the length of the smartlist, i.e. it is a
 * line within the bounds of the consensus. The only exception is when we
 * don't want to skip the first line, in which case cur will be -1.
 */
static int
next_router(smartlist_t *cons, int cur)
{
  int len = smartlist_len(cons);
  tor_assert(cur >= -1 && cur < len);
  if (++cur >= len) return len;
  const char *line = smartlist_get(cons, cur);
  while (!is_valid_router_entry(line)) {
    if (++cur >= len) return len;
    line = smartlist_get(cons, cur);
  }
  return cur;
}

/** Helper: compare two base64-encoded identity hashes which may be of
 * different lengths. Comparison ends when the first non-base64 char is found.
 */
static int
hashcmp(const char *hash1, const char *hash2)
{
  /* NULL is always lower, useful for last_hash which starts at NULL. */
  if (hash1 == NULL && hash2 == NULL) return 0;
  if (hash1 == NULL) return -1;
  if (hash2 == NULL) return 1;

  /* Don't index with a char; char may be signed. */
  const unsigned char *a = (unsigned char*)hash1;
  const unsigned char *b = (unsigned char*)hash2;
  while (1) {
    uint8_t av = base64_compare_table[*a];
    uint8_t bv = base64_compare_table[*b];
    if (av == X) {
      if (bv == X)
        return 0; /* Both ended with exactly the same characters. */
      else
        return -1; /* hash2 goes on longer than hash1 and thus hash1 is lower. */
    } else if (bv == X) {
      return 1; /* hash1 goes on longer than hash2 and thus hash1 is greater. */
    } else if (av < bv) {
      return -1; /* The first difference shows that hash1 is lower. */
    } else if (av > bv) {
      return 1; /* The first difference shows that hash1 is greater. */
    } else {
      ++a;
      ++b;
    }
  }
}

/** Generate an ed diff as a smartlist from two consensuses, also given as
 * smartlists. Will return NULL if the diff could not be generated, which can
 * happen if any lines the script had to add matched "." or if the routers
 * were not properly ordered. Neither of the two consensuses are modified in
 * any way, so it's up to the caller to free their resources.
 */
smartlist_t *
gen_diff(smartlist_t *cons1, smartlist_t *cons2)
{
  int len1 = smartlist_len(cons1);
  int len2 = smartlist_len(cons2);
  smartlist_t *result = smartlist_new();
  bitarray_t *changed1 = bitarray_init_zero(len1);
  bitarray_t *changed2 = bitarray_init_zero(len2);
  int i1=-1, i2=-1;
  int start1=0, start2=0;

  const char *hash1 = NULL;
  const char *hash2 = NULL;

  /* To check that hashes are ordered properly */
  const char *last_hash1 = NULL;
  const char *last_hash2 = NULL;

  /* i1 and i2 are initialized at the first line of each consensus. They never
   * reach past len1 and len2 respectively, since next_router doesn't let that
   * happen. i1 and i2 are advanced by at least one line at each iteration as
   * long as they have not yet reached len1 and len2, so the loop is
   * guaranteed to end, and each pair of (i1,i2) will be inspected at most
   * once.
   */
  while (i1 < len1 || i2 < len2) {
    /* Advance each of the two navigation positions by one router entry if not
     * yet at the end.
     */
    if (i1 < len1) {
      i1 = next_router(cons1, i1);
      if (i1 != len1) {
        last_hash1 = hash1;
        hash1 = get_id_hash(smartlist_get(cons1, i1));
        /* Identity hashes must always increase. */
        if (hashcmp(hash1, last_hash1) <= 0) goto error_cleanup;
      }
    }

    if (i2 < len2) {
      i2 = next_router(cons2, i2);
      if (i2 != len2) {
        last_hash2 = hash2;
        hash2 = get_id_hash(smartlist_get(cons2, i2));
        /* Identity hashes must always increase. */
        if (hashcmp(hash2, last_hash2) <= 0) goto error_cleanup;
      }
    }

    /* If we have reached the end of both consensuses, there is no need to
     * compare hashes anymore, since this is the last iteration. */
    if (i1 < len1 || i2 < len2) {

      /* Keep on advancing the lower (by identity hash sorting) position until
       * we have two matching positions. The only other possible outcome is
       * that a lower position reaches the end of the consensus before it can
       * reach a hash that is no longer the lower one. Since there will always
       * be a lower hash for as long as the loop runs, one of the two indexes
       * will always be incremented, thus assuring that the loop must end
       * after a finite number of iterations.
       */
      int cmp = hashcmp(hash1, hash2);
      while (cmp != 0) {
        if (i1 < len1 && cmp < 0) {
          i1 = next_router(cons1, i1);
          if (i1 == len1) {
            /* We finished the first consensus, so grab all the remaining
             * lines of the second consensus and finish up. */
            i2 = len2;
            break;
          }
          last_hash1 = hash1;
          hash1 = get_id_hash(smartlist_get(cons1, i1));
          /* Identity hashes must always increase. */
          if (hashcmp(hash1, last_hash1) <= 0) goto error_cleanup;
        }
        if (i2 < len2 && cmp > 0) {
          i2 = next_router(cons2, i2);
          if (i2 == len2) {
            /* We finished the second consensus, so grab all the remaining
             * lines of the first consensus and finish up. */
            i1 = len1;
            break;
          }
          last_hash2 = hash2;
          hash2 = get_id_hash(smartlist_get(cons2, i2));
          /* Identity hashes must always increase. */
          if (hashcmp(hash2, last_hash2) <= 0) goto error_cleanup;
        }
        cmp = hashcmp(hash1, hash2);
      }
    }

    /* Make slices out of these chunks (up to the common router entry) and
     * calculate the changes for them.
     */
    smartlist_slice_t *cons1_sl = smartlist_slice(cons1, start1, i1-start1);
    smartlist_slice_t *cons2_sl = smartlist_slice(cons2, start2, i2-start2);
    calc_changes(cons1_sl, cons2_sl, changed1, changed2);
    tor_free(cons1_sl);
    tor_free(cons2_sl);
    start1 = i1, start2 = i2;
  }

  /* Navigate the changes in reverse order and generate one ed command for
   * each chunk of changes.
   */
  i1=len1-1, i2=len2-1;
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

  SMARTLIST_FOREACH(result, char*, line, tor_free(line));
  smartlist_free(result);

  return NULL;
}

/** Apply the diff to the consensus and return a new consensus, also as a
 * line-based smartlist. Will return NULL if the ed diff is not properly
 * formatted. Neither the consensus nor the diff are modified in any way, so
 * it's up to the caller to free their resources.
 */

smartlist_t *
apply_diff(smartlist_t *cons1, smartlist_t *diff)
{
  int i, diff_len = smartlist_len(diff);
  int j = smartlist_len(cons1);
  smartlist_t *cons2 = smartlist_new();

  for (i=0; i<diff_len; ++i) {
    const char *diff_line = smartlist_get(diff, i);
    char *endptr1, *endptr2;
    int start, end;
#define LINE_BASE 10
    start = (int)strtol(diff_line, &endptr1, LINE_BASE);

    /* Missing range start. */
    if (endptr1 == diff_line) goto error_cleanup;

    /* Two-item range */
    if (*endptr1 == ',') {
        end = (int)strtol(endptr1+1, &endptr2, LINE_BASE);
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

    /* Add unchanged lines. */
    for (; j > end; --j) {
      const char *cons_line = smartlist_get(cons1, j-1);
      smartlist_add(cons2, tor_strdup(cons_line));
    }

    /* Ignore removed lines. */
    if (action == 'c' || action == 'd') {
      while (--j >= start) ;
    }

    /** Add new lines.
     * In reverse order, since it will all be reversed at the end. */
    if (action == 'a' || action == 'c') {
      int added_end = i;

      /* It would make no sense to add zero new lines. */
      if (!strcmp(smartlist_get(diff, ++i), ".")) goto error_cleanup;

      /* Fetch the reverse start of the added lines. */
      while (strcmp(smartlist_get(diff, ++i), ".")) ;
      int added_i = i-1;

      while (added_i > added_end) {
        const char *added_line = smartlist_get(diff, added_i--);
        smartlist_add(cons2, tor_strdup(added_line));
      }
    }

  }

  /* Add remaining unchanged lines. */
  for (; j > 0; --j) {
    const char *cons_line = smartlist_get(cons1, j-1);
    smartlist_add(cons2, tor_strdup(cons_line));
  }

  /* Reverse the whole thing since we did it from the end. */
  smartlist_reverse(cons2);
  return cons2;

error_cleanup:

  SMARTLIST_FOREACH(cons2, char*, line, tor_free(line));
  smartlist_free(cons2);

  return NULL;
}

// vim: et sw=2
