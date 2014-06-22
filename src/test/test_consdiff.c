#include "orconfig.h"
#include "or.h"
#include "test.h"

static void
test_consdiff_smartlist_slice(void)
{
  /* Create a regular smartlist. */
  smartlist_t *sl = smartlist_new();
  smartlist_add(sl, (void*)1);
  smartlist_add(sl, (void*)2);
  smartlist_add(sl, (void*)3);
  smartlist_add(sl, (void*)4);
  smartlist_add(sl, (void*)5);

  smartlist_slice_t *sls;

  /* See if the slice was done correctly. */
  sls = smartlist_slice(sl, 2, 3);
  test_eq_ptr(sl, sls->list);
  test_eq_ptr((void*)3, smartlist_get(sls->list, sls->offset));
  test_eq_ptr((void*)5, smartlist_get(sls->list, sls->offset + (sls->len-1)));

 done:
  tor_free(sls);
  smartlist_free(sl);
}

static void
test_consdiff_smartlist_slice_string_pos(void)
{
  /* Create a regular smartlist. */
  smartlist_t *sl = smartlist_new();
  smartlist_split_string(sl, "a:d:c:a:b", ":", 0, 0);

  /* See that smartlist_slice_string_pos respects the bounds of the slice. */
  smartlist_slice_t *sls = smartlist_slice(sl, 2, 3);
  test_eq(3, smartlist_slice_string_pos(sls, "a"));
  test_eq(-1, smartlist_slice_string_pos(sls, "d"));

 done:
  tor_free(sls);
  SMARTLIST_FOREACH(sl, char*, line, tor_free(line));
  smartlist_free(sl);
}

static void
test_consdiff_lcs_lens(void)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();

  smartlist_split_string(sl1, "a:b:c:d:e", ":", 0, 0);
  smartlist_split_string(sl2, "a:c:d:i:e", ":", 0, 0);

  smartlist_slice_t *sls1 = smartlist_slice(sl1, 0, smartlist_len(sl1));
  smartlist_slice_t *sls2 = smartlist_slice(sl2, 0, smartlist_len(sl2));
  int *lens1, *lens2;

  /* Expected lcs lengths in regular and reverse order. */
  int e_lens1[] = { 0, 1, 2, 3, 3, 4 };
  int e_lens2[] = { 0, 1, 1, 2, 3, 4 };

  lens1 = lcs_lens(sls1, sls2, 1);
  lens2 = lcs_lens(sls1, sls2, -1);
  test_memeq(e_lens1, lens1, sizeof(int) * 6);
  test_memeq(e_lens2, lens2, sizeof(int) * 6);

 done:
  tor_free(lens1);
  tor_free(lens2);
  tor_free(sls1);
  tor_free(sls2);
  SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
}

static void
test_consdiff_trim_slices(void)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  smartlist_t *sl3 = smartlist_new();
  smartlist_t *sl4 = smartlist_new();

  smartlist_split_string(sl1, "a:b:b:b:d", ":", 0, 0);
  smartlist_split_string(sl2, "a:c:c:c:d", ":", 0, 0);
  smartlist_split_string(sl3, "a:b:b:b:a", ":", 0, 0);
  smartlist_split_string(sl4, "c:b:b:b:c", ":", 0, 0);
  
  smartlist_slice_t *sls1 = smartlist_slice(sl1, 0, smartlist_len(sl1));
  smartlist_slice_t *sls2 = smartlist_slice(sl2, 0, smartlist_len(sl2));
  smartlist_slice_t *sls3 = smartlist_slice(sl3, 0, smartlist_len(sl3));
  smartlist_slice_t *sls4 = smartlist_slice(sl4, 0, smartlist_len(sl4));

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
  SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  SMARTLIST_FOREACH(sl3, char*, line, tor_free(line));
  SMARTLIST_FOREACH(sl4, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
  smartlist_free(sl3);
  smartlist_free(sl4);
}

static void
test_consdiff_set_changed(void)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  smartlist_split_string(sl1, "a:b:a:a", ":", 0, 0);
  smartlist_split_string(sl2, "a:a:a:a", ":", 0, 0);
  bitarray_t *changed1 = bitarray_init_zero(4);
  bitarray_t *changed2 = bitarray_init_zero(4);
  smartlist_slice_t *sls1, *sls2;

  /* Length of sls1 is 0. */
  sls1 = smartlist_slice(sl1, 0, 0);
  sls2 = smartlist_slice(sl2, 1, 2);
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
  sls1 = smartlist_slice(sl1, 1, 1);
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
  SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
  tor_free(sls1);
  tor_free(sls2);
}

static void
test_consdiff_calc_changes(void)
{
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  smartlist_split_string(sl1, "a:a:a:a", ":", 0, 0);
  smartlist_split_string(sl2, "a:a:a:a", ":", 0, 0);
  bitarray_t *changed1 = bitarray_init_zero(4);
  bitarray_t *changed2 = bitarray_init_zero(4);
  smartlist_slice_t *sls1, *sls2;

  sls1 = smartlist_slice(sl1, 0, 4);
  sls2 = smartlist_slice(sl2, 0, 4);
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
  sls1 = smartlist_slice(sl1, 0, 4);
  sls2 = smartlist_slice(sl2, 0, 4);
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
  sls1 = smartlist_slice(sl1, 0, 4);
  sls2 = smartlist_slice(sl2, 0, 4);
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
  SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
  smartlist_free(sl1);
  smartlist_free(sl2);
  tor_free(sls1);
  tor_free(sls2);
}

static void
test_consdiff_get_id_hash(void)
{
  /* No hash. */
  test_eq_ptr(NULL, get_id_hash("r name"));
  /* The hash is too short. */
  test_eq_ptr(NULL, get_id_hash("r name hash etc"));
  /* The hash contains characters that are not base64. */
  test_eq_ptr(NULL, get_id_hash("r name hash_longer_than_27_chars_but_isnt_base64 etc"));

  char *line = "r name hash+longer+than+27+chars+and+valid+base64 etc";
  char *e_hash = line+7;
  test_eq_ptr(e_hash, get_id_hash(line));

 done:
  ;
}

static void
test_consdiff_is_valid_router_entry(void)
{
  /* Doesn't start with "r ". */
  test_eq(0, is_valid_router_entry("foo"));

  /* These are already tested with get_id_hash, but make sure it's run
   * properly. */

  test_eq(0, is_valid_router_entry("r name"));
  test_eq(0, is_valid_router_entry("r name hash etc"));
  test_eq(0, is_valid_router_entry("r name hash_longer_than_27_chars_but_isnt_base64 etc"));

  test_eq(1, is_valid_router_entry("r name hash+longer+than+27+chars+and+valid+base64 etc"));

 done:
  ;
}

static void
test_consdiff_next_router(void)
{
  smartlist_t *sl = smartlist_new();
  smartlist_add(sl, "foo");
  smartlist_add(sl, "r name hash+longer+than+27+chars+and+valid+base64 etc");
  smartlist_add(sl, "foo");
  smartlist_add(sl, "foo");
  smartlist_add(sl, "r name hash+longer+than+27+chars+and+valid+base64 etc");
  smartlist_add(sl, "foo");

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
test_consdiff_hashcmp(void)
{
  /* NULL arguments. */
  test_eq(0, hashcmp(NULL, NULL));
  test_eq(-1, hashcmp(NULL, "foo"));
  test_eq(1, hashcmp("bar", NULL));

  /* Nil base64 values. */
  test_eq(0, hashcmp("", ""));
  test_eq(0, hashcmp("_", "&"));

  /* Exact same valid strings. */
  test_eq(0, hashcmp("abcABC/+", "abcABC/+"));
  /* Both end with an invalid base64 char other than '\0'. */
  test_eq(0, hashcmp("abcABC/+ ", "abcABC/+ "));
  /* Only one ends with an invalid base64 char other than '\0'. */
  test_eq(0, hashcmp("abcABC/+ ", "abcABC/+"));

  /* Comparisons that would return differently with strcmp(). */
  test_eq(-1, strcmp("/foo", "Afoo"));
  test_eq(1, hashcmp("/foo", "Afoo"));
  test_eq(1, strcmp("Afoo", "0foo"));
  test_eq(-1, hashcmp("Afoo", "0foo"));

  /* Comparisons that would return the same as with strcmp(). */
  test_eq(1, strcmp("afoo", "Afoo"));
  test_eq(1, hashcmp("afoo", "Afoo"));

 done:
  ;
}

struct testcase_t consdiff_tests[] = {
  END_OF_TESTCASES
};
