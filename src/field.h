/**
 * @file field.h
 * @brief Generic gf header.
 * @copyright
 *   Copyright (c) 2014-2018 Ristretto Developers, Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 * @author Mike Hamburg
 */

#ifndef __GF_H__
#define __GF_H__

#include "constant_time.h"
#include "f_field.h"
#include <string.h>

/** Square x, n times. */
static RISTRETTO_INLINE void gf_sqrn (
    gf_25519_t *__restrict__ y,
    const gf_25519_t *x,
    int n
) {
    gf_25519_t tmp;
    assert(n>0);
    if (n&1) {
        gf_sqr(y,x);
        n--;
    } else {
        gf_sqr(&tmp,x);
        gf_sqr(y,&tmp);
        n-=2;
    }
    for (; n; n-=2) {
        gf_sqr(&tmp,y);
        gf_sqr(y,&tmp);
    }
}

#define gf_add_nr gf_add_RAW

/** Subtract mod p.  Bias by 2 and don't reduce  */
static inline void gf_sub_nr ( gf_25519_t *c, const gf_25519_t *a, const gf_25519_t *b ) {
    gf_sub_RAW(c,a,b);
    gf_bias(c, 2);
    if (GF_HEADROOM < 3) gf_weak_reduce(c);
}

/** Subtract mod p. Bias by amt but don't reduce.  */
static inline void gf_subx_nr ( gf_25519_t *c, const gf_25519_t *a, const gf_25519_t *b, int amt ) {
    gf_sub_RAW(c,a,b);
    gf_bias(c, amt);
    if (GF_HEADROOM < amt+1) gf_weak_reduce(c);
}

/** Mul by signed int.  Not constant-time WRT the sign of that int. */
static inline void gf_mulw(gf_25519_t *c, const gf_25519_t *a, int32_t w) {
    if (w>0) {
        gf_mulw_unsigned(c, a, w);
    } else {
        gf_mulw_unsigned(c, a, -w);
        gf_sub(c,&ZERO,c);
    }
}

/** Constant time, x = is_z ? z : y */
static inline void gf_cond_sel(gf_25519_t *x, const gf_25519_t *y, const gf_25519_t *z, mask_t is_z) {
    constant_time_select(x,y,z,sizeof(gf_25519_t),is_z,0);
}

/** Constant time, if (neg) x=-x; */
static inline void gf_cond_neg(gf_25519_t *x, mask_t neg) {
    gf_25519_t y;
    gf_sub(&y,&ZERO,x);
    gf_cond_sel(x,x,&y,neg);
}

/** Constant time, if (swap) (x,y) = (y,x); */
static inline void
gf_cond_swap(gf_25519_t *x, gf_25519_t *__restrict__ y, mask_t swap) {
    constant_time_cond_swap(x,y,sizeof(gf_25519_t),swap);
}

static RISTRETTO_INLINE void gf_mul_qnr(gf_25519_t *__restrict__ out, const gf_25519_t *x) {
    /* r = QNR * r0^2 */
    gf_mul(out,x,&SQRT_MINUS_ONE);
}

static RISTRETTO_INLINE void gf_div_qnr(gf_25519_t *__restrict__ out, const gf_25519_t *x) {
    /* r = QNR * r0^2 */
    gf_mul(out,x,&SQRT_MINUS_ONE);
    gf_sub(out,&ZERO,out);
}

#define gf_mul_i gf_mul_qnr
#define gf_div_i gf_div_qnr


#endif // __GF_H__
