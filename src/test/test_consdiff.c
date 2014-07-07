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
  if (sl) SMARTLIST_FOREACH(sl, char*, line, tor_free(line));
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
  if (sl1) SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  if (sl2) SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
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
  if (sl1) SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  if (sl2) SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
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
  if (sl1) SMARTLIST_FOREACH(sl1, char*, line, tor_free(line));
  if (sl2) SMARTLIST_FOREACH(sl2, char*, line, tor_free(line));
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
test_consdiff_base64cmp(void)
{
  /* NULL arguments. */
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
test_consdiff_gen_ed_diff(void)
{
  smartlist_t *cons1, *cons2, *diff;
  cons1 = smartlist_new();
  cons2 = smartlist_new();

  /* Identity hashes are not sorted properly, return NULL. */
  smartlist_add(cons1, "r name bbbbbbbbbbbbbbbbbbbbbbbbbbb etc");
  smartlist_add(cons1, "foo");
  smartlist_add(cons1, "r name aaaaaaaaaaaaaaaaaaaaaaaaaaa etc");
  smartlist_add(cons1, "bar");

  smartlist_add(cons2, "r name aaaaaaaaaaaaaaaaaaaaaaaaaaa etc");
  smartlist_add(cons2, "foo");
  smartlist_add(cons2, "r name ccccccccccccccccccccccccccc etc");
  smartlist_add(cons2, "bar");

  diff = gen_ed_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

  /* Same, but now with the second consensus. */
  diff = gen_ed_diff(cons2, cons1);
  test_eq_ptr(NULL, diff);

  /* Identity hashes are repeated, return NULL. */
  smartlist_clear(cons1);

  smartlist_add(cons1, "r name bbbbbbbbbbbbbbbbbbbbbbbbbbb etc");
  smartlist_add(cons1, "foo");
  smartlist_add(cons1, "r name bbbbbbbbbbbbbbbbbbbbbbbbbbb etc");
  smartlist_add(cons1, "bar");

  diff = gen_ed_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

  /* We have to add a line that is just a dot, return NULL. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);

  smartlist_add(cons1, "foo1");
  smartlist_add(cons1, "foo2");

  smartlist_add(cons2, "foo1");
  smartlist_add(cons2, ".");
  smartlist_add(cons2, "foo2");

  diff = gen_ed_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

  /* We have dot lines, but they don't interfere with the script format. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);

  smartlist_add(cons1, "foo1");
  smartlist_add(cons1, ".");
  smartlist_add(cons1, ".");
  smartlist_add(cons1, "foo2");

  smartlist_add(cons2, "foo1");
  smartlist_add(cons2, ".");
  smartlist_add(cons2, "foo2");

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

  smartlist_add(cons1, "foo");
  smartlist_add(cons1, "bar");

  smartlist_add(cons2, "foo");
  smartlist_add(cons2, "bar");

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
  smartlist_add(cons2, "foo2");
  smartlist_add(cons2, "bar2");
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
test_consdiff_apply_ed_diff(void)
{
  smartlist_t *cons1, *cons2, *diff;
  cons1 = smartlist_new();
  diff = smartlist_new();

  smartlist_split_string(cons1, "A:B:C:D:E", ":", 0, 0);

  /* Command without range. */
  smartlist_add(diff, "a");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Range without command. */
  smartlist_add(diff, "1");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Range without end. */
  smartlist_add(diff, "1,");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Incoherent ranges. */
  smartlist_add(diff, "1,1");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  smartlist_add(diff, "3,2");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Script is not in reverse order. */
  smartlist_add(diff, "1d");
  smartlist_add(diff, "3d");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Script contains unrecognised commands longer than one char. */
  smartlist_add(diff, "1foo");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Script contains unrecognised commands. */
  smartlist_add(diff, "1e");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  smartlist_clear(diff);

  /* Command that should be followed by at least one line and a ".", but
   * isn't. */
  smartlist_add(diff, "0a");
  cons2 = apply_ed_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* Now it is followed by a ".", but it inserts zero lines. */
  smartlist_add(diff, ".");
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
test_consdiff_crypto_digest_smartlist_ends(void)
{
  smartlist_t *sl = smartlist_new();
  char digest[DIGEST256_LEN];

  smartlist_split_string(sl, "A:B:C:D:E", ":", 0, 0);
  crypto_digest_smartlist_ends(digest, sl, "");
  char e_digest1[DIGEST256_LEN] = {
    0xf0, 0x39, 0x3f, 0xeb, 0xe8, 0xba, 0xaa, 0x55,
    0xe3, 0x2f, 0x7b, 0xe2, 0xa7, 0xcc, 0x18, 0x0b,
    0xf3, 0x4e, 0x52, 0x13, 0x7d, 0x99, 0xe0, 0x56,
    0xc8, 0x17, 0xa9, 0xc0, 0x7b, 0x8f, 0x23, 0x9a };
  test_memeq(e_digest1, digest, DIGEST256_LEN*sizeof(char));

  SMARTLIST_FOREACH(sl, char*, line, tor_free(line));
  smartlist_clear(sl);
  smartlist_split_string(sl, "A:B:C:D:E", ":", 0, 0);
  crypto_digest_smartlist_ends(digest, sl, "\n");
  char e_digest2[DIGEST256_LEN] = {
    0x8b, 0x15, 0x83, 0xda, 0x45, 0xbf, 0x94, 0x54,
    0xa0, 0x07, 0x84, 0x83, 0xf6, 0xf7, 0x6d, 0xcf,
    0x62, 0x92, 0x9b, 0x57, 0xcc, 0x95, 0x03, 0x1d,
    0x5b, 0x74, 0xa0, 0x73, 0x4a, 0x9a, 0x0b, 0xa6 };
  test_memeq(e_digest2, digest, DIGEST256_LEN*sizeof(char));

  SMARTLIST_FOREACH(sl, char*, line, tor_free(line));
  smartlist_clear(sl);
  smartlist_split_string(sl, "AA:B:CC:D:EEE", ":", 0, 0);
  crypto_digest_smartlist_ends(digest, sl, "foobar");
  char e_digest3[DIGEST256_LEN] = {
    0x81, 0x20, 0x93, 0x64, 0x1e, 0xf1, 0x31, 0x82,
    0x63, 0x4d, 0x34, 0x42, 0x98, 0x63, 0xaf, 0x76,
    0xc3, 0xca, 0x41, 0x11, 0x78, 0x35, 0x6a, 0x45,
    0x47, 0x2d, 0xbc, 0xce, 0xa7, 0x57, 0x74, 0xb1 };
  test_memeq(e_digest3, digest, DIGEST256_LEN*sizeof(char));

 done:
  if (sl) SMARTLIST_FOREACH(sl, char*, line, tor_free(line));
  smartlist_free(sl);
}

static void
test_consdiff_gen_diff(void)
{
  smartlist_t *cons1, *cons2, *diff;
  cons1 = smartlist_new();
  cons2 = smartlist_new();

  /* Identity hashes are not sorted properly, return NULL.
   * Already tested in gen_ed_diff, but see that a NULL ed diff also makes
   * gen_diff return NULL. */
  smartlist_add(cons1, "r name bbbbbbbbbbbbbbbbbbbbbbbbbbb etc");
  smartlist_add(cons1, "foo");
  smartlist_add(cons1, "r name aaaaaaaaaaaaaaaaaaaaaaaaaaa etc");
  smartlist_add(cons1, "bar");

  smartlist_add(cons2, "r name aaaaaaaaaaaaaaaaaaaaaaaaaaa etc");
  smartlist_add(cons2, "foo");
  smartlist_add(cons2, "r name ccccccccccccccccccccccccccc etc");
  smartlist_add(cons2, "bar");

  diff = consdiff_gen_diff(cons1, cons2);
  test_eq_ptr(NULL, diff);

  /* Test 'a', 'c' and 'd' together. See that it is done in reverse order.
   * As tested in gen_ed_diff, but also check the header. */
  smartlist_clear(cons1);
  smartlist_clear(cons2);
  smartlist_split_string(cons1, "A:B:C:D:E", ":", 0, 0);
  smartlist_split_string(cons2, "A:C:O:E:U", ":", 0, 0);
  diff = consdiff_gen_diff(cons1, cons2);
  test_neq_ptr(NULL, diff);
  test_eq(9, smartlist_len(diff));
  test_streq("network-status-diff-version 1", smartlist_get(diff, 0));
  /*test_streq("hash foo bar", smartlist_get(diff, 1));*/
  test_streq("5a", smartlist_get(diff, 2));
  test_streq("U", smartlist_get(diff, 3));
  test_streq(".", smartlist_get(diff, 4));
  test_streq("4c", smartlist_get(diff, 5));
  test_streq("O", smartlist_get(diff, 6));
  test_streq(".", smartlist_get(diff, 7));
  test_streq("2d", smartlist_get(diff, 8));

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
test_consdiff_apply_diff(void)
{
  smartlist_t *cons1, *cons2, *diff;
  cons1 = smartlist_new();
  diff = smartlist_new();

  /* diff doesn't have enough lines. */
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* first line doesn't match format-version string. */
  smartlist_add(diff, "foo-bar");
  smartlist_add(diff, "header-line");
  smartlist_add(diff, "0d");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* The first word of the second header line is not "hash". */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "word a b");
  smartlist_add(diff, "0d");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* Wrong number of words after "hash". */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash a b c");
  smartlist_add(diff, "0d");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* base16 sha256 digests do not have the expected length. */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash aaa bbb");
  smartlist_add(diff, "0d");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* base16 sha256 digests contain non-base16 characters. */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash"
      " ????????????????????????????????????????????????????????????????"
      " ----------------------------------------------------------------");
  smartlist_add(diff, "0d");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* The digest of the starting consensus in the diff is not correct. */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash"
      " 2222222222222222222222222222222222222222222222222222222222222222"
      " 3333333333333333333333333333333333333333333333333333333333333333");
  smartlist_add(diff, "0d");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* Invalid ed diff.
   * As tested in apply_ed_diff, but check that apply_diff does return NULL if
   * the ed diff can't be applied. */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash"
      /* sha256 of "". */
      " e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
      /* bogus sha256. */
      " 3333333333333333333333333333333333333333333333333333333333333333");
  smartlist_add(diff, "foobar");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* Resulting consensus doesn't match its digest as found in the diff. */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash"
      /* sha256 of "". */
      " e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
      /* bogus sha256. */
      " 3333333333333333333333333333333333333333333333333333333333333333");
  smartlist_add(diff, "0a");
  smartlist_add(diff, "foo");
  smartlist_add(diff, ".");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_eq_ptr(NULL, cons2);

  /* Very simple test, only to see that nothing errors. */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash"
      /* sha256 of "". */
      " e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
      /* sha256 of "foo\n". */
      " b5bb9d8014a0f9b1d61e21e796d78dccdf1352f23cd32812f4850b878ae4944c");
  smartlist_add(diff, "0a");
  smartlist_add(diff, "foo");
  smartlist_add(diff, ".");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_neq_ptr(NULL, cons2);
  SMARTLIST_FOREACH(cons2, char *, cp, tor_free(cp));
  smartlist_free(cons2);

  /* Check that capital letters in base16-encoded digests work too. */
  smartlist_clear(diff);
  smartlist_add(diff, "network-status-diff-version 1");
  smartlist_add(diff, "hash"
      /* sha256 of "". */
      " E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"
      /* sha256 of "foo\n". */
      " B5BB9D8014A0F9B1D61E21E796D78DCCDF1352F23CD32812F4850B878AE4944C");
  smartlist_add(diff, "0a");
  smartlist_add(diff, "foo");
  smartlist_add(diff, ".");
  cons2 = consdiff_apply_diff(cons1, diff);
  test_neq_ptr(NULL, cons2);
  SMARTLIST_FOREACH(cons2, char *, cp, tor_free(cp));
  smartlist_free(cons2);

  smartlist_clear(diff);

 done:
  smartlist_free(cons1);
  smartlist_free(diff);
}

struct testcase_t consdiff_tests[] = {
  END_OF_TESTCASES
};
