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

  /* See if the slice was done correctly. */
  smartlist_slice_t *sls = smartlist_slice(sl, 2, 3);
  test_eq_ptr(sl, sls->list);
  test_eq_ptr((void*)3, smartlist_get(sls->list, sls->offset));
  test_eq_ptr((void*)5, smartlist_get(sls->list, sls->offset + (sls->len-1)));

 done:
  smartlist_free(sl);
  tor_free(sls);
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
  smartlist_free(sl);
  tor_free(sls);
}

static void
test_consdiff_trim_slices(void)
{
  /* Create a regular smartlist. */
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  smartlist_split_string(sl1, "a:b:b:b:d", ":", 0, 0);
  smartlist_split_string(sl2, "a:c:c:c:d", ":", 0, 0);
  
  smartlist_slice_t *sls1 = smartlist_slice(sl1, 0, smartlist_len(sl1));
  smartlist_slice_t *sls2 = smartlist_slice(sl2, 0, smartlist_len(sl2));

  test_eq(5, sls1->len);
  test_eq(5, sls2->len);
  trim_slices(sls1, sls2);
  test_eq(3, sls1->len);
  test_eq(3, sls2->len);

  sls1 = smartlist_slice(sl1, 0, smartlist_len(sl1));

 done:
  smartlist_free(sl1);
  smartlist_free(sl2);
  tor_free(sls1);
  tor_free(sls2);
}

struct testcase_t consdiff_tests[] = {
  END_OF_TESTCASES
};
