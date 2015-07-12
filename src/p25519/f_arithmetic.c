/**
 * @cond internal
 * @file f_arithmetic.c
 * @copyright
 *   Copyright (c) 2014 Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 * @author Mike Hamburg
 * @brief Field-specific arithmetic.
 */

#include "field.h"

const gf_25519_t P25519_SQRT_MINUS_ONE = {FIELD_LITERAL(
    0x61b274a0ea0b0,
    0x0d5a5fc8f189d,
    0x7ef5e9cbd0c60,
    0x78595a6804c9e,
    0x2b8324804fc1d
)};
    
const gf_25519_t SQRT_ONE_MINUS_D = {FIELD_LITERAL( // FIXME MAGIC goes elsewhere?
    0x6db8831bbddec,
    0x38d7b56c9c165,
    0x016b221394bdc,
    0x7540f7816214a,
    0x0a0d85b4032b1
)};
    
static const gf_25519_t ONE = {FIELD_LITERAL( // FIXME copy-pasted
    1,0,0,0,0
)}; 

// ARCH MAGIC FIXME copy-pasted from decaf_fast.c
static mask_t gf_eq(const gf_25519_t a, const gf_25519_t b) {
    gf_25519_t c;
    gf_sub(c,a,b);
    gf_strong_reduce(c);
    mask_t ret=0;
    int i;
    for (i=0; i<5; i++) { ret |= c->limb[i]; }
    return ((__uint128_t)ret - 1) >> 64;
}

/* Guarantee: a^2 x = 0 if x = 0; else a^2 x = 1 or SQRT_MINUS_ONE; */
void 
gf_isr (
    gf_25519_t a,
    const gf_25519_t x
) {
    gf_25519_t st[3], tmp1, tmp2;
    const struct { unsigned char sh, idx; } ops[] = {
        {1,2},{1,2},{3,1},{6,0},{1,2},{12,1},{25,1},{25,1},{50,0},{125,0},{2,2},{1,2}
    };
    st[0][0] = st[1][0] = st[2][0] = x[0];
    unsigned int i;
    for (i=0; i<sizeof(ops)/sizeof(ops[0]); i++) {
        gf_sqrn(tmp1, st[1^(i&1)], ops[i].sh);
        gf_mul(tmp2, tmp1, st[ops[i].idx]);
        st[i&1][0] = tmp2[0];
    }
    
    mask_t mask = gf_eq(st[1],ONE) | gf_eq(st[1],SQRT_MINUS_ONE);
    
    // ARCH MAGIC FIXME: should be cond_sel
    for (i=0; i<5; i++) tmp1->limb[i] = (ONE->limb[i]            &  mask)
                                      | (SQRT_MINUS_ONE->limb[i] & ~mask);
    gf_mul(a,tmp1,st[0]);
}
