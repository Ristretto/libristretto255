/**
 * @file curve25519/scalar.c
 * @author Mike Hamburg
 *
 * @copyright
 *   Copyright (c) 2015-2018 Ristretto Developers, Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 *
 * @brief Ristretto255 high-level functions.
 *
 * @warning This file was automatically generated in Python.
 * Please do not edit it.
 */
#include "word.h"
#include "constant_time.h"
#include <ristretto255/common.h>
#include <ristretto255/point.h>

/* Template stuff */
#define SCALAR_BITS RISTRETTO255_SCALAR_BITS
#define SCALAR_SER_BYTES RISTRETTO255_SCALAR_BYTES
#define SCALAR_LIMBS RISTRETTO255_SCALAR_LIMBS
#define scalar_t ristretto255_scalar_t

static const ristretto_word_t MONTGOMERY_FACTOR = (ristretto_word_t)0xd2b51da312547e1bull;
static const scalar_t sc_p = {{{
    SC_LIMB(0x5812631a5cf5d3ed), SC_LIMB(0x14def9dea2f79cd6), SC_LIMB(0x0000000000000000), SC_LIMB(0x1000000000000000)
}}}, sc_r2 = {{{
    SC_LIMB(0xa40611e3449c0f01), SC_LIMB(0xd00e1ba768859347), SC_LIMB(0xceec73d217f5be65), SC_LIMB(0x0399411b7c309a3d)
}}};
/* End of template stuff */

#define WBITS RISTRETTO_WORD_BITS /* NB this may be different from ARCH_WORD_BITS */

const scalar_t ristretto255_scalar_one = {{{1}}}, ristretto255_scalar_zero = {{{0}}};

/** {extra,accum} - sub +? p
 * Must have extra <= 1
 */
static RISTRETTO_NOINLINE void sc_subx(
    scalar_t out,
    const ristretto_word_t accum[SCALAR_LIMBS],
    const scalar_t sub,
    const scalar_t p,
    ristretto_word_t extra
) {
    ristretto_dsword_t chain = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + accum[i]) - sub->limb[i];
        out->limb[i] = chain;
        chain >>= WBITS;
    }
    ristretto_word_t borrow = chain+extra; /* = 0 or -1 */
    
    chain = 0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + out->limb[i]) + (p->limb[i] & borrow);
        out->limb[i] = chain;
        chain >>= WBITS;
    }
}

static RISTRETTO_NOINLINE void sc_montmul (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    unsigned int i,j;
    ristretto_word_t accum[SCALAR_LIMBS+1] = {0};
    ristretto_word_t hi_carry = 0;
    
    for (i=0; i<SCALAR_LIMBS; i++) {
        ristretto_word_t mand = a->limb[i];
        const ristretto_word_t *mier = b->limb;
        
        ristretto_dword_t chain = 0;
        for (j=0; j<SCALAR_LIMBS; j++) {
            chain += ((ristretto_dword_t)mand)*mier[j] + accum[j];
            accum[j] = chain;
            chain >>= WBITS;
        }
        accum[j] = chain;
        
        mand = accum[0] * MONTGOMERY_FACTOR;
        chain = 0;
        mier = sc_p->limb;
        for (j=0; j<SCALAR_LIMBS; j++) {
            chain += (ristretto_dword_t)mand*mier[j] + accum[j];
            if (j) accum[j-1] = chain;
            chain >>= WBITS;
        }
        chain += accum[j];
        chain += hi_carry;
        accum[j-1] = chain;
        hi_carry = chain >> WBITS;
    }
    
    sc_subx(out, accum, sc_p, sc_p, hi_carry);
}

void ristretto255_scalar_mul (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    sc_montmul(out,a,b);
    sc_montmul(out,out,sc_r2);
}

/* PERF: could implement this */
static RISTRETTO_INLINE void sc_montsqr (scalar_t out, const scalar_t a) {
    sc_montmul(out,a,a);
}

ristretto_error_t ristretto255_scalar_invert (
    scalar_t out,
    const scalar_t a
) {
    /* Fermat's little theorem, sliding window.
     * Sliding window is fine here because the modulus isn't secret.
     */
    const int SCALAR_WINDOW_BITS = 3;
    scalar_t precmp[1<<3];  // Rewritten from SCALAR_WINDOW_BITS for windows compatibility
    const int LAST = (1<<SCALAR_WINDOW_BITS)-1;

    /* Precompute precmp = [a^1,a^3,...] */
    sc_montmul(precmp[0],a,sc_r2);
    if (LAST > 0) sc_montmul(precmp[LAST],precmp[0],precmp[0]);

    int i;
    for (i=1; i<=LAST; i++) {
        sc_montmul(precmp[i],precmp[i-1],precmp[LAST]);
    }
    
    /* Sliding window */
    unsigned residue = 0, trailing = 0, started = 0;
    for (i=SCALAR_BITS-1; i>=-SCALAR_WINDOW_BITS; i--) {
        
        if (started) sc_montsqr(out,out);
        
        ristretto_word_t w = (i>=0) ? sc_p->limb[i/WBITS] : 0;
        if (i >= 0 && i<WBITS) {
            assert(w >= 2);
            w-=2;
        }
        
        residue = (residue<<1) | ((w>>(i%WBITS))&1);
        if (residue>>SCALAR_WINDOW_BITS != 0) {
            assert(trailing == 0);
            trailing = residue;
            residue = 0;
        }
        
        if (trailing > 0 && (trailing & ((1<<SCALAR_WINDOW_BITS)-1)) == 0) {
            if (started) {
                sc_montmul(out,out,precmp[trailing>>(SCALAR_WINDOW_BITS+1)]);
            } else {
                ristretto255_scalar_copy(out,precmp[trailing>>(SCALAR_WINDOW_BITS+1)]);
                started = 1;
            }
            trailing = 0;
        }
        trailing <<= 1;
        
    }
    assert(residue==0);
    assert(trailing==0);
    
    /* Demontgomerize */
    sc_montmul(out,out,ristretto255_scalar_one);
    ristretto_bzero(precmp, sizeof(precmp));
    return ristretto_succeed_if(~ristretto255_scalar_eq(out,ristretto255_scalar_zero));
}

void ristretto255_scalar_sub (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    sc_subx(out, a->limb, b, sc_p, 0);
}

void ristretto255_scalar_add (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    ristretto_dword_t chain = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + a->limb[i]) + b->limb[i];
        out->limb[i] = chain;
        chain >>= WBITS;
    }
    sc_subx(out, out->limb, sc_p, sc_p, chain);
}

void
ristretto255_scalar_set_unsigned (
    scalar_t out,
    uint64_t w
) {
    memset(out,0,sizeof(scalar_t));
    unsigned int i = 0;
    for (; i<sizeof(uint64_t)/sizeof(ristretto_word_t); i++) {
        out->limb[i] = w;
#if RISTRETTO_WORD_BITS < 64
        w >>= 8*sizeof(ristretto_word_t);
#endif
    }
}

ristretto_bool_t
ristretto255_scalar_eq (
    const scalar_t a,
    const scalar_t b
) {
    ristretto_word_t diff = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        diff |= a->limb[i] ^ b->limb[i];
    }
    return mask_to_bool(word_is_zero(diff));
}

static RISTRETTO_INLINE void scalar_decode_short (
    scalar_t s,
    const unsigned char *ser,
    unsigned int nbytes
) {
    unsigned int i,j,k=0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        ristretto_word_t out = 0;
        for (j=0; j<sizeof(ristretto_word_t) && k<nbytes; j++,k++) {
            out |= ((ristretto_word_t)ser[k])<<(8*j);
        }
        s->limb[i] = out;
    }
}

ristretto_error_t ristretto255_scalar_decode(
    scalar_t s,
    const unsigned char ser[SCALAR_SER_BYTES]
) {
    unsigned int i;
    scalar_decode_short(s, ser, SCALAR_SER_BYTES);
    ristretto_dsword_t accum = 0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        accum = (accum + s->limb[i] - sc_p->limb[i]) >> WBITS;
    }
    /* Here accum == 0 or -1 */
    
    ristretto255_scalar_mul(s,s,ristretto255_scalar_one); /* ham-handed reduce */
    
    return ristretto_succeed_if(~word_is_zero(accum));
}

void ristretto255_scalar_destroy (
    scalar_t scalar
) {
    ristretto_bzero(scalar, sizeof(scalar_t));
}

void ristretto255_scalar_decode_long(
    scalar_t s,
    const unsigned char *ser,
    size_t ser_len
) {
    if (ser_len == 0) {
        ristretto255_scalar_copy(s, ristretto255_scalar_zero);
        return;
    }
    
    size_t i;
    scalar_t t1, t2;

    i = ser_len - (ser_len%SCALAR_SER_BYTES);
    if (i==ser_len) i -= SCALAR_SER_BYTES;
    
    scalar_decode_short(t1, &ser[i], ser_len-i);

    if (ser_len == sizeof(scalar_t)) {
        assert(i==0);
        /* ham-handed reduce */
        ristretto255_scalar_mul(s,t1,ristretto255_scalar_one);
        ristretto255_scalar_destroy(t1);
        return;
    }

    while (i) {
        i -= SCALAR_SER_BYTES;
        sc_montmul(t1,t1,sc_r2);
        ignore_result( ristretto255_scalar_decode(t2, ser+i) );
        ristretto255_scalar_add(t1, t1, t2);
    }

    ristretto255_scalar_copy(s, t1);
    ristretto255_scalar_destroy(t1);
    ristretto255_scalar_destroy(t2);
}

void ristretto255_scalar_encode(
    unsigned char ser[SCALAR_SER_BYTES],
    const scalar_t s
) {
    unsigned int i,j,k=0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        for (j=0; j<sizeof(ristretto_word_t); j++,k++) {
            ser[k] = s->limb[i] >> (8*j);
        }
    }
}

void ristretto255_scalar_cond_sel (
    scalar_t out,
    const scalar_t a,
    const scalar_t b,
    ristretto_bool_t pick_b
) {
    constant_time_select(out,a,b,sizeof(scalar_t),bool_to_mask(pick_b),sizeof(out->limb[0]));
}

void ristretto255_scalar_halve (
    scalar_t out,
    const scalar_t a
) {
    ristretto_word_t mask = -(a->limb[0] & 1);
    ristretto_dword_t chain = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + a->limb[i]) + (sc_p->limb[i] & mask);
        out->limb[i] = chain;
        chain >>= RISTRETTO_WORD_BITS;
    }
    for (i=0; i<SCALAR_LIMBS-1; i++) {
        out->limb[i] = out->limb[i]>>1 | out->limb[i+1]<<(WBITS-1);
    }
    out->limb[i] = out->limb[i]>>1 | chain<<(WBITS-1);
}

