/* Copyright (c) 2014, Daniel Martí
 * Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"
#include "test.h"

#include "consdiff.c"

static void
test_consdiff_smartlist_slice(void *arg)
{
  smartlist_t *sl = smartlist_new();
  smartlist_slice_t *sls;

  /* Create a regular smartlist. */
  (void)arg;
  smartlist_add(sl, (void*)1);
  smartlist_add(sl, (void*)2);
  smartlist_add(sl, (void*)3);
  smartlist_add(sl, (void*)4);
  smartlist_add(sl, (void*)5);

  /* See if the slice was done correctly. */
  sls = smartlist_slice(sl, 2, 5);
  test_eq_ptr(sl, sls->list);
  test_eq_ptr((void*)3, smartlist_get(sls->list, sls->offset));
  test_eq_ptr((void*)5, smartlist_get(sls->list, sls->offset + (sls->len-1)));
  tor_free(sls);

  /* See that using -1 as the end does get to the last element. */
  sls = smartlist_slice(sl, 2, -1);
  test_eq_ptr(sl, sls->list);
  test_eq_ptr((void*)3, smartlist_get(sls->list, sls->offset));
  test_eq_ptr((void*)5, smartlist_get(sls->list, sls->offset + (sls->len-1)));

 done:
  tor_free(sls);
  smartlist_free(sl);
}

static void
test_consdiff_smartlist_slice_string_pos(void *arg)
{
  smartlist_t *sl = smartlist_new();
  smartlist_slice_t *sls;

  /* Create a regular smartlist. */
  (void)arg;
  smartlist_split_string(sl, "a:d:c:a:b", ":", 0, 0);

  /* See that smartlist_slice_string_pos respects the bounds of the slice. */
  sls = smartlist_slice(sl, 2, 5);
  test_eq(3, smartlist_slice_string_pos(sls, "a"));
  test_eq(-1, smartlist_slice_string_pos(sls, "d"));

 done:
  tor_free(sls);
  if (sl) SMARTLIST_FOREACH(sl, char*, line, tor_free(line));
  smartlist_free(sl);
}

static void
test_consdiff_lcs_lengths(void *arg)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  smartlist_slice_t *sls1, *sls2;
  int *lengths1, *lengths2;

  /* Expected lcs lengths in regular and reverse order. */
  int e_lengths1[] = { 0, 1, 2, 3, 3, 4 };
  int e_lengths2[] = { 0, 1, 1, 2, 3, 4 };

  (void)arg;
  smartlist_split_string(sl1, "a:b:c:d:e", ":", 0, 0);
  smartlist_split_string(sl2, "a:c:d:i:e", ":", 0, 0);

  sls1 = smartlist_slice(sl1, 0, -1);
  sls2 = smartlist_slice(sl2, 0, -1);

  lengths1 = lcs_lengths(sls1, sls2, 1);
  lengths2 = lcs_lengths(sls1, sls2, -1);
  test_memeq(e_lengths1, lengths1, sizeof(int) * 6);
  test_memeq(e_lengths2, lengths2, sizeof(int) * 6);

 done:
  tor_free(lengths1);
  tor_free(lengths2);
  tor_free(sls1);
  tor_free(sls2);
  if (sl1) SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  if (sl2) SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
}

static void
test_consdiff_trim_slices(void *arg)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  smartlist_t *sl3 = smartlist_new();
  smartlist_t *sl4 = smartlist_new();
  smartlist_slice_t *sls1, *sls2, *sls3, *sls4;

  (void)arg;
  smartlist_split_string(sl1, "a:b:b:b:d", ":", 0, 0);
  smartlist_split_string(sl2, "a:c:c:c:d", ":", 0, 0);
  smartlist_split_string(sl3, "a:b:b:b:a", ":", 0, 0);
  smartlist_split_string(sl4, "c:b:b:b:c", ":", 0, 0);

  sls1 = smartlist_slice(sl1, 0, -1);
  sls2 = smartlist_slice(sl2, 0, -1);
  sls3 = smartlist_slice(sl3, 0, -1);
  sls4 = smartlist_slice(sl4, 0, -1);

  /* They should be trimmed by one line at each end. */
  test_eq(5, sls1->len);
  test_eq(5, sls2->len);
  trim_slices(sls1, sls2);
  test_eq(3, sls1->len);
  test_eq(3, sls2->len);

  /* They should not be trimmed at all. */
  test_eq(5, sls3->len);
  test_eq(5, sls4->len);
  trim_slices(sls3, sls4);
  test_eq(5, sls3->len);
  test_eq(5, sls4->len);

 done:
  tor_free(sls1);
  tor_free(sls2);
  tor_free(sls3);
  tor_free(sls4);
  if (sl1) SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  if (sl2) SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  if (sl3) SMARTLIST_FOREACH(sl3, char*, line, tor_free(line));
  if (sl4) SMARTLIST_FOREACH(sl4, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
  smartlist_free(sl3);
  smartlist_free(sl4);
}

static void
test_consdiff_set_changed(void *arg)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  bitarray_t *changed1 = bitarray_init_zero(4);
  bitarray_t *changed2 = bitarray_init_zero(4);
  smartlist_slice_t *sls1, *sls2;

  (void)arg;
  smartlist_split_string(sl1, "a:b:a:a", ":", 0, 0);
  smartlist_split_string(sl2, "a:a:a:a", ":", 0, 0);

  /* Length of sls1 is 0. */
  sls1 = smartlist_slice(sl1, 0, 0);
  sls2 = smartlist_slice(sl2, 1, 3);
  set_changed(changed1, changed2, sls1, sls2);

  /* The former is not changed, the latter changes all of its elements. */
  test_assert(!bitarray_is_set(changed1, 0));
  test_assert(!bitarray_is_set(changed1, 1));
  test_assert(!bitarray_is_set(changed1, 2));
  test_assert(!bitarray_is_set(changed1, 3));

  test_assert(!bitarray_is_set(changed2, 0));
  test_assert(bitarray_is_set(changed2, 1));
  test_assert(bitarray_is_set(changed2, 2));
  test_assert(!bitarray_is_set(changed2, 3));
  bitarray_clear(changed2, 1);
  bitarray_clear(changed2, 2);

  /* Length of sls1 is 1 and its element is in sls2. */
  tor_free(sls1);
  sls1 = smartlist_slice(sl1, 0, 1);
  set_changed(changed1, changed2, sls1, sls2);

  /* The latter changes all elements but the (first) common one. */
  test_assert(!bitarray_is_set(changed1, 0));
  test_assert(!bitarray_is_set(changed1, 1));
  test_assert(!bitarray_is_set(changed1, 2));
  test_assert(!bitarray_is_set(changed1, 3));

  test_assert(!bitarray_is_set(changed2, 0));
  test_assert(!bitarray_is_set(changed2, 1));
  test_assert(bitarray_is_set(changed2, 2));
  test_assert(!bitarray_is_set(changed2, 3));
  bitarray_clear(changed2, 2);

  /* Length of sls1 is 1 and its element is not in sls2. */
  tor_free(sls1);
  sls1 = smartlist_slice(sl1, 1, 2);
  set_changed(changed1, changed2, sls1, sls2);

  /* The former changes its element, the latter changes all elements. */
  test_assert(!bitarray_is_set(changed1, 0));
  test_assert(bitarray_is_set(changed1, 1));
  test_assert(!bitarray_is_set(changed1, 2));
  test_assert(!bitarray_is_set(changed1, 3));

  test_assert(!bitarray_is_set(changed2, 0));
  test_assert(bitarray_is_set(changed2, 1));
  test_assert(bitarray_is_set(changed2, 2));
  test_assert(!bitarray_is_set(changed2, 3));

 done:
  bitarray_free(changed1);
  bitarray_free(changed2);
  if (sl1) SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  if (sl2) SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
  tor_free(sls1);
  tor_free(sls2);
}

static void
test_consdiff_calc_changes(void *arg)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  smartlist_slice_t *sls1, *sls2;
  bitarray_t *changed1 = bitarray_init_zero(4);
  bitarray_t *changed2 = bitarray_init_zero(4);

  (void)arg;
  smartlist_split_string(sl1, "a:a:a:a", ":", 0, 0);
  smartlist_split_string(sl2, "a:a:a:a", ":", 0, 0);

  sls1 = smartlist_slice(sl1, 0, -1);
  sls2 = smartlist_slice(sl2, 0, -1);
  calc_changes(sls1, sls2, changed1, changed2);

  /* Nothing should be set to changed. */
  test_assert(!bitarray_is_set(changed1, 0));
  test_assert(!bitarray_is_set(changed1, 1));
  test_assert(!bitarray_is_set(changed1, 2));
  test_assert(!bitarray_is_set(changed1, 3));

  test_assert(!bitarray_is_set(changed2, 0));
  test_assert(!bitarray_is_set(changed2, 1));
  test_assert(!bitarray_is_set(changed2, 2));
  test_assert(!bitarray_is_set(changed2, 3));

  SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_clear(sl2);
  smartlist_split_string(sl2, "a:b:a:b", ":", 0, 0);
  tor_free(sls1);
  tor_free(sls2);
  sls1 = smartlist_slice(sl1, 0, -1);
  sls2 = smartlist_slice(sl2, 0, -1);
  calc_changes(sls1, sls2, changed1, changed2);

  /* Two elements are changed. */
  test_assert(!bitarray_is_set(changed1, 0));
  test_assert(bitarray_is_set(changed1, 1));
  test_assert(bitarray_is_set(changed1, 2));
  test_assert(!bitarray_is_set(changed1, 3));
  bitarray_clear(changed1, 1);
  bitarray_clear(changed1, 2);

  test_assert(!bitarray_is_set(changed2, 0));
  test_assert(bitarray_is_set(changed2, 1));
  test_assert(!bitarray_is_set(changed2, 2));
  test_assert(bitarray_is_set(changed2, 3));
  bitarray_clear(changed1, 1);
  bitarray_clear(changed1, 3);

  SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_clear(sl2);
  smartlist_split_string(sl2, "b:b:b:b", ":", 0, 0);
  tor_free(sls1);
  tor_free(sls2);
  sls1 = smartlist_slice(sl1, 0, -1);
  sls2 = smartlist_slice(sl2, 0, -1);
  calc_changes(sls1, sls2, changed1, changed2);

  /* All elements are changed. */
  test_assert(bitarray_is_set(changed1, 0));
  test_assert(bitarray_is_set(changed1, 1));
  test_assert(bitarray_is_set(changed1, 2));
  test_assert(bitarray_is_set(changed1, 3));

  test_assert(bitarray_is_set(changed2, 0));
  test_assert(bitarray_is_set(changed2, 1));
  test_assert(bitarray_is_set(changed2, 2));
  test_assert(bitarray_is_set(changed2, 3));

 done:
  bitarray_free(changed1);
  bitarray_free(changed2);
  if (sl1) SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  if (sl2) SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
  tor_free(sls1);
  tor_free(sls2);
}

static void
test_consdiff_get_id_hash(void *arg)
{
  const char *line, *e_hash;
  /* No hash. */
  (void)arg;
  test_eq_ptr(NULL, get_id_hash("r name"));
  /* The hash contains characters that are not base64. */
  test_eq_ptr(NULL, get_id_hash( "r name _hash_isnt_base64 etc"));

  line = "r name hash+valid+base64 etc";
  e_hash = line+7;
  test_eq_ptr(e_hash, get_id_hash(line));

 done:
  ;
}

static void
test_consdiff_is_valid_router_entry(void *arg)
{
  /* Doesn't start with "r ". */
  (void)arg;
  test_eq(0, is_valid_router_entry("foo"));

  /* These are already tested with get_id_hash, but make sure it's run
   * properly. */

  test_eq(0, is_valid_router_entry("r name"));
  test_eq(0, is_valid_router_entry("r name _hash_isnt_base64 etc"));
  test_eq(1, is_valid_router_entry("r name hash+valid+base64 etc"));

 done:
  ;
}

static void
test_consdiff_next_router(void *arg)
{
  smartlist_t *sl = smartlist_new();
  (void)arg;
  smartlist_add(sl, (char*)"foo");
  smartlist_add(sl,
      (char*)"r name hash+longer+than+27+chars+and+valid+base64 etc");
  smartlist_add(sl, (char*)"foo");
  smartlist_add(sl, (char*)"foo");
  smartlist_add(sl,
      (char*)"r name hash+longer+than+27+chars+and+valid+base64 etc");
  smartlist_add(sl, (char*)"foo");

  /* Not currently on a router entry line, finding the next one. */
  test_eq(1, next_router(sl, 0));
  test_eq(4, next_router(sl, 2));

  /* Already at the beginning of a router entry line, ignore it. */
  test_eq(4, next_router(sl, 1));

  /* There are no more router entries, so return the line after the last. */
  test_eq(6, next_router(sl, 4));
  test_eq(6, next_router(sl, 5));

 done:
  smartlist_free(sl);
}

static void
test_consdiff_base64cmp(void *arg)
{
  /* NULL arguments. */
  (void)arg;
  test_eq(0, base64cmp(NULL, NULL));
  test_eq(-1, base64cmp(NULL, "foo"));
  test_eq(1, base64cmp("bar", NULL));

  /* Nil base64 values. */
  test_eq(0, base64cmp("", ""));
  test_eq(0, base64cmp("_", "&"));

  /* Exact same valid strings. */
  test_eq(0, base64cmp("abcABC/+", "abcABC/+"));
  /* Both end with an invalid base64 char other than '\0'. */
  test_eq(0, base64cmp("abcABC/+ ", "abcABC/+ "));
  /* Only one ends with an invalid base64 char other than '\0'. */
  test_eq(0, base64cmp("abcABC/+ ", "abcABC/+"));

  /* Comparisons that would return differently with strcmp(). */
  test_eq(-1, strcmp("/foo", "Afoo"));
  test_eq(1, base64cmp("/foo", "Afoo"));
  test_eq(1, strcmp("Afoo", "0foo"));
  test_eq(-1, base64cmp("Afoo", "0foo"));

  /* Comparisons that would return the same as with strcmp(). */
  test_eq(1, strcmp("afoo", "Afoo"));
  test_eq(1, base64cmp("afoo", "Afoo"));

 done:
  ;
}

static void
test_consdiff_gen_ed_diff(void *arg)
{
  smartlist_t *cons1=NULL, *cons2=NULL, *diff=NULL;
  int i;
  (void)arg;
  cons1 = smartlist_new();
  cons2 = smartlist_new();

  /* Identity hashes are not sorted properly, return NULL. */
  smartlist_add(cons1, (char*)"r name bbbbbbbbbbbbbbbbbbbbbbbbbbb etc");
  smartlist_add(cons1, (char*)"foo");
  smartlist_add(cons1, (char*)"r name aaaaaaaaaaaaaaaaaaaaaaaaaaa etc");
  smartlist_add(cons1, (char*)"bar");

  smartlist_add(cons2, (char*)"r name aaaaaaaaaaaaaaaaaaaaaaaaaaa etc");
  smartlist_add(cons2, (char*)"foo");
  smartlist_add(cons2, (char*)"r name ccccccccccccccccccccccccccc etc");
  smartlist_add(cons2, (char*)"bar");

  diff = gen_ed_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

  /* Same, but now with the second consensus. */
  diff = gen_ed_diff(cons2, cons1);
  test_eq_ptr(NULL, diff);

  /* Identity hashes are repeated, return NULL. */
  smartlist_clear(cons1);

  smartlist_add(cons1, (char*)"r name bbbbbbbbbbbbbbbbbbbbbbbbbbb etc");
  smartlist_add(cons1, (char*)"foo");
  smartlist_add(cons1, (char*)"r name bbbbbbbbbbbbbbbbbbbbbbbbbbb etc");
  smartlist_add(cons1, (char*)"bar");

  diff = gen_ed_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

  /* We have to add a line that is just a dot, return NULL. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);

  smartlist_add(cons1, (char*)"foo1");
  smartlist_add(cons1, (char*)"foo2");

  smartlist_add(cons2, (char*)"foo1");
  smartlist_add(cons2, (char*)".");
  smartlist_add(cons2, (char*)"foo2");

  diff = gen_ed_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

#define MAX_LINE_COUNT (10000)
  /* Too many lines to be fed to the quadratic-time function. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);

  for (i=0; i < MAX_LINE_COUNT; ++i) smartlist_add(cons1, (char*)"a");
  for (i=0; i < MAX_LINE_COUNT; ++i) smartlist_add(cons1, (char*)"b");

  diff = gen_ed_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

  /* We have dot lines, but they don't interfere with the script format. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);

  smartlist_add(cons1, (char*)"foo1");
  smartlist_add(cons1, (char*)".");
  smartlist_add(cons1, (char*)".");
  smartlist_add(cons1, (char*)"foo2");

  smartlist_add(cons2, (char*)"foo1");
  smartlist_add(cons2, (char*)".");
  smartlist_add(cons2, (char*)"foo2");

  diff = gen_ed_diff(cons1, cons2);
  test_neq_ptr(NULL, diff);
  SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_free(diff);

  /* Empty diff tests. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);

  diff = gen_ed_diff(cons1, cons2);
  test_neq_ptr(NULL, diff);
  test_eq(0, smartlist_len(diff));
  smartlist_free(diff);

  smartlist_add(cons1, (char*)"foo");
  smartlist_add(cons1, (char*)"bar");

  smartlist_add(cons2, (char*)"foo");
  smartlist_add(cons2, (char*)"bar");

  diff = gen_ed_diff(cons1, cons2);
  test_neq_ptr(NULL, diff);
  test_eq(0, smartlist_len(diff));
  smartlist_free(diff);

  /* Everything is deleted. */
  smartlist_clear(cons2);

  diff = gen_ed_diff(cons1, cons2);
  test_neq_ptr(NULL, diff);
  test_eq(1, smartlist_len(diff));
  test_streq("1,2d", smartlist_get(diff, 0));

  SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_free(diff);

  /* Everything is added. */
  diff = gen_ed_diff(cons2, cons1);
  test_neq_ptr(NULL, diff);
  test_eq(4, smartlist_len(diff));
  test_streq("0a", smartlist_get(diff, 0));
  test_streq("foo", smartlist_get(diff, 1));
  test_streq("bar", smartlist_get(diff, 2));
  test_streq(".", smartlist_get(diff, 3));

  SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_free(diff);

  /* Everything is changed. */
  smartlist_add(cons2, (char*)"foo2");
  smartlist_add(cons2, (char*)"bar2");
  diff = gen_ed_diff(cons1, cons2);
  test_neq_ptr(NULL, diff);
  test_eq(4, smartlist_len(diff));
  test_streq("1,2c", smartlist_get(diff, 0));
  test_streq("foo2", smartlist_get(diff, 1));
  test_streq("bar2", smartlist_get(diff, 2));
  test_streq(".", smartlist_get(diff, 3));

  SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_free(diff);

  /* Test 'a', 'c' and 'd' together. See that it is done in reverse order. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);
  smartlist_split_string(cons1, "A:B:C:D:E", ":", 0, 0);
  smartlist_split_string(cons2, "A:C:O:E:U", ":", 0, 0);
  diff = gen_ed_diff(cons1, cons2);
  test_neq_ptr(NULL, diff);
  test_eq(7, smartlist_len(diff));
  test_streq("5a", smartlist_get(diff, 0));
  test_streq("U", smartlist_get(diff, 1));
  test_streq(".", smartlist_get(diff, 2));
  test_streq("4c", smartlist_get(diff, 3));
  test_streq("O", smartlist_get(diff, 4));
  test_streq(".", smartlist_get(diff, 5));
  test_streq("2d", smartlist_get(diff, 6));

  /* TODO: small real use-cases, i.e. consensuses. */

 done:
  if (cons1) SMARTLIST_FOREACH(cons1, char*, line, tor_free(line));
  if (cons2) SMARTLIST_FOREACH(cons2, char*, line, tor_free(line));
  smartlist_free(cons1);
  smartlist_free(cons2);
  if (diff) SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_free(diff);
}

static void
test_consdiff_apply_ed_diff(void *arg)
{
  smartlist_t *cons1=NULL, *cons2=NULL, *diff=NULL;
  (void)arg;
  cons1 = smartlist_new();
  diff = smartlist_new();

  smartlist_split_string(cons1, "A:B:C:D:E", ":", 0, 0);

  /* Command without range. */
  smartlist_add(diff, (char*)"a");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Range without command. */
  smartlist_add(diff, (char*)"1");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Range without end. */
  smartlist_add(diff, (char*)"1,");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Incoherent ranges. */
  smartlist_add(diff, (char*)"1,1");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  smartlist_add(diff, (char*)"3,2");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Script is not in reverse order. */
  smartlist_add(diff, (char*)"1d");
  smartlist_add(diff, (char*)"3d");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Script contains unrecognised commands longer than one char. */
  smartlist_add(diff, (char*)"1foo");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Script contains unrecognised commands. */
  smartlist_add(diff, (char*)"1e");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Command that should be followed by at least one line and a ".", but
   * isn't. */
  smartlist_add(diff, (char*)"0a");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* Now it is followed by a ".", but it inserts zero lines. */
  smartlist_add(diff, (char*)".");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Test appending text, 'a'. */
  smartlist_split_string(diff, "3a:U:O:.:0a:V:.", ":", 0, 0);
  cons2 = apply_ed_diff(cons1, diff);
  test_neq_ptr(NULL, cons2);
  test_eq(8, smartlist_len(cons2));
  test_streq("V", smartlist_get(cons2, 0));
  test_streq("A", smartlist_get(cons2, 1));
  test_streq("B", smartlist_get(cons2, 2));
  test_streq("C", smartlist_get(cons2, 3));
  test_streq("U", smartlist_get(cons2, 4));
  test_streq("O", smartlist_get(cons2, 5));
  test_streq("D", smartlist_get(cons2, 6));
  test_streq("E", smartlist_get(cons2, 7));

  SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_clear(diff);
  SMARTLIST_FOREACH(cons2, char*, line, tor_free(line));
  smartlist_free(cons2);

  /* Test deleting text, 'd'. */
  smartlist_split_string(diff, "4d:1,2d", ":", 0, 0);
  cons2 = apply_ed_diff(cons1, diff);
  test_neq_ptr(NULL, cons2);
  test_eq(2, smartlist_len(cons2));
  test_streq("C", smartlist_get(cons2, 0));
  test_streq("E", smartlist_get(cons2, 1));

  SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_clear(diff);
  SMARTLIST_FOREACH(cons2, char*, line, tor_free(line));
  smartlist_free(cons2);

  /* Test changing text, 'c'. */
  smartlist_split_string(diff, "4c:T:X:.:1,2c:M:.", ":", 0, 0);
  cons2 = apply_ed_diff(cons1, diff);
  test_neq_ptr(NULL, cons2);
  test_eq(5, smartlist_len(cons2));
  test_streq("M", smartlist_get(cons2, 0));
  test_streq("C", smartlist_get(cons2, 1));
  test_streq("T", smartlist_get(cons2, 2));
  test_streq("X", smartlist_get(cons2, 3));
  test_streq("E", smartlist_get(cons2, 4));

  SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_clear(diff);
  SMARTLIST_FOREACH(cons2, char*, line, tor_free(line));
  smartlist_free(cons2);

  /* Test 'a', 'd' and 'c' together. */
  smartlist_split_string(diff, "4c:T:X:.:2d:0a:M:.", ":", 0, 0);
  cons2 = apply_ed_diff(cons1, diff);
  test_neq_ptr(NULL, cons2);
  test_eq(6, smartlist_len(cons2));
  test_streq("M", smartlist_get(cons2, 0));
  test_streq("A", smartlist_get(cons2, 1));
  test_streq("C", smartlist_get(cons2, 2));
  test_streq("T", smartlist_get(cons2, 3));
  test_streq("X", smartlist_get(cons2, 4));
  test_streq("E", smartlist_get(cons2, 5));

 done:
  if (cons1) SMARTLIST_FOREACH(cons1, char*, line, tor_free(line));
  if (cons2) SMARTLIST_FOREACH(cons2, char*, line, tor_free(line));
  smartlist_free(cons1);
  smartlist_free(cons2);
  if (diff) SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_free(diff);
}

static void
test_consdiff_gen_diff(void *arg)
{
  char *cons1_str=NULL, *cons2_str=NULL;
  smartlist_t *cons1=NULL, *cons2=NULL, *diff=NULL;
  digests_t digests1, digests2;
  (void)arg;
  cons1 = smartlist_new();
  cons2 = smartlist_new();

  /* Identity hashes are not sorted properly, return NULL.
   * Already tested in gen_ed_diff, but see that a NULL ed diff also makes
   * gen_diff return NULL. */
  cons1_str = tor_strdup(
      "header\nnetwork-status-version foo\n"
      "r name bbbbbbbbbbbbbbbbb etc\nfoo\n"
      "r name aaaaaaaaaaaaaaaaa etc\nbar\n"
      "directory-signature foo bar\nbar\n"
      );
  cons2_str = tor_strdup(
      "header\nnetwork-status-version foo\n"
      "r name aaaaaaaaaaaaaaaaa etc\nfoo\n"
      "r name ccccccccccccccccc etc\nbar\n"
      "directory-signature foo bar\nbar\n"
      );

  test_eq(0, router_get_networkstatus_v3_hashes(cons1_str, &digests1));
  test_eq(0, router_get_networkstatus_v3_hashes(cons2_str, &digests2));

  tor_split_lines(cons1, cons1_str, (int)strlen(cons1_str));
  tor_split_lines(cons2, cons2_str, (int)strlen(cons2_str));

  diff = consdiff_gen_diff(cons1, cons2, &digests1, &digests2);
  test_eq_ptr(NULL, diff);

  /* Check that the headers are done properly. */
  tor_free(cons1_str);
  cons1_str = tor_strdup(
      "header\nnetwork-status-version foo\n"
      "r name ccccccccccccccccc etc\nfoo\n"
      "r name eeeeeeeeeeeeeeeee etc\nbar\n"
      "directory-signature foo bar\nbar\n"
      );
  test_eq(0, router_get_networkstatus_v3_hashes(cons1_str, &digests1));
  smartlist_clear(cons1);
  tor_split_lines(cons1, cons1_str, (int)strlen(cons1_str));
  diff = consdiff_gen_diff(cons1, cons2, &digests1, &digests2);
  test_neq_ptr(NULL, diff);
  test_eq(7, smartlist_len(diff));
  test_streq("network-status-diff-version 1", smartlist_get(diff, 0));
  test_streq("hash "
      "C2199B6827514F39ED9B3F2E2E73735C6C5468FD636240BB454C526220DE702A "
      "B193E5FBFE5C009AEDE56F9218E6421A1AE5C19F43E091786A73F43F60409B60",
      smartlist_get(diff, 1));
  test_streq("4,5d", smartlist_get(diff, 2));
  test_streq("2a", smartlist_get(diff, 3));
  test_streq("r name aaaaaaaaaaaaaaaaa etc", smartlist_get(diff, 4));
  test_streq("foo", smartlist_get(diff, 5));
  test_streq(".", smartlist_get(diff, 6));

  /* TODO: small real use-cases, i.e. consensuses. */

 done:
  tor_free(cons1_str);
  tor_free(cons2_str);
  smartlist_free(cons1);
  smartlist_free(cons2);
  if (diff) SMARTLIST_FOREACH(diff, char*, line, tor_free(line));
  smartlist_free(diff);
}

static void
test_consdiff_apply_diff(void *arg)
{
  smartlist_t *cons1=NULL, *diff=NULL;
  char *cons1_str=NULL, *cons2 = NULL;
  digests_t digests1;
  (void)arg;
  cons1 = smartlist_new();
  diff = smartlist_new();

  cons1_str = tor_strdup(
      "header\nnetwork-status-version foo\n"
      "r name ccccccccccccccccc etc\nfoo\n"
      "r name eeeeeeeeeeeeeeeee etc\nbar\n"
      "directory-signature foo bar\nbar\n"
      );
  test_eq(0, router_get_networkstatus_v3_hashes(cons1_str, &digests1));
  tor_split_lines(cons1, cons1_str, (int)strlen(cons1_str));

  /* diff doesn't have enough lines. */
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* first line doesn't match format-version string. */
  smartlist_add(diff, (char*)"foo-bar");
  smartlist_add(diff, (char*)"header-line");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* The first word of the second header line is not "hash". */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"word a b");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* Wrong number of words after "hash". */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash a b c");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* base16 sha256 digests do not have the expected length. */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash aaa bbb");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* base16 sha256 digests contain non-base16 characters. */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash"
      " ????????????????????????????????????????????????????????????????"
      " ----------------------------------------------------------------");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* Invalid ed diff.
   * As tested in apply_ed_diff, but check that apply_diff does return NULL if
   * the ed diff can't be applied. */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash"
      /* sha256 of cons1. */
      " C2199B6827514F39ED9B3F2E2E73735C6C5468FD636240BB454C526220DE702A"
      /* sha256 of cons2. */
      " 635D34593020C08E5ECD865F9986E29D50028EFA62843766A8197AD228A7F6AA");
  smartlist_add(diff, (char*)"foobar");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* Base consensus doesn't match its digest as found in the diff. */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash"
      /* bogus sha256. */
      " 3333333333333333333333333333333333333333333333333333333333333333"
      /* sha256 of cons2. */
      " 635D34593020C08E5ECD865F9986E29D50028EFA62843766A8197AD228A7F6AA");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* Resulting consensus doesn't match its digest as found in the diff. */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash"
      /* sha256 of cons1. */
      " C2199B6827514F39ED9B3F2E2E73735C6C5468FD636240BB454C526220DE702A"
      /* bogus sha256. */
      " 3333333333333333333333333333333333333333333333333333333333333333");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_eq_ptr(NULL, cons2);

  /* Very simple test, only to see that nothing errors. */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash"
      /* sha256 of cons1. */
      " C2199B6827514F39ED9B3F2E2E73735C6C5468FD636240BB454C526220DE702A"
      /* sha256 of cons2. */
      " 635D34593020C08E5ECD865F9986E29D50028EFA62843766A8197AD228A7F6AA");
  smartlist_add(diff, (char*)"4c");
  smartlist_add(diff, (char*)"sample");
  smartlist_add(diff, (char*)".");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_neq_ptr(NULL, cons2);
  test_streq(
      "header\nnetwork-status-version foo\n"
      "r name ccccccccccccccccc etc\nsample\n"
      "r name eeeeeeeeeeeeeeeee etc\nbar\n"
      "directory-signature foo bar\nbar\n",
      cons2);
  tor_free(cons2);

  /* Check that lowercase letters in base16-encoded digests work too. */
  smartlist_clear(diff);
  smartlist_add(diff, (char*)"network-status-diff-version 1");
  smartlist_add(diff, (char*)"hash"
      /* sha256 of cons1. */
      " c2199b6827514f39ed9b3f2e2e73735c6c5468fd636240bb454c526220de702a"
      /* sha256 of cons2. */
      " 635d34593020c08e5ecd865f9986e29d50028efa62843766a8197ad228a7f6aa");
  smartlist_add(diff, (char*)"4c");
  smartlist_add(diff, (char*)"sample");
  smartlist_add(diff, (char*)".");
  cons2 = consdiff_apply_diff(cons1, diff, &digests1);
  test_neq_ptr(NULL, cons2);
  test_streq(
      "header\nnetwork-status-version foo\n"
      "r name ccccccccccccccccc etc\nsample\n"
      "r name eeeeeeeeeeeeeeeee etc\nbar\n"
      "directory-signature foo bar\nbar\n",
      cons2);
  tor_free(cons2);

  smartlist_clear(diff);

 done:
  tor_free(cons1_str);
  smartlist_free(cons1);
  smartlist_free(diff);
}

#define CONSDIFF_LEGACY(name)                                          \
  { #name, test_consdiff_ ## name , 0, NULL, NULL }

struct testcase_t consdiff_tests[] = {
  CONSDIFF_LEGACY(smartlist_slice),
  CONSDIFF_LEGACY(smartlist_slice_string_pos),
  CONSDIFF_LEGACY(lcs_lengths),
  CONSDIFF_LEGACY(trim_slices),
  CONSDIFF_LEGACY(set_changed),
  CONSDIFF_LEGACY(calc_changes),
  CONSDIFF_LEGACY(get_id_hash),
  CONSDIFF_LEGACY(is_valid_router_entry),
  CONSDIFF_LEGACY(next_router),
  CONSDIFF_LEGACY(base64cmp),
  CONSDIFF_LEGACY(gen_ed_diff),
  CONSDIFF_LEGACY(apply_ed_diff),
  CONSDIFF_LEGACY(gen_diff),
  CONSDIFF_LEGACY(apply_diff),
  END_OF_TESTCASES
};

