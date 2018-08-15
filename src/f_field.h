/**
 * @file f_field.h
 * @author Mike Hamburg
 *
 * @copyright
 *   Copyright (c) 2015-2018 Ristretto Developers, Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 *
 * @brief Field-specific code for 2^255 - 19.
 */

#ifndef __P25519_F_FIELD_H__
#define __P25519_F_FIELD_H__ 1

#include "constant_time.h"
#include <string.h>
#include <assert.h>

#include "word.h"

#define SER_BYTES         RISTRETTO255_SER_BYTES
#define GF_LIT_LIMB_BITS  51
#define GF_BITS           255

#define INLINE_UNUSED __inline__ __attribute__((unused,always_inline))

#ifdef __cplusplus
extern "C" {
#endif

/* Defined below in f_impl.h */
static INLINE_UNUSED void gf_copy (gf_25519_t *out, const gf_25519_t *a) { *out = *a; }
static INLINE_UNUSED void gf_add_RAW (gf_25519_t *out, const gf_25519_t *a, const gf_25519_t *b);
static INLINE_UNUSED void gf_sub_RAW (gf_25519_t *out, const gf_25519_t *a, const gf_25519_t *b);
static INLINE_UNUSED void gf_bias (gf_25519_t *inout, int amount);
static INLINE_UNUSED void gf_weak_reduce (gf_25519_t *inout);

void gf_strong_reduce (gf_25519_t *inout);
void gf_add (gf_25519_t *out, const gf_25519_t *a, const gf_25519_t *b);
void gf_sub (gf_25519_t *out, const gf_25519_t *a, const gf_25519_t *b);
void gf_mul (gf_25519_t *__restrict__ out, const gf_25519_t *a, const gf_25519_t *b);
void gf_mulw_unsigned (gf_25519_t *__restrict__ out, const gf_25519_t *a, uint32_t b);
void gf_sqr (gf_25519_t *__restrict__ out, const gf_25519_t *a);
mask_t gf_isr(gf_25519_t *a, const gf_25519_t *x); /** a^2 x = 1, QNR, or 0 if x=0.  Return true if successful */
mask_t gf_eq (const gf_25519_t *x, const gf_25519_t *y);
mask_t gf_lobit (const gf_25519_t *x);
mask_t gf_hibit (const gf_25519_t *x);

void gf_serialize (uint8_t *serial, const gf_25519_t *x,int with_highbit);
mask_t gf_deserialize (gf_25519_t *x, const uint8_t serial[SER_BYTES], int with_hibit, uint8_t hi_nmask);


#ifdef __cplusplus
} /* extern "C" */
#endif

#include "f_impl.h" /* Bring in the inline implementations */

extern const gf_25519_t SQRT_MINUS_ONE;

#ifndef LIMBPERM
  #define LIMBPERM(i) (i)
#endif
#define LIMB_MASK(i) (((1ull)<<LIMB_PLACE_VALUE(i))-1)

static const gf_25519_t ZERO = {{0}}, ONE = {{ [LIMBPERM(0)] = 1 }};

#endif /* __P25519_F_FIELD_H__ */
