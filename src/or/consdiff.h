/* Copyright (c) 2014, Daniel Martí
 * Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_CONSDIFF_H
#define TOR_CONSDIFF_H

#include "or.h"

/** Data structure to define a slice of a smarltist. */
typedef struct {
  /**
   * Smartlist that this slice is made from.
   * References the whole original smartlist that the slice was made out of.
   * */
  smartlist_t *list;
  /** Starting position of the slice in the smartlist. */
  int offset;
  /** Length of the slice, i.e. the number of elements it holds. */
  int len;
} smartlist_slice_t;

smartlist_t *
consdiff_gen_diff(smartlist_t *cons1, smartlist_t *cons2,
                  digests_t *digests1, digests_t *digests2);
char *
consdiff_apply_diff(smartlist_t *cons1, smartlist_t *diff,
                    digests_t *digests1);
int
consdiff_get_digests(smartlist_t *diff,
                     char *digest1, char *digest1_hex,
                     char *digest2, char *digest2_hex);

#endif

