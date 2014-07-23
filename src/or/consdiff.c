/* Copyright (c) 2014, Daniel Martí
 * Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file consdiff.c
 * \brief Consensus diff implementation, including both the generation and the
 * application of diffs in a minimal ed format.
 *
 * The consensus diff application is done in consdiff_apply_diff, which relies
 * on apply_ed_diff for the main ed diff part and on some digest helper
 * functions to check the digest hashes found in the consensus diff header.
 *
 * The consensus diff generation is more complex. consdiff_gen_diff generates
 * it, relying on gen_ed_diff to generate the ed diff and some digest helper
 * functions to generate the digest hashes.
 *
 * gen_ed_diff is the tricky bit. In it simplest form, it will take quadratic
 * time and linear space to generate an ed diff given two smartlists. As shown
 * in its comment section, calling calc_changes on the entire two consensuses
 * will calculate what is to be added and what is to be deleted in the diff.
 * Its comment section briefly explains how it works.
 *
 * In our case specific to consensuses, we take advantage of the fact that
 * consensuses list routers sorted by their identities. We use that
 * information to avoid running calc_changes on the whole smartlists.
 * gen_ed_diff will navigate through the two consensuses identity by identity
 * and will send small couples of slices to calc_changes, keeping the running
 * time near-linear. This is explained in more detail in the gen_ed_diff
 * comments.
 **/

#include "or.h"
#include "config.h"
#include "container.h"
#include "crypto.h"
#include "util.h"
#include "consdiff.h"

/** Create (allocate) a new slice from a smartlist. Assumes that the start
 * and the end indexes are within the bounds of the initial smartlist. The end
 * element is not part of the resulting slice. If end is -1, the slice is to
 * reach the end of the smartlist.
 */
static smartlist_slice_t *
smartlist_slice(smartlist_t *list, int start, int end)
{
  int list_len = smartlist_len(list);
  smartlist_slice_t *slice;
  tor_assert(start >= 0);
  tor_assert(start <= list_len);
  if (end == -1) end = list_len;
  tor_assert(start <= end);

  slice = tor_malloc(sizeof(smartlist_slice_t));
  slice->list = list;
  slice->offset = start;
  slice->len = end - start;
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
  int i, j, si, sj;
  size_t a_size = sizeof(int) * (slice2->len+1);

  /* Resulting lcs lengths. */
  int *result = tor_malloc_zero(a_size);
  /* Copy of the lcs lengths from the last iteration. */
  int *prev = tor_malloc(a_size);

  tor_assert(direction == 1 || direction == -1);

  si = slice1->offset;
  if (direction == -1) si += (slice1->len-1);
  for (i = 0; i < slice1->len; ++i, si+=direction) {

    const char *line1 = smartlist_get(slice1->list, si);
    /* Store the last results. */
    memcpy(prev, result, a_size);

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
  const char *line1 = NULL, *line2 = NULL;
  int i1, i2;

  while (slice1->len>0 && slice2->len>0) {
    line1 = smartlist_get(slice1->list, slice1->offset);
    line2 = smartlist_get(slice2->list, slice2->offset);
    if (strcmp(line1, line2)) break;
    slice1->offset++; slice1->len--;
    slice2->offset++; slice2->len--;
  }

  i1 = (slice1->offset+slice1->len)-1;
  i2 = (slice2->offset+slice2->len)-1;
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
  int i, end, toskip = -1;
  tor_assert(slice1->len == 0 || slice1->len == 1);

  if (slice1->len == 1) {
    const char *line_common = smartlist_get(slice1->list, slice1->offset);
    toskip = smartlist_slice_string_pos(slice2, line_common);
    if (toskip == -1) bitarray_set(changed1, slice1->offset);
  }
  end = slice2->offset + slice2->len;
  for (i = slice2->offset; i < end; ++i)
    if (i != toskip) bitarray_set(changed2, i);
}

/**
 * Helper: Figure out what elements are new or gone on the second smartlist
 * relative to the first smartlist, and store the booleans in the bitarrays.
 * True on the first bitarray means the element is gone, true on the second
 * bitarray means it's new.
 *
 * In its base case, either of the smartlists is of length <= 1 and we can
 * quickly see what elements are new or are gone. In the other case, we will
 * split one smartlist by half and we'll use optimal_column_to_split to find
 * the optimal column at which to split the second smartlist so that we are
 * finding the smallest diff possible.
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

    int mid, *lens_top, *lens_bot, i, k, max_sum;
    smartlist_slice_t *top, *bot, *left, *right;

    /* Split the first slice in half. */
    mid = slice1->len/2;
    top = smartlist_slice(slice1->list, slice1->offset, slice1->offset+mid);
    bot = smartlist_slice(slice1->list, slice1->offset+mid,
        slice1->offset+slice1->len);

    /* 'k' will be the column that we find is optimal thanks to the lcs
     * lengths that lcs_lens reported.
     */
    lens_top = lcs_lens(top, slice2, 1);
    lens_bot = lcs_lens(bot, slice2, -1);
    k=0, max_sum=-1;
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
    left = smartlist_slice(slice2->list, slice2->offset, slice2->offset+k);
    right = smartlist_slice(slice2->list, slice2->offset+k,
        slice2->offset+slice2->len);

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
  const char *hash, *hash_end;
  r_line += strlen("r ");

  /* Skip the router name. */
  hash = strchr(r_line, ' ');
  if (hash == NULL) return NULL;

  hash++;
  hash_end = hash;
  /* Stop when the first non-base64 character is found. Use unsigned chars to
   * avoid negative indexes causing crashes. */
  while (base64_compare_table[*((unsigned char*)hash_end)] != X)
    hash_end++;

  /* Empty hash. */
  if (hash_end == hash) return NULL;

  return hash;
}

/** Helper: Check that a line is a valid router entry. We must at least be
 * able to fetch a proper identity hash from it for it to be valid.
 */
static int
is_valid_router_entry(const char *line)
{
  if (strcmpstart(line, "r ") != 0) return 0;
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
  const char *line;
  int len = smartlist_len(cons);
  tor_assert(cur >= -1 && cur < len);
  if (++cur >= len) return len;
  line = smartlist_get(cons, cur);
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
base64cmp(const char *hash1, const char *hash2)
{
  const unsigned char *a, *b;
  /* NULL is always lower, useful for last_hash which starts at NULL. */
  if (hash1 == NULL && hash2 == NULL) return 0;
  if (hash1 == NULL) return -1;
  if (hash2 == NULL) return 1;

  /* Don't index with a char; char may be signed. */
  a = (unsigned char*)hash1;
  b = (unsigned char*)hash2;
  while (1) {
    uint8_t av = base64_compare_table[*a];
    uint8_t bv = base64_compare_table[*b];
    if (av == X) {
      if (bv == X)
        /* Both ended with exactly the same characters. */
        return 0;
      else
        /* hash2 goes on longer than hash1 and thus hash1 is lower. */
        return -1;
    } else if (bv == X) {
      /* hash1 goes on longer than hash2 and thus hash1 is greater. */
      return 1;
    } else if (av < bv) {
      /* The first difference shows that hash1 is lower. */
      return -1;
    } else if (av > bv) {
      /* The first difference shows that hash1 is greater. */
      return 1;
    } else {
      ++a;
      ++b;
    }
  }
}

/** Given a list of strings in <b>lst</b>, set the SHA256 digest at
 * <b>digest_out</b> to the hash of the concatenation of those strings, plus
 * the optional string <b>end</b> to be added after each string. */
static void
crypto_digest_smartlist_ends(char *digest_out, const smartlist_t *lst,
                             const char *end)
{
  crypto_digest_t *d = crypto_digest256_new(DIGEST_SHA256);
  SMARTLIST_FOREACH_BEGIN(lst, const char *, cp) {
    crypto_digest_add_bytes(d, cp, strlen(cp));
    if (end)
      crypto_digest_add_bytes(d, end, strlen(end));
  } SMARTLIST_FOREACH_END(cp);
  crypto_digest_get_digest(d, digest_out, DIGEST256_LEN);
  crypto_digest_free(d);
}

/** Generate an ed diff as a smartlist from two consensuses, also given as
 * smartlists. Will return NULL if the diff could not be generated, which can
 * happen if any lines the script had to add matched "." or if the routers
 * were not properly ordered.
 *
 * This implementation is consensus-specific. To generate an ed diff for any
 * given input in quadratic time, you can replace all the code until the
 * navigation in reverse order with the following:
 *
 *   int len1 = smartlist_len(cons1);
 *   int len2 = smartlist_len(cons2);
 *   bitarray_t *changed1 = bitarray_init_zero(len1);
 *   bitarray_t *changed2 = bitarray_init_zero(len2);
 *   cons1_sl = smartlist_slice(cons1, 0, -1);
 *   cons2_sl = smartlist_slice(cons2, 0, -1);
 *   calc_changes(cons1_sl, cons2_sl, changed1, changed2);
 */
static smartlist_t *
gen_ed_diff(smartlist_t *cons1, smartlist_t *cons2)
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

    smartlist_slice_t *cons1_sl, *cons2_sl;

    /* Advance each of the two navigation positions by one router entry if not
     * yet at the end.
     */
    if (i1 < len1) {
      i1 = next_router(cons1, i1);
      if (i1 != len1) {
        last_hash1 = hash1;
        hash1 = get_id_hash(smartlist_get(cons1, i1));
        if (base64cmp(hash1, last_hash1) <= 0) {
          log_warn(LD_CONSDIFF, "Refusing to generate consensus diff because "
              "the base consensus doesn't have its router entries "
              "sorted properly.");
          goto error_cleanup;
        }
      }
    }

    if (i2 < len2) {
      i2 = next_router(cons2, i2);
      if (i2 != len2) {
        last_hash2 = hash2;
        hash2 = get_id_hash(smartlist_get(cons2, i2));
        if (base64cmp(hash2, last_hash2) <= 0) {
          log_warn(LD_CONSDIFF, "Refusing to generate consensus diff because "
              "the target consensus doesn't have its router entries "
              "sorted properly.");
          goto error_cleanup;
        }
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
      int cmp = base64cmp(hash1, hash2);
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
          if (base64cmp(hash1, last_hash1) <= 0) {
            log_warn(LD_CONSDIFF, "Refusing to generate consensus diff "
                "because the base consensus doesn't have its router entries "
                "sorted properly.");
            goto error_cleanup;
          }
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
          if (base64cmp(hash2, last_hash2) <= 0) {
            log_warn(LD_CONSDIFF, "Refusing to generate consensus diff "
                "because the target consensus doesn't have its router entries "
                "sorted properly.");
            goto error_cleanup;
          }
        }
        cmp = base64cmp(hash1, hash2);
      }
    }

    /* Make slices out of these chunks (up to the common router entry) and
     * calculate the changes for them.
     * Error if any of the two slices are longer than 10K lines. That should
     * never happen with any pair of real consensuses. Feeding more than 10K
     * lines to calc_changes would be very slow anyway.
     */
#define MAX_LINE_COUNT (10000)
    if (i1-start1 > MAX_LINE_COUNT || i2-start2 > MAX_LINE_COUNT) {
      log_warn(LD_CONSDIFF, "Refusing to generate consensus diff because "
          "we found too few common router ids.");
      goto error_cleanup;
    }

    cons1_sl = smartlist_slice(cons1, start1, i1);
    cons2_sl = smartlist_slice(cons2, start2, i2);
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

    int start1, start2, end1, end2, added, deleted;

    /* We are at a point were no changed bools are true, so just keep going. */
    if (!(i1 >= 0 && bitarray_is_set(changed1, i1)) &&
        !(i2 >= 0 && bitarray_is_set(changed2, i2))) {
      if (i1 >= 0) i1--;
      if (i2 >= 0) i2--;
      continue;
    }

    end1 = i1, end2 = i2;

    /* Grab all contiguous changed lines */
    while (i1 >= 0 && bitarray_is_set(changed1, i1)) i1--;
    while (i2 >= 0 && bitarray_is_set(changed2, i2)) i2--;

    start1 = i1+1, start2 = i2+1;
    added = end2-i2, deleted = end1-i1;

    if (added == 0) {
      if (deleted == 1) smartlist_add_asprintf(result, "%id", start1+1);
      else smartlist_add_asprintf(result, "%i,%id", start1+1, start1+deleted);

    } else {
      int i;
      if (deleted == 0)
        smartlist_add_asprintf(result, "%ia", start1);
      else if (deleted == 1)
        smartlist_add_asprintf(result, "%ic", start1+1);
      else
        smartlist_add_asprintf(result, "%i,%ic", start1+1, start1+deleted);

      for (i = start2; i <= end2; ++i) {
        const char *line = smartlist_get(cons2, i);
        if (!strcmp(line, ".")) {
          log_warn(LD_CONSDIFF, "Cannot generate consensus diff because "
              "one of the lines to be added is \".\".");
          goto error_cleanup;
        }
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

/** Apply the ed diff to the consensus and return a new consensus, also as a
 * line-based smartlist. Will return NULL if the ed diff is not properly
 * formatted.
 */
static smartlist_t *
apply_ed_diff(smartlist_t *cons1, smartlist_t *diff)
{
  int i, diff_len = smartlist_len(diff);
  int j = smartlist_len(cons1);
  smartlist_t *cons2 = smartlist_new();

  for (i=0; i<diff_len; ++i) {
    const char *diff_line = smartlist_get(diff, i);
    char *endptr1, *endptr2;
    int start, end;
    char action;

    start = (int)strtol(diff_line, &endptr1, 10);
    if (endptr1 == diff_line) {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
          "an ed command was missing a line number.");
      goto error_cleanup;
    }

    /* Two-item range */
    if (*endptr1 == ',') {
        end = (int)strtol(endptr1+1, &endptr2, 10);
        if (endptr2 == endptr1+1) {
          goto error_cleanup;
          log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
              "an ed command was missing a range end line number.");
        }
        /* Incoherent range. */
        if (end <= start) {
          goto error_cleanup;
          log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
              "an invalid range was found in an ed command.");
        }

    /* We'll take <n1> as <n1>,<n1> for simplicity. */
    } else {
        endptr2 = endptr1;
        end = start;
    }

    if (end > j) {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
          "its commands are not properly sorted in reverse order.");
      goto error_cleanup;
    }

    if (*(endptr2+1) != '\0') {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
          "an ed command longer than one char was found.");
      goto error_cleanup;
    }

    action = *endptr2;

    switch (action) {
      case 'a':
      case 'c':
      case 'd':
        break;
      default:
        log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
            "an unrecognised ed command was found.");
        goto error_cleanup;
    }

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
      int added_i, added_end = i;

      i++; /* Skip the line with the range and command. */
      while (i < diff_len) {
        if (!strcmp(smartlist_get(diff, i), ".")) break;
        if (++i == diff_len) {
          log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
              "it has lines to be inserted that don't end with a \".\".");
          goto error_cleanup;
        }
      }

      added_i = i-1;

      /* It would make no sense to add zero new lines. */
      if (added_i == added_end) {
        log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
            "it has an ed command that tries to insert zero lines.");
        goto error_cleanup;
      }

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

/** Generate a consensus diff as a smartlist from two given consensuses, also
 * as smartlists. Will return NULL if the consensus diff could not be
 * generated. Neither of the two consensuses are modified in any way, so it's
 * up to the caller to free their resources.
 */
smartlist_t *
consdiff_gen_diff(smartlist_t *cons1, smartlist_t *cons2)
{
  smartlist_t *ed_diff, *ed_cons2, *result;
  int cons2_eq;
  char cons1_hash[DIGEST256_LEN];
  char cons2_hash[DIGEST256_LEN];
  char cons1_hash_hex[HEX_DIGEST256_LEN+1];
  char cons2_hash_hex[HEX_DIGEST256_LEN+1];

  ed_diff = gen_ed_diff(cons1, cons2);
  /* ed diff could not be generated - reason already logged by gen_ed_diff. */
  if (ed_diff == NULL) return NULL;

  /* See that the script actually produces what we want. */
  ed_cons2 = apply_ed_diff(cons1, ed_diff);
  if (!ed_cons2) {
    log_warn(LD_CONSDIFF, "Refusing to generate consensus diff because "
        "the generated ed diff could not be tested to successfully generate "
        "the target consensus.");
    return NULL;
  }

  cons2_eq = smartlist_strings_eq(cons2, ed_cons2);
  SMARTLIST_FOREACH(ed_cons2, char*, line, tor_free(line));
  smartlist_free(ed_cons2);
  if (!cons2_eq) {
    log_warn(LD_CONSDIFF, "Refusing to generate consensus diff because "
        "the generated ed diff did not generate the target consensus "
        "successfully when tested.");
    return NULL;
  }

  /* Calculate the digests. */
  crypto_digest_smartlist_ends(cons1_hash, cons1, "\n");
  crypto_digest_smartlist_ends(cons2_hash, cons2, "\n");
  base16_encode(cons1_hash_hex, HEX_DIGEST256_LEN+1,
      cons1_hash, DIGEST256_LEN);
  base16_encode(cons2_hash_hex, HEX_DIGEST256_LEN+1,
      cons2_hash, DIGEST256_LEN);

  /* Create the resulting consensus diff. */
  result = smartlist_new();
  smartlist_add_asprintf(result, "network-status-diff-version 1");
  smartlist_add_asprintf(result, "hash %s %s", cons1_hash_hex, cons2_hash_hex);
  smartlist_add_all(result, ed_diff);
  smartlist_free(ed_diff);
  return result;
}

/** Apply the consensus diff to the given consensus and return a new
 * consensus, also as a line-based smartlist. Will return NULL if the diff
 * could not be applied. Neither the consensus nor the diff are modified in
 * any way, so it's up to the caller to free their resources.
 */
smartlist_t *
consdiff_apply_diff(smartlist_t *cons1, smartlist_t *diff)
{
  smartlist_t *hash_words = NULL, *cons2 = NULL, *ed_diff;
  const char *format, *e_cons1_hash_hex, *e_cons2_hash_hex;
  char e_cons1_hash[DIGEST256_LEN];
  char e_cons2_hash[DIGEST256_LEN];
  char cons1_hash[DIGEST256_LEN];
  char cons2_hash[DIGEST256_LEN];
  char cons1_hash_hex[HEX_DIGEST256_LEN+1];
  if (smartlist_len(diff) < 3) {
    log_warn(LD_CONSDIFF, "Refusing to apply consensus diff because "
        "it has too few lines.");
    goto error_cleanup;
  }

  /* Check that it's the format and version we know. */
  format = smartlist_get(diff, 0);
  if (strcmp(format, "network-status-diff-version 1")) {
    log_warn(LD_CONSDIFF, "The consensus diff's format is not known.");
    goto error_cleanup;
  }

  /* Grab the SHA256 base16 hashes. */
  hash_words = smartlist_new();
  smartlist_split_string(hash_words, smartlist_get(diff, 1), " ", 0, 0);

  /* There have to be three words, the first of which must be "hash". */
  if (smartlist_len(hash_words) != 3 ||
      strcmp(smartlist_get(hash_words, 0), "hash")) {
    log_warn(LD_CONSDIFF, "Refusing to apply consensus diff because "
        "it does not include the necessary sha256 digests.");
    goto error_cleanup;
  }

  /* Expected hashes as found in the consensus diff header. They must be of
   * length HEX_DIGEST256_LEN, normally 64 hexadecimal characters.
   * If any of the decodings fail, error to make sure that the hashes are
   * proper base16-encoded SHA256 digests. */
  e_cons1_hash_hex = smartlist_get(hash_words, 1);
  e_cons2_hash_hex = smartlist_get(hash_words, 2);
  if (strlen(e_cons1_hash_hex) != HEX_DIGEST256_LEN ||
      strlen(e_cons2_hash_hex) != HEX_DIGEST256_LEN ||
      base16_decode(e_cons1_hash, DIGEST256_LEN,
          e_cons1_hash_hex, HEX_DIGEST256_LEN) != 0 ||
      base16_decode(e_cons2_hash, DIGEST256_LEN,
          e_cons2_hash_hex, HEX_DIGEST256_LEN) != 0) {
    log_warn(LD_CONSDIFF, "Refusing to apply consensus diff because "
        "the found digests are not proper sha256 digests.");
    goto error_cleanup;
  }

  /* See that the consensus that was given to us matches its hash. */
  crypto_digest_smartlist_ends(cons1_hash, cons1, "\n");
  base16_encode(cons1_hash_hex, HEX_DIGEST256_LEN+1,
      cons1_hash, DIGEST256_LEN);
  if (memcmp(cons1_hash, e_cons1_hash, DIGEST256_LEN*sizeof(char)) != 0) {
    log_warn(LD_CONSDIFF, "Refusing to apply consensus diff because "
        "the base consensus doesn't match its own digest as found in "
        "the consensus diff header.");
    goto error_cleanup;
  }

  /* Grab the ed diff and calculate the resulting consensus. */
  /* To avoid copying memory or iterating over all the elements, make a
   * read-only smartlist without the two header lines. */
  ed_diff = tor_malloc(sizeof(smartlist_t));
  ed_diff->list = diff->list+2;
  ed_diff->num_used = diff->num_used-2;
  ed_diff->capacity = diff->capacity-2;
  cons2 = apply_ed_diff(cons1, ed_diff);
  tor_free(ed_diff);
  /* ed diff could not be applied - reason already logged by apply_ed_diff. */
  if (cons2 == NULL) goto error_cleanup;

  /* See that the resulting consensus matches its hash. */
  crypto_digest_smartlist_ends(cons2_hash, cons2, "\n");
  if (memcmp(cons2_hash, e_cons2_hash, DIGEST256_LEN*sizeof(char)) != 0) {
    log_warn(LD_CONSDIFF, "Refusing to apply consensus diff because "
        "the resulting consensus doesn't match its own digest as found in "
        "the consensus diff header.");
    goto error_cleanup;
  }

  SMARTLIST_FOREACH(hash_words, char *, cp, tor_free(cp));
  smartlist_free(hash_words);
  return cons2;

  error_cleanup:

  if (hash_words) {
    SMARTLIST_FOREACH(hash_words, char *, cp, tor_free(cp));
    smartlist_free(hash_words);
  }
  if (cons2) {
    SMARTLIST_FOREACH(cons2, char *, cp, tor_free(cp));
    smartlist_free(cons2);
  }

  return NULL;
}

