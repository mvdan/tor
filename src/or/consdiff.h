/** Data structure to define a slice of a smarltist. */
typedef struct {
  /** Smartlist that this slice is made from. */
  smartlist_t *list;
  /** Starting position of the smartlist. */
  int offset;
  /** Number of elements in the slice. */
  int len;
} smartlist_slice_t;

smartlist_t *gen_diff(smartlist_t *cons1, smartlist_t *cons2);
smartlist_t *apply_diff(smartlist_t *cons1, smartlist_t *diff);
