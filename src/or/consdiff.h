/* Copyright (c) 2014, Daniel Martí
 * Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_CONSDIFF_H
#define TOR_CONSDIFF_H

#include "container.h"

/** Data structure to define a slice of a smarltist. */
typedef struct {
  /** Smartlist that this slice is made from. */
  smartlist_t *list;
  /** Starting position of the smartlist. */
  int offset;
  /** Number of elements in the slice. */
  int len;
} smartlist_slice_t;

smartlist_t *consdiff_gen_diff(smartlist_t *cons1, smartlist_t *cons2);
smartlist_t *consdiff_apply_diff(smartlist_t *cons1, smartlist_t *diff);

#endif

