/**
 * @file bool.c
 * @author Mike Hamburg
 *
 * @copyright
 *   Copyright (c) 2015-2018 Ristretto Developers, Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 *
 * @brief Boolean values used by this library
 */

#include <ristretto255.h>

/* These constants exist to make the existing implementation immediately
 * FFI-able and therefore testable, however exposing these constants and
 * `ristretto_bool_t` as part of the public API is probably a bad idea.
 * -@tarcieri
 *
 * Here is Mike Hamburg's original plan for booleans (from word.h):
 *
 * The external interface uses ristretto_bool_t, but this might be a different
 * size than our particular arch's word_t (and thus mask_t).  Also, the caller
 * isn't guaranteed to pass it as nonzero.  So bool_to_mask converts word sizes
 * and checks nonzero.
 *
 * On the flip side, mask_t is always -1 or 0, but it might be a different size
 * than ristretto_bool_t.
 */

/** RISTRETTO_TRUE = -1 so that RISTRETTO_TRUE & x = x */
const ristretto_bool_t RISTRETTO_TRUE = -(ristretto_bool_t)1;

/** RISTRETTO_FALSE = 0 so that RISTRETTO_FALSE & x = 0 */
const ristretto_bool_t RISTRETTO_FALSE = 0;
