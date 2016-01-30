/** @brief Decaf high-level functions. */

#define _XOPEN_SOURCE 600 /* for posix_memalign */
#define __STDC_WANT_LIB_EXT1__ 1 /* for memset_s */
#include <string.h>

#include "word.h"
#include "field.h"

#include <decaf.h>

/* Template stuff */
#define API_NS(_id) $(c_ns)_##_id
#define SCALAR_BITS $(C_NS)_SCALAR_BITS
#define SCALAR_LIMBS $(C_NS)_SCALAR_LIMBS
#define scalar_t API_NS(scalar_t)
#define point_t API_NS(point_t)
#define precomputed_s API_NS(precomputed_s)
#define IMAGINE_TWIST $(imagine_twist)
#define COFACTOR $(cofactor)

/** Comb config: number of combs, n, t, s. */
#define COMBS_N $(combs.n)
#define COMBS_T $(combs.t)
#define COMBS_S $(combs.s)
#define DECAF_WINDOW_BITS $(window_bits)
#define DECAF_WNAF_FIXED_TABLE_BITS $(wnaf.fixed)
#define DECAF_WNAF_VAR_TABLE_BITS $(wnaf.var)

static const int EDWARDS_D = $(d);
static const scalar_t sc_p = {{{
    $(ser(q,64,"SC_LIMB"))
}}}, sc_r2 = {{{
    $(ser(((2**128)**((scalar_bits+63)/64))%q,64,"SC_LIMB"))
}}}, point_scalarmul_adjustment = {{{
    $(ser((2**(scalar_bits-1+window_bits - ((scalar_bits-1)%window_bits)) - 1) % q,64,"SC_LIMB"))
}}}, precomputed_scalarmul_adjustment = {{{
    $(ser((2**(combs.n*combs.t*combs.s) - 1) % q,64,"SC_LIMB"))
}}};
static const decaf_word_t MONTGOMERY_FACTOR = (decaf_word_t)0x$("%x" % pow(-q,2**64-1,2**64))ull;

const uint8_t API_NS(x_base_point)[SER_BYTES] /* TODO */ = {
    $(ser(mont_base,8))
};

#if COFACTOR==8
    static const gf SQRT_ONE_MINUS_D = {FIELD_LITERAL(
        $(sqrt_one_minus_d)
    )};
#endif

/* End of template stuff */


#if (COFACTOR == 8) && !IMAGINE_TWIST
/* FUTURE: Curve41417 doesn't have these properties. */
#error "Currently require IMAGINE_TWIST (and thus p=5 mod 8) for cofactor 8"
#endif

#if IMAGINE_TWIST && (P_MOD_8 != 5)
#error "Cannot use IMAGINE_TWIST except for p == 5 mod 8"
#endif

#if (COFACTOR != 8) && (COFACTOR != 4)
#error "COFACTOR must be 4 or 8"
#endif
 
#if IMAGINE_TWIST
extern const gf SQRT_MINUS_ONE;
#endif

#define WBITS DECAF_WORD_BITS /* NB this may be different from ARCH_WORD_BITS */

const scalar_t API_NS(scalar_one) = {{{1}}}, API_NS(scalar_zero) = {{{0}}};
extern const point_t API_NS(point_base);

/* Projective Niels coordinates */
typedef struct { gf a, b, c; } niels_s, niels_t[1];
typedef struct { niels_t n; gf z; } __attribute__((aligned(sizeof(big_register_t))))
    pniels_s, pniels_t[1];

/* Precomputed base */
struct precomputed_s { niels_t table [COMBS_N<<(COMBS_T-1)]; };

extern const gf API_NS(precomputed_base_as_fe)[];
const precomputed_s *API_NS(precomputed_base) =
    (const precomputed_s *) &API_NS(precomputed_base_as_fe);

const size_t API_NS(sizeof_precomputed_s) = sizeof(precomputed_s);
const size_t API_NS(alignof_precomputed_s) = sizeof(big_register_t);

#define FOR_LIMB(i,op) { unsigned int i=0; for (i=0; i<NLIMBS; i++)  { op; }}
#define FOR_LIMB_U(i,op) { unsigned int i=0; UNROLL for (i=0; i<NLIMBS; i++)  { op; }}

/* The plan on booleans:
 *
 * The external interface uses decaf_bool_t, but this might be a different
 * size than our particular arch's word_t (and thus mask_t).  Also, the caller
 * isn't guaranteed to pass it as nonzero.  So bool_to_mask converts word sizes
 * and checks nonzero.
 *
 * On the flip side, mask_t is always -1 or 0, but it might be a different size
 * than decaf_bool_t.
 *
 * On the third hand, we have success vs boolean types, but that's handled in
 * common.h: it converts between decaf_bool_t and decaf_error_t.
 */
static INLINE decaf_bool_t mask_to_bool (mask_t m) {
    return (decaf_sword_t)(sword_t)m;
}

static INLINE mask_t bool_to_mask (decaf_bool_t m) {
    /* On most arches this will be optimized to a simple cast. */
    mask_t ret = 0;
    unsigned int limit = sizeof(decaf_bool_t)/sizeof(mask_t);
    if (limit < 1) limit = 1;
    for (unsigned int i=0; i<limit; i++) {
        ret |= ~ word_is_zero(m >> (i*8*sizeof(word_t)));
    }
    return ret;
}

/** Constant time, x = is_z ? z : y */
static INLINE void
cond_sel(gf x, const gf y, const gf z, mask_t is_z) {
    constant_time_select(x,y,z,sizeof(gf),is_z,0);
}

/** Constant time, if (neg) x=-x; */
static void
cond_neg(gf x, mask_t neg) {
    gf y;
    gf_sub(y,ZERO,x);
    cond_sel(x,x,y,neg);
}

/** Constant time, if (swap) (x,y) = (y,x); */
static INLINE void
cond_swap(gf x, gf_s *__restrict__ y, mask_t swap) {
    constant_time_cond_swap(x,y,sizeof(gf_s),swap);
}

/** Inverse square root using addition chain. */
static mask_t
gf_isqrt_chk(gf y, const gf x, mask_t allow_zero) {
    gf tmp0, tmp1;
    gf_isr((gf_s *)y, (const gf_s *)x);
    gf_sqr(tmp0,y);
    gf_mul(tmp1,tmp0,x);
    return gf_eq(tmp1,ONE) | (allow_zero & gf_eq(tmp1,ZERO));
}

/** Inverse. */
static void
gf_invert(gf y, const gf x) {
    gf t1, t2;
    gf_sqr(t1, x); // o^2
    mask_t ret = gf_isqrt_chk(t2, t1, 0); // +-1/sqrt(o^2) = +-1/o
    (void)ret; assert(ret);
    gf_sqr(t1, t2);
    gf_mul(t2, t1, x); // not direct to y in case of alias.
    gf_copy(y, t2);
}

/** Mul by signed int.  Not constant-time WRT the sign of that int. */
static INLINE void
gf_mulw_sgn(gf c, const gf a, int32_t w) {
    if (w>0) {
        gf_mulw(c, a, w);
    } else {
        gf_mulw(c, a, -w);
        gf_sub(c,ZERO,c);
    }
}

/** Return high bit of x = low bit of 2x mod p */
static mask_t hibit(const gf x) {
    gf y;
    gf_add(y,x,x);
    gf_strong_reduce(y);
    return -(y->limb[0]&1);
}

#if COFACTOR==8
/** Return high bit of x = low bit of 2x mod p */
static mask_t lobit(const gf x) {
    gf y;
    gf_copy(y,x);
    gf_strong_reduce(y);
    return -(y->limb[0]&1);
}
#endif

/** {extra,accum} - sub +? p
 * Must have extra <= 1
 */
static NOINLINE void
sc_subx(
    scalar_t out,
    const decaf_word_t accum[SCALAR_LIMBS],
    const scalar_t sub,
    const scalar_t p,
    decaf_word_t extra
) {
    decaf_dsword_t chain = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + accum[i]) - sub->limb[i];
        out->limb[i] = chain;
        chain >>= WBITS;
    }
    decaf_word_t borrow = chain+extra; /* = 0 or -1 */
    
    chain = 0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + out->limb[i]) + (p->limb[i] & borrow);
        out->limb[i] = chain;
        chain >>= WBITS;
    }
}

static NOINLINE void
sc_montmul (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    unsigned int i,j;
    decaf_word_t accum[SCALAR_LIMBS+1] = {0};
    decaf_word_t hi_carry = 0;
    
    for (i=0; i<SCALAR_LIMBS; i++) {
        decaf_word_t mand = a->limb[i];
        const decaf_word_t *mier = b->limb;
        
        decaf_dword_t chain = 0;
        for (j=0; j<SCALAR_LIMBS; j++) {
            chain += ((decaf_dword_t)mand)*mier[j] + accum[j];
            accum[j] = chain;
            chain >>= WBITS;
        }
        accum[j] = chain;
        
        mand = accum[0] * MONTGOMERY_FACTOR;
        chain = 0;
        mier = sc_p->limb;
        for (j=0; j<SCALAR_LIMBS; j++) {
            chain += (decaf_dword_t)mand*mier[j] + accum[j];
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

void API_NS(scalar_mul) (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    sc_montmul(out,a,b);
    sc_montmul(out,out,sc_r2);
}

/* PERF: could implement this */
static INLINE void sc_montsqr (scalar_t out, const scalar_t a) {
    sc_montmul(out,a,a);
}

decaf_error_t API_NS(scalar_invert) (
    scalar_t out,
    const scalar_t a
) {
    /* Fermat's little theorem, sliding window.
     * Sliding window is fine here because the modulus isn't secret.
     */
    const int SCALAR_WINDOW_BITS = 3;
    scalar_t precmp[1<<SCALAR_WINDOW_BITS];
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
        
        decaf_word_t w = (i>=0) ? sc_p->limb[i/WBITS] : 0;
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
                API_NS(scalar_copy)(out,precmp[trailing>>(SCALAR_WINDOW_BITS+1)]);
                started = 1;
            }
            trailing = 0;
        }
        trailing <<= 1;
        
    }
    assert(residue==0);
    assert(trailing==0);
    
    /* Demontgomerize */
    sc_montmul(out,out,API_NS(scalar_one));
    decaf_bzero(precmp, sizeof(precmp));
    return decaf_succeed_if(~API_NS(scalar_eq)(out,API_NS(scalar_zero)));
}

void API_NS(scalar_sub) (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    sc_subx(out, a->limb, b, sc_p, 0);
}

void API_NS(scalar_add) (
    scalar_t out,
    const scalar_t a,
    const scalar_t b
) {
    decaf_dword_t chain = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + a->limb[i]) + b->limb[i];
        out->limb[i] = chain;
        chain >>= WBITS;
    }
    sc_subx(out, out->limb, sc_p, sc_p, chain);
}

static NOINLINE void
sc_halve (
    scalar_t out,
    const scalar_t a,
    const scalar_t p
) {
    decaf_word_t mask = -(a->limb[0] & 1);
    decaf_dword_t chain = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        chain = (chain + a->limb[i]) + (p->limb[i] & mask);
        out->limb[i] = chain;
        chain >>= WBITS;
    }
    for (i=0; i<SCALAR_LIMBS-1; i++) {
        out->limb[i] = out->limb[i]>>1 | out->limb[i+1]<<(WBITS-1);
    }
    out->limb[i] = out->limb[i]>>1 | chain<<(WBITS-1);
}

void
API_NS(scalar_set_unsigned) (
    scalar_t out,
    uint64_t w
) {
    memset(out,0,sizeof(scalar_t));
    unsigned int i = 0;
    for (; i<sizeof(uint64_t)/sizeof(decaf_word_t); i++) {
        out->limb[i] = w;
        w >>= (sizeof(uint64_t) > sizeof(decaf_word_t)) ? 8*sizeof(decaf_word_t) : 0;
    }
}

decaf_bool_t
API_NS(scalar_eq) (
    const scalar_t a,
    const scalar_t b
) {
    decaf_word_t diff = 0;
    unsigned int i;
    for (i=0; i<SCALAR_LIMBS; i++) {
        diff |= a->limb[i] ^ b->limb[i];
    }
    return mask_to_bool(word_is_zero(diff));
}

/** identity = (0,1) */
const point_t API_NS(point_identity) = {{{{{0}}},{{{1}}},{{{1}}},{{{0}}}}};

static void
deisogenize (
    gf_s *__restrict__ s,
    gf_s *__restrict__ minus_t_over_s,
    const point_t p,
    mask_t toggle_hibit_s,
    mask_t toggle_hibit_t_over_s,
    mask_t toggle_rotation
) {
#if COFACTOR == 4 && !IMAGINE_TWIST
    (void) toggle_rotation;
    
    gf b, d;
    gf_s *c = s, *a = minus_t_over_s;
    gf_mulw_sgn(a, p->y, 1-EDWARDS_D);
    gf_mul(c, a, p->t);     /* -dYT, with EDWARDS_D = d-1 */
    gf_mul(a, p->x, p->z); 
    gf_sub(d, c, a);  /* aXZ-dYT with a=-1 */
    gf_add(a, p->z, p->y); 
    gf_sub(b, p->z, p->y); 
    gf_mul(c, b, a);
    gf_mulw_sgn(b, c, -EDWARDS_D); /* (a-d)(Z+Y)(Z-Y) */
    mask_t ok = gf_isqrt_chk ( a, b, DECAF_TRUE); /* r in the paper */
    (void)ok; assert(ok);
    gf_mulw_sgn (b, a, -EDWARDS_D); /* u in the paper */

    gf_mul(c,a,d); /* r(aZX-dYT) */
    gf_mul(a,b,p->z); /* uZ */
    gf_add(a,a,a); /* 2uZ */
    
    cond_neg(c, toggle_hibit_t_over_s ^ ~hibit(a)); /* u <- -u if negative. */
    cond_neg(a, toggle_hibit_t_over_s ^ ~hibit(a)); /* t/s <-? -t/s */
    
    gf_add(d,c,p->y);
    gf_mul(s,b,d);
    cond_neg(s, toggle_hibit_s ^ hibit(s));
#else
    /* More complicated because of rotation */
    /* MAGIC This code is wrong for certain non-Curve25519 curves;
     * check if it's because of Cofactor==8 or IMAGINE_ROTATION */
    
    gf c, d;
    gf_s *b = s, *a = minus_t_over_s;

    #if IMAGINE_TWIST
        gf x, t;
        gf_mul ( x, p->x, SQRT_MINUS_ONE);
        gf_mul ( t, p->t, SQRT_MINUS_ONE);
        gf_sub ( x, ZERO, x );
        gf_sub ( t, ZERO, t );
    
        gf_add ( a, p->z, x );
        gf_sub ( b, p->z, x );
        gf_mul ( c, a, b ); /* "zx" = Z^2 - aX^2 = Z^2 - X^2 */
    #else
        const gf_s *x = p->x, *t = p->t;
        /* Won't hit the cond_sel below because COFACTOR==8 requires IMAGINE_TWIST for now. */
    
        gf_sqr ( a, p->z );
        gf_sqr ( b, p->x );
        gf_add ( c, a, b ); /* "zx" = Z^2 - aX^2 = Z^2 + X^2 */
    #endif
    
    gf_mul ( a, p->z, t ); /* "tz" = T*Z */
    gf_sqr ( b, a );
    gf_mul ( d, b, c ); /* (TZ)^2 * (Z^2-aX^2) */
    mask_t ok = gf_isqrt_chk ( b, d, DECAF_TRUE );
    (void)ok; assert(ok);
    gf_mul ( d, b, a ); /* "osx" = 1 / sqrt(z^2-ax^2) */
    gf_mul ( a, b, c ); 
    gf_mul ( b, a, d ); /* 1/tz */

    mask_t rotate;
    #if (COFACTOR == 8)
        gf e;
        gf_sqr(e, p->z);
        gf_mul(a, e, b); /* z^2 / tz = z/t = 1/xy */
        rotate = hibit(a) ^ toggle_rotation;
        /* Curve25519: cond select between zx * 1/tz or sqrt(1-d); y=-x */
        gf_mul ( a, b, c ); 
        cond_sel ( a, a, SQRT_ONE_MINUS_D, rotate );
        cond_sel ( x, p->y, x, rotate );
    #else
        (void)toggle_rotation;
        rotate = 0;
    #endif
    
    gf_mul ( c, a, d ); // new "osx"
    gf_mul ( a, c, p->z );
    gf_add ( a, a, a ); // 2 * "osx" * Z
    mask_t tg1 = rotate ^ toggle_hibit_t_over_s ^~ hibit(a);
    cond_neg ( c, tg1 );
    cond_neg ( a, rotate ^ tg1 );
    gf_mul ( d, b, p->z );
    gf_add ( d, d, c );
    gf_mul ( b, d, x ); /* here "x" = y unless rotate */
    cond_neg ( b, toggle_hibit_s ^ hibit(b) );
    
#endif
}

void API_NS(point_encode)( unsigned char ser[SER_BYTES], const point_t p ) {
    gf s, mtos;
    deisogenize(s,mtos,p,0,0,0);
    gf_serialize ( ser, s );
}

decaf_error_t API_NS(point_decode) (
    point_t p,
    const unsigned char ser[SER_BYTES],
    decaf_bool_t allow_identity
) {
    gf s, a, b, c, d, e, f;
    mask_t succ = gf_deserialize(s, ser);
    mask_t zero = gf_eq(s, ZERO);
    succ &= bool_to_mask(allow_identity) | ~zero;
    succ &= ~hibit(s);
    gf_sqr ( a, s );
#if IMAGINE_TWIST
    gf_sub ( f, ONE, a ); /* f = 1-as^2 = 1-s^2*/
#else
    gf_add ( f, ONE, a ); /* f = 1-as^2 = 1+s^2 */
#endif
    succ &= ~ gf_eq( f, ZERO );
    gf_sqr ( b, f ); 
    gf_mulw_sgn ( c, a, 4*IMAGINE_TWIST-4*EDWARDS_D ); 
    gf_add ( c, c, b ); /* t^2 */
    gf_mul ( d, f, s ); /* s(1-as^2) for denoms */
    gf_sqr ( e, d );
    gf_mul ( b, c, e );
    
    succ &= gf_isqrt_chk ( e, b, DECAF_TRUE ); /* e = 1/(t s (1-as^2)) */
    gf_mul ( b, e, d ); /* 1/t */
    gf_mul ( d, e, c ); /* d = t / (s(1-as^2)) */
    gf_mul ( e, d, f ); /* t/s */
    mask_t negtos = hibit(e);
    cond_neg(b, negtos);
    cond_neg(d, negtos);

#if IMAGINE_TWIST
    gf_add ( p->z, ONE, a); /* Z = 1+as^2 = 1-s^2 */
#else
    gf_sub ( p->z, ONE, a); /* Z = 1+as^2 = 1-s^2 */
#endif

#if COFACTOR == 8
    gf_mul ( a, p->z, d); /* t(1+s^2) / s(1-s^2) = 2/xy */
    succ &= ~lobit(a); /* = ~hibit(a/2), since hibit(x) = lobit(2x) */
#endif
    
    gf_mul ( a, f, b ); /* y = (1-s^2) / t */
    gf_mul ( p->y, p->z, a ); /* Y = yZ */
#if IMAGINE_TWIST
    gf_add ( b, s, s );
    gf_mul(p->x, b, SQRT_MINUS_ONE); /* Curve25519 */
#else
    gf_add ( p->x, s, s );
#endif
    gf_mul ( p->t, p->x, a ); /* T = 2s (1-as^2)/t */
    
    p->y->limb[0] -= zero;
    
    assert(API_NS(point_valid)(p) | ~succ);
    
    return decaf_succeed_if(mask_to_bool(succ));
}

#if IMAGINE_TWIST
#define TWISTED_D (-(EDWARDS_D))
#else
#define TWISTED_D ((EDWARDS_D)-1)
#endif

#if TWISTED_D < 0
#define EFF_D (-(TWISTED_D))
#define NEG_D 1
#else
#define EFF_D TWISTED_D
#define NEG_D 0
#endif

void API_NS(point_sub) (
    point_t p,
    const point_t q,
    const point_t r
) {
    gf a, b, c, d;
    gf_sub_nr ( b, q->y, q->x );
    gf_sub_nr ( d, r->y, r->x );
    gf_add_nr ( c, r->y, r->x );
    gf_mul ( a, c, b );
    gf_add_nr ( b, q->y, q->x );
    gf_mul ( p->y, d, b );
    gf_mul ( b, r->t, q->t );
    gf_mulw_sgn ( p->x, b, 2*EFF_D );
    gf_add_nr ( b, a, p->y );
    gf_sub_nr ( c, p->y, a );
    gf_mul ( a, q->z, r->z );
    gf_add_nr ( a, a, a );
#if NEG_D
    gf_sub_nr ( p->y, a, p->x );
    gf_add_nr ( a, a, p->x );
#else
    gf_add_nr ( p->y, a, p->x );
    gf_sub_nr ( a, a, p->x );
#endif
    gf_mul ( p->z, a, p->y );
    gf_mul ( p->x, p->y, c );
    gf_mul ( p->y, a, b );
    gf_mul ( p->t, b, c );
}
    
void API_NS(point_add) (
    point_t p,
    const point_t q,
    const point_t r
) {
    gf a, b, c, d;
    gf_sub_nr ( b, q->y, q->x );
    gf_sub_nr ( c, r->y, r->x );
    gf_add_nr ( d, r->y, r->x );
    gf_mul ( a, c, b );
    gf_add_nr ( b, q->y, q->x );
    gf_mul ( p->y, d, b );
    gf_mul ( b, r->t, q->t );
    gf_mulw_sgn ( p->x, b, 2*EFF_D );
    gf_add_nr ( b, a, p->y );
    gf_sub_nr ( c, p->y, a );
    gf_mul ( a, q->z, r->z );
    gf_add_nr ( a, a, a );
#if NEG_D
    gf_add_nr ( p->y, a, p->x );
    gf_sub_nr ( a, a, p->x );
#else
    gf_sub_nr ( p->y, a, p->x );
    gf_add_nr ( a, a, p->x );
#endif
    gf_mul ( p->z, a, p->y );
    gf_mul ( p->x, p->y, c );
    gf_mul ( p->y, a, b );
    gf_mul ( p->t, b, c );
}

static NOINLINE void
point_double_internal (
    point_t p,
    const point_t q,
    int before_double
) {
    gf a, b, c, d;
    gf_sqr ( c, q->x );
    gf_sqr ( a, q->y );
    gf_add_nr ( d, c, a );
    gf_add_nr ( p->t, q->y, q->x );
    gf_sqr ( b, p->t );
    gf_subx_nr ( b, b, d, 3 );
    gf_sub_nr ( p->t, a, c );
    gf_sqr ( p->x, q->z );
    gf_add_nr ( p->z, p->x, p->x );
    gf_subx_nr ( a, p->z, p->t, 4 );
    gf_mul ( p->x, a, b );
    gf_mul ( p->z, p->t, a );
    gf_mul ( p->y, p->t, d );
    if (!before_double) gf_mul ( p->t, b, d );
}

void API_NS(point_double)(point_t p, const point_t q) {
    point_double_internal(p,q,0);
}

void API_NS(point_negate) (
   point_t nega,
   const point_t a
) {
    gf_sub(nega->x, ZERO, a->x);
    gf_copy(nega->y, a->y);
    gf_copy(nega->z, a->z);
    gf_sub(nega->t, ZERO, a->t);
}

static INLINE void
scalar_decode_short (
    scalar_t s,
    const unsigned char ser[SER_BYTES],
    unsigned int nbytes
) {
    unsigned int i,j,k=0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        decaf_word_t out = 0;
        for (j=0; j<sizeof(decaf_word_t) && k<nbytes; j++,k++) {
            out |= ((decaf_word_t)ser[k])<<(8*j);
        }
        s->limb[i] = out;
    }
}

decaf_error_t API_NS(scalar_decode)(
    scalar_t s,
    const unsigned char ser[SER_BYTES]
) {
    unsigned int i;
    scalar_decode_short(s, ser, SER_BYTES);
    decaf_dsword_t accum = 0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        accum = (accum + s->limb[i] - sc_p->limb[i]) >> WBITS;
    }
    /* Here accum == 0 or -1 */
    
    API_NS(scalar_mul)(s,s,API_NS(scalar_one)); /* ham-handed reduce */
    
    return decaf_succeed_if(~word_is_zero(accum));
}

void API_NS(scalar_destroy) (
    scalar_t scalar
) {
    decaf_bzero(scalar, sizeof(scalar_t));
}

static INLINE void ignore_result ( decaf_bool_t boo ) {
    (void)boo;
}

void API_NS(scalar_decode_long)(
    scalar_t s,
    const unsigned char *ser,
    size_t ser_len
) {
    if (ser_len == 0) {
        API_NS(scalar_copy)(s, API_NS(scalar_zero));
        return;
    }
    
    size_t i;
    scalar_t t1, t2;

    i = ser_len - (ser_len%SER_BYTES);
    if (i==ser_len) i -= SER_BYTES;
    
    scalar_decode_short(t1, &ser[i], ser_len-i);

    if (ser_len == sizeof(scalar_t)) {
        assert(i==0);
        /* ham-handed reduce */
        API_NS(scalar_mul)(s,t1,API_NS(scalar_one));
        API_NS(scalar_destroy)(t1);
        return;
    }

    while (i) {
        i -= SER_BYTES;
        sc_montmul(t1,t1,sc_r2);
        ignore_result( API_NS(scalar_decode)(t2, ser+i) );
        API_NS(scalar_add)(t1, t1, t2);
    }

    API_NS(scalar_copy)(s, t1);
    API_NS(scalar_destroy)(t1);
    API_NS(scalar_destroy)(t2);
}

void API_NS(scalar_encode)(
    unsigned char ser[SER_BYTES],
    const scalar_t s
) {
    unsigned int i,j,k=0;
    for (i=0; i<SCALAR_LIMBS; i++) {
        for (j=0; j<sizeof(decaf_word_t); j++,k++) {
            ser[k] = s->limb[i] >> (8*j);
        }
    }
}

/* Operations on [p]niels */
static INLINE void
cond_neg_niels (
    niels_t n,
    mask_t neg
) {
    cond_swap(n->a, n->b, neg);
    cond_neg(n->c, neg);
}

static NOINLINE void pt_to_pniels (
    pniels_t b,
    const point_t a
) {
    gf_sub ( b->n->a, a->y, a->x );
    gf_add ( b->n->b, a->x, a->y );
    gf_mulw_sgn ( b->n->c, a->t, 2*TWISTED_D );
    gf_add ( b->z, a->z, a->z );
}

static NOINLINE void pniels_to_pt (
    point_t e,
    const pniels_t d
) {
    gf eu;
    gf_add ( eu, d->n->b, d->n->a );
    gf_sub ( e->y, d->n->b, d->n->a );
    gf_mul ( e->t, e->y, eu);
    gf_mul ( e->x, d->z, e->y );
    gf_mul ( e->y, d->z, eu );
    gf_sqr ( e->z, d->z );
}

static NOINLINE void
niels_to_pt (
    point_t e,
    const niels_t n
) {
    gf_add ( e->y, n->b, n->a );
    gf_sub ( e->x, n->b, n->a );
    gf_mul ( e->t, e->y, e->x );
    gf_copy ( e->z, ONE );
}

static NOINLINE void
add_niels_to_pt (
    point_t d,
    const niels_t e,
    int before_double
) {
    gf a, b, c;
    gf_sub_nr ( b, d->y, d->x );
    gf_mul ( a, e->a, b );
    gf_add_nr ( b, d->x, d->y );
    gf_mul ( d->y, e->b, b );
    gf_mul ( d->x, e->c, d->t );
    gf_add_nr ( c, a, d->y );
    gf_sub_nr ( b, d->y, a );
    gf_sub_nr ( d->y, d->z, d->x );
    gf_add_nr ( a, d->x, d->z );
    gf_mul ( d->z, a, d->y );
    gf_mul ( d->x, d->y, b );
    gf_mul ( d->y, a, c );
    if (!before_double) gf_mul ( d->t, b, c );
}

static NOINLINE void
sub_niels_from_pt (
    point_t d,
    const niels_t e,
    int before_double
) {
    gf a, b, c;
    gf_sub_nr ( b, d->y, d->x );
    gf_mul ( a, e->b, b );
    gf_add_nr ( b, d->x, d->y );
    gf_mul ( d->y, e->a, b );
    gf_mul ( d->x, e->c, d->t );
    gf_add_nr ( c, a, d->y );
    gf_sub_nr ( b, d->y, a );
    gf_add_nr ( d->y, d->z, d->x );
    gf_sub_nr ( a, d->z, d->x );
    gf_mul ( d->z, a, d->y );
    gf_mul ( d->x, d->y, b );
    gf_mul ( d->y, a, c );
    if (!before_double) gf_mul ( d->t, b, c );
}

static void
add_pniels_to_pt (
    point_t p,
    const pniels_t pn,
    int before_double
) {
    gf L0;
    gf_mul ( L0, p->z, pn->z );
    gf_copy ( p->z, L0 );
    add_niels_to_pt( p, pn->n, before_double );
}

static void
sub_pniels_from_pt (
    point_t p,
    const pniels_t pn,
    int before_double
) {
    gf L0;
    gf_mul ( L0, p->z, pn->z );
    gf_copy ( p->z, L0 );
    sub_niels_from_pt( p, pn->n, before_double );
}

static INLINE void
constant_time_lookup_xx (
    void *__restrict__ out_,
    const void *table_,
    word_t elem_bytes,
    word_t n_table,
    word_t idx
) {
    constant_time_lookup(out_,table_,elem_bytes,n_table,idx);
}

static NOINLINE void
prepare_fixed_window(
    pniels_t *multiples,
    const point_t b,
    int ntable
) {
    point_t tmp;
    pniels_t pn;
    int i;
    
    point_double_internal(tmp, b, 0);
    pt_to_pniels(pn, tmp);
    pt_to_pniels(multiples[0], b);
    API_NS(point_copy)(tmp, b);
    for (i=1; i<ntable; i++) {
        add_pniels_to_pt(tmp, pn, 0);
        pt_to_pniels(multiples[i], tmp);
    }
    
    decaf_bzero(pn,sizeof(pn));
    decaf_bzero(tmp,sizeof(tmp));
}

void API_NS(point_scalarmul) (
    point_t a,
    const point_t b,
    const scalar_t scalar
) {
    const int WINDOW = DECAF_WINDOW_BITS,
        WINDOW_MASK = (1<<WINDOW)-1,
        WINDOW_T_MASK = WINDOW_MASK >> 1,
        NTABLE = 1<<(WINDOW-1);
        
    scalar_t scalar1x;
    API_NS(scalar_add)(scalar1x, scalar, point_scalarmul_adjustment);
    sc_halve(scalar1x,scalar1x,sc_p);
    
    /* Set up a precomputed table with odd multiples of b. */
    pniels_t pn, multiples[NTABLE];
    point_t tmp;
    prepare_fixed_window(multiples, b, NTABLE);

    /* Initialize. */
    int i,j,first=1;
    i = SCALAR_BITS - ((SCALAR_BITS-1) % WINDOW) - 1;

    for (; i>=0; i-=WINDOW) {
        /* Fetch another block of bits */
        word_t bits = scalar1x->limb[i/WBITS] >> (i%WBITS);
        if (i%WBITS >= WBITS-WINDOW && i/WBITS<SCALAR_LIMBS-1) {
            bits ^= scalar1x->limb[i/WBITS+1] << (WBITS - (i%WBITS));
        }
        bits &= WINDOW_MASK;
        mask_t inv = (bits>>(WINDOW-1))-1;
        bits ^= inv;
    
        /* Add in from table.  Compute t only on last iteration. */
        constant_time_lookup_xx(pn, multiples, sizeof(pn), NTABLE, bits & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv);
        if (first) {
            pniels_to_pt(tmp, pn);
            first = 0;
        } else {
           /* Using Hisil et al's lookahead method instead of extensible here
            * for no particular reason.  Double WINDOW times, but only compute t on
            * the last one.
            */
            for (j=0; j<WINDOW-1; j++)
                point_double_internal(tmp, tmp, -1);
            point_double_internal(tmp, tmp, 0);
            add_pniels_to_pt(tmp, pn, i ? -1 : 0);
        }
    }
    
    /* Write out the answer */
    API_NS(point_copy)(a,tmp);
    
    decaf_bzero(scalar1x,sizeof(scalar1x));
    decaf_bzero(pn,sizeof(pn));
    decaf_bzero(multiples,sizeof(multiples));
    decaf_bzero(tmp,sizeof(tmp));
}

void API_NS(point_double_scalarmul) (
    point_t a,
    const point_t b,
    const scalar_t scalarb,
    const point_t c,
    const scalar_t scalarc
) {
    const int WINDOW = DECAF_WINDOW_BITS,
        WINDOW_MASK = (1<<WINDOW)-1,
        WINDOW_T_MASK = WINDOW_MASK >> 1,
        NTABLE = 1<<(WINDOW-1);
        
    scalar_t scalar1x, scalar2x;
    API_NS(scalar_add)(scalar1x, scalarb, point_scalarmul_adjustment);
    sc_halve(scalar1x,scalar1x,sc_p);
    API_NS(scalar_add)(scalar2x, scalarc, point_scalarmul_adjustment);
    sc_halve(scalar2x,scalar2x,sc_p);
    
    /* Set up a precomputed table with odd multiples of b. */
    pniels_t pn, multiples1[NTABLE], multiples2[NTABLE];
    point_t tmp;
    prepare_fixed_window(multiples1, b, NTABLE);
    prepare_fixed_window(multiples2, c, NTABLE);

    /* Initialize. */
    int i,j,first=1;
    i = SCALAR_BITS - ((SCALAR_BITS-1) % WINDOW) - 1;

    for (; i>=0; i-=WINDOW) {
        /* Fetch another block of bits */
        word_t bits1 = scalar1x->limb[i/WBITS] >> (i%WBITS),
                     bits2 = scalar2x->limb[i/WBITS] >> (i%WBITS);
        if (i%WBITS >= WBITS-WINDOW && i/WBITS<SCALAR_LIMBS-1) {
            bits1 ^= scalar1x->limb[i/WBITS+1] << (WBITS - (i%WBITS));
            bits2 ^= scalar2x->limb[i/WBITS+1] << (WBITS - (i%WBITS));
        }
        bits1 &= WINDOW_MASK;
        bits2 &= WINDOW_MASK;
        mask_t inv1 = (bits1>>(WINDOW-1))-1;
        mask_t inv2 = (bits2>>(WINDOW-1))-1;
        bits1 ^= inv1;
        bits2 ^= inv2;
    
        /* Add in from table.  Compute t only on last iteration. */
        constant_time_lookup_xx(pn, multiples1, sizeof(pn), NTABLE, bits1 & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv1);
        if (first) {
            pniels_to_pt(tmp, pn);
            first = 0;
        } else {
           /* Using Hisil et al's lookahead method instead of extensible here
            * for no particular reason.  Double WINDOW times, but only compute t on
            * the last one.
            */
            for (j=0; j<WINDOW-1; j++)
                point_double_internal(tmp, tmp, -1);
            point_double_internal(tmp, tmp, 0);
            add_pniels_to_pt(tmp, pn, 0);
        }
        constant_time_lookup_xx(pn, multiples2, sizeof(pn), NTABLE, bits2 & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv2);
        add_pniels_to_pt(tmp, pn, i?-1:0);
    }
    
    /* Write out the answer */
    API_NS(point_copy)(a,tmp);
    

    decaf_bzero(scalar1x,sizeof(scalar1x));
    decaf_bzero(scalar2x,sizeof(scalar2x));
    decaf_bzero(pn,sizeof(pn));
    decaf_bzero(multiples1,sizeof(multiples1));
    decaf_bzero(multiples2,sizeof(multiples2));
    decaf_bzero(tmp,sizeof(tmp));
}

void API_NS(point_dual_scalarmul) (
    point_t a1,
    point_t a2,
    const point_t b,
    const scalar_t scalar1,
    const scalar_t scalar2
) {
    const int WINDOW = DECAF_WINDOW_BITS,
        WINDOW_MASK = (1<<WINDOW)-1,
        WINDOW_T_MASK = WINDOW_MASK >> 1,
        NTABLE = 1<<(WINDOW-1);
        
    scalar_t scalar1x, scalar2x;
    API_NS(scalar_add)(scalar1x, scalar1, point_scalarmul_adjustment);
    sc_halve(scalar1x,scalar1x,sc_p);
    API_NS(scalar_add)(scalar2x, scalar2, point_scalarmul_adjustment);
    sc_halve(scalar2x,scalar2x,sc_p);
    
    /* Set up a precomputed table with odd multiples of b. */
    point_t multiples1[NTABLE], multiples2[NTABLE], working, tmp;
    pniels_t pn;
    
    API_NS(point_copy)(working, b);

    /* Initialize. */
    int i,j;
    
    for (i=0; i<NTABLE; i++) {
        API_NS(point_copy)(multiples1[i], API_NS(point_identity));
        API_NS(point_copy)(multiples2[i], API_NS(point_identity));
    }

    for (i=0; i<SCALAR_BITS; i+=WINDOW) {   
        if (i) {
            for (j=0; j<WINDOW-1; j++)
                point_double_internal(working, working, -1);
            point_double_internal(working, working, 0);
        }
        
        /* Fetch another block of bits */
        word_t bits1 = scalar1x->limb[i/WBITS] >> (i%WBITS),
               bits2 = scalar2x->limb[i/WBITS] >> (i%WBITS);
        if (i%WBITS >= WBITS-WINDOW && i/WBITS<SCALAR_LIMBS-1) {
            bits1 ^= scalar1x->limb[i/WBITS+1] << (WBITS - (i%WBITS));
            bits2 ^= scalar2x->limb[i/WBITS+1] << (WBITS - (i%WBITS));
        }
        bits1 &= WINDOW_MASK;
        bits2 &= WINDOW_MASK;
        mask_t inv1 = (bits1>>(WINDOW-1))-1;
        mask_t inv2 = (bits2>>(WINDOW-1))-1;
        bits1 ^= inv1;
        bits2 ^= inv2;
        
        pt_to_pniels(pn, working);

        constant_time_lookup_xx(tmp, multiples1, sizeof(tmp), NTABLE, bits1 & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv1);
        /* add_pniels_to_pt(multiples1[bits1 & WINDOW_T_MASK], pn, 0); */
        add_pniels_to_pt(tmp, pn, 0);
        constant_time_insert(multiples1, tmp, sizeof(tmp), NTABLE, bits1 & WINDOW_T_MASK);
        
        
        constant_time_lookup_xx(tmp, multiples2, sizeof(tmp), NTABLE, bits2 & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv1^inv2);
        /* add_pniels_to_pt(multiples2[bits2 & WINDOW_T_MASK], pn, 0); */
        add_pniels_to_pt(tmp, pn, 0);
        constant_time_insert(multiples2, tmp, sizeof(tmp), NTABLE, bits2 & WINDOW_T_MASK);
    }
    
    if (NTABLE > 1) {
        API_NS(point_copy)(working, multiples1[NTABLE-1]);
        API_NS(point_copy)(tmp    , multiples2[NTABLE-1]);
    
        for (i=NTABLE-1; i>1; i--) {
            API_NS(point_add)(multiples1[i-1], multiples1[i-1], multiples1[i]);
            API_NS(point_add)(multiples2[i-1], multiples2[i-1], multiples2[i]);
            API_NS(point_add)(working, working, multiples1[i-1]);
            API_NS(point_add)(tmp,     tmp,     multiples2[i-1]);
        }
    
        API_NS(point_add)(multiples1[0], multiples1[0], multiples1[1]);
        API_NS(point_add)(multiples2[0], multiples2[0], multiples2[1]);
        point_double_internal(working, working, 0);
        point_double_internal(tmp,         tmp, 0);
        API_NS(point_add)(a1, working, multiples1[0]);
        API_NS(point_add)(a2, tmp,     multiples2[0]);
    } else {
        API_NS(point_copy)(a1, multiples1[0]);
        API_NS(point_copy)(a2, multiples2[0]);
    }

    decaf_bzero(scalar1x,sizeof(scalar1x));
    decaf_bzero(scalar2x,sizeof(scalar2x));
    decaf_bzero(pn,sizeof(pn));
    decaf_bzero(multiples1,sizeof(multiples1));
    decaf_bzero(multiples2,sizeof(multiples2));
    decaf_bzero(tmp,sizeof(tmp));
    decaf_bzero(working,sizeof(working));
}

decaf_bool_t API_NS(point_eq) ( const point_t p, const point_t q ) {
    /* equality mod 2-torsion compares x/y */
    gf a, b;
    gf_mul ( a, p->y, q->x );
    gf_mul ( b, q->y, p->x );
    mask_t succ = gf_eq(a,b);
    
    #if (COFACTOR == 8) && IMAGINE_TWIST
        gf_mul ( a, p->y, q->y );
        gf_mul ( b, q->x, p->x );
        #if !(IMAGINE_TWIST)
            gf_sub ( a, ZERO, a );
        #else
           /* Interesting note: the 4tor would normally be rotation.
            * But because of the *i twist, it's actually
            * (x,y) <-> (iy,ix)
            */
    
           /* No code, just a comment. */
        #endif
        succ |= gf_eq(a,b);
    #endif
    
    return mask_to_bool(succ);
}

void API_NS(point_from_hash_nonuniform) (
    point_t p,
    const unsigned char ser[SER_BYTES]
) {
    gf r0,r,a,b,c,N,e;
    gf_deserialize(r0,ser);
    gf_strong_reduce(r0);
    gf_sqr(a,r0);
#if P_MOD_8 == 5
    /* r = QNR * r0^2 */
    gf_mul(r,a,SQRT_MINUS_ONE);
#elif P_MOD_8 == 3 || P_MOD_8 == 7
    gf_sub(r,ZERO,a);
#else
#error "Only supporting p=3,5,7 mod 8"
#endif

    /* Compute D@c := (dr+a-d)(dr-ar-d) with a=1 */
    gf_sub(a,r,ONE);
    gf_mulw_sgn(b,a,EDWARDS_D); /* dr-d */
    gf_add(a,b,ONE);
    gf_sub(b,b,r);
    gf_mul(c,a,b);
    
    /* compute N := (r+1)(a-2d) */
    gf_add(a,r,ONE);
    gf_mulw_sgn(N,a,1-2*EDWARDS_D);
    
    /* e = +-sqrt(1/ND) or +-r0 * sqrt(qnr/ND) */
    gf_mul(a,c,N);
    mask_t square = gf_isqrt_chk(b,a,DECAF_FALSE);
    cond_sel(c,r0,ONE,square); /* r? = square ? 1 : r0 */
    gf_mul(e,b,c);
    
    /* s@a = +-|N.e| */
    gf_mul(a,N,e);
    cond_neg(a,hibit(a)^square); /* NB this is - what is listen in the paper */
    
    /* t@b = -+ cN(r-1)((a-2d)e)^2 - 1 */
    gf_mulw_sgn(c,e,1-2*EDWARDS_D); /* (a-2d)e */
    gf_sqr(b,c);
    gf_sub(e,r,ONE);
    gf_mul(c,b,e);
    gf_mul(b,c,N);
    cond_neg(b,square);
    gf_sub(b,b,ONE);

    /* isogenize */
#if IMAGINE_TWIST
    gf_mul(c,a,SQRT_MINUS_ONE);
    gf_copy(a,c);
#endif
    
    gf_sqr(c,a); /* s^2 */
    gf_add(a,a,a); /* 2s */
    gf_add(e,c,ONE);
    gf_mul(p->t,a,e); /* 2s(1+s^2) */
    gf_mul(p->x,a,b); /* 2st */
    gf_sub(a,ONE,c);
    gf_mul(p->y,e,a); /* (1+s^2)(1-s^2) */
    gf_mul(p->z,a,b); /* (1-s^2)t */
    
    assert(API_NS(point_valid)(p));
}

decaf_error_t
API_NS(invert_elligator_nonuniform) (
    unsigned char recovered_hash[SER_BYTES],
    const point_t p,
    uint16_t hint_
) {
    mask_t hint = hint_;
    mask_t sgn_s = -(hint & 1),
        sgn_t_over_s = -(hint>>1 & 1),
        sgn_r0 = -(hint>>2 & 1),
        sgn_ed_T = -(hint>>3 & 1);
    gf a, b, c, d;
    deisogenize(a,c,p,sgn_s,sgn_t_over_s,sgn_ed_T);
    
    /* ok, a = s; c = -t/s */
    gf_mul(b,c,a);
    gf_sub(b,ONE,b); /* t+1 */
    gf_sqr(c,a); /* s^2 */
    mask_t is_identity = gf_eq(p->t,ZERO);
    {
        /* identity adjustments */
        /* in case of identity, currently c=0, t=0, b=1, will encode to 1 */
        /* if hint is 0, -> 0 */
        /* if hint is to neg t/s, then go to infinity, effectively set s to 1 */
        cond_sel(c,c,ONE,is_identity & sgn_t_over_s);
        cond_sel(b,b,ZERO,is_identity & ~sgn_t_over_s & ~sgn_s); /* identity adjust */        
    }
    gf_mulw_sgn(d,c,2*EDWARDS_D-1); /* $d = (2d-a)s^2 */
    gf_add(a,b,d); /* num? */
    gf_sub(d,d,b); /* den? */
    gf_mul(b,a,d); /* n*d */
    cond_sel(a,d,a,sgn_s);
#if P_MOD_8 == 5
    gf_mul(d,b,SQRT_MINUS_ONE);
#else
    gf_sub(d,ZERO,b);
#endif
    mask_t succ = gf_isqrt_chk(c,d,DECAF_TRUE);
    gf_mul(b,a,c);
    cond_neg(b, sgn_r0^hibit(b));
    
    succ &= ~(gf_eq(b,ZERO) & sgn_r0);
#if COFACTOR == 8
    succ &= ~(is_identity & sgn_ed_T); /* NB: there are no preimages of rotated identity. */
#endif
    
    gf_serialize(recovered_hash, b); 
    /* TODO: deal with overflow flag */
    return decaf_succeed_if(mask_to_bool(succ));
}

void API_NS(point_from_hash_uniform) (
    point_t pt,
    const unsigned char hashed_data[2*SER_BYTES]
) {
    point_t pt2;
    API_NS(point_from_hash_nonuniform)(pt,hashed_data);
    API_NS(point_from_hash_nonuniform)(pt2,&hashed_data[SER_BYTES]);
    API_NS(point_add)(pt,pt,pt2);
}

decaf_error_t
API_NS(invert_elligator_uniform) (
    unsigned char partial_hash[2*SER_BYTES],
    const point_t p,
    uint16_t hint
) {
    point_t pt2;
    API_NS(point_from_hash_nonuniform)(pt2,&partial_hash[SER_BYTES]);
    API_NS(point_sub)(pt2,p,pt2);
    return API_NS(invert_elligator_nonuniform)(partial_hash,pt2,hint);
}

decaf_bool_t API_NS(point_valid) (
    const point_t p
) {
    gf a,b,c;
    gf_mul(a,p->x,p->y);
    gf_mul(b,p->z,p->t);
    mask_t out = gf_eq(a,b);
    gf_sqr(a,p->x);
    gf_sqr(b,p->y);
    gf_sub(a,b,a);
    gf_sqr(b,p->t);
    gf_mulw_sgn(c,b,TWISTED_D);
    gf_sqr(b,p->z);
    gf_add(b,b,c);
    out &= gf_eq(a,b);
    out &= ~gf_eq(p->z,ZERO);
    return mask_to_bool(out);
}

void API_NS(point_debugging_torque) (
    point_t q,
    const point_t p
) {
#if COFACTOR == 8
    gf tmp;
    gf_mul(tmp,p->x,SQRT_MINUS_ONE);
    gf_mul(q->x,p->y,SQRT_MINUS_ONE);
    gf_copy(q->y,tmp);
    gf_copy(q->z,p->z);
    gf_sub(q->t,ZERO,p->t);
#else
    gf_sub(q->x,ZERO,p->x);
    gf_sub(q->y,ZERO,p->y);
    gf_copy(q->z,p->z);
    gf_copy(q->t,p->t);
#endif
}

void API_NS(point_debugging_pscale) (
    point_t q,
    const point_t p,
    const uint8_t factor[SER_BYTES]
) {
    gf gfac,tmp;
    ignore_result(gf_deserialize(gfac,factor));
    cond_sel(gfac,gfac,ONE,gf_eq(gfac,ZERO));
    gf_mul(tmp,p->x,gfac);
    gf_copy(q->x,tmp);
    gf_mul(tmp,p->y,gfac);
    gf_copy(q->y,tmp);
    gf_mul(tmp,p->z,gfac);
    gf_copy(q->z,tmp);
    gf_mul(tmp,p->t,gfac);
    gf_copy(q->t,tmp);
}

static void gf_batch_invert (
    gf *__restrict__ out,
    const gf *in,
    unsigned int n
) {
    gf t1;
    assert(n>1);
  
    gf_copy(out[1], in[0]);
    int i;
    for (i=1; i<(int) (n-1); i++) {
        gf_mul(out[i+1], out[i], in[i]);
    }
    gf_mul(out[0], out[n-1], in[n-1]);

    gf_invert(out[0], out[0]);

    for (i=n-1; i>0; i--) {
        gf_mul(t1, out[i], out[0]);
        gf_copy(out[i], t1);
        gf_mul(t1, out[0], in[i]);
        gf_copy(out[0], t1);
    }
}

static void batch_normalize_niels (
    niels_t *table,
    const gf *zs,
    gf *__restrict__ zis,
    int n
) {
    int i;
    gf product;
    gf_batch_invert(zis, zs, n);

    for (i=0; i<n; i++) {
        gf_mul(product, table[i]->a, zis[i]);
        gf_strong_reduce(product);
        gf_copy(table[i]->a, product);
        
        gf_mul(product, table[i]->b, zis[i]);
        gf_strong_reduce(product);
        gf_copy(table[i]->b, product);
        
        gf_mul(product, table[i]->c, zis[i]);
        gf_strong_reduce(product);
        gf_copy(table[i]->c, product);
    }
    
    decaf_bzero(product,sizeof(product));
}

void API_NS(precompute) (
    precomputed_s *table,
    const point_t base
) { 
    const unsigned int n = COMBS_N, t = COMBS_T, s = COMBS_S;
    assert(n*t*s >= SCALAR_BITS);
  
    point_t working, start, doubles[t-1];
    API_NS(point_copy)(working, base);
    pniels_t pn_tmp;
  
    gf zs[n<<(t-1)], zis[n<<(t-1)];
  
    unsigned int i,j,k;
    
    /* Compute n tables */
    for (i=0; i<n; i++) {

        /* Doubling phase */
        for (j=0; j<t; j++) {
            if (j) API_NS(point_add)(start, start, working);
            else API_NS(point_copy)(start, working);

            if (j==t-1 && i==n-1) break;

            point_double_internal(working, working,0);
            if (j<t-1) API_NS(point_copy)(doubles[j], working);

            for (k=0; k<s-1; k++)
                point_double_internal(working, working, k<s-2);
        }

        /* Gray-code phase */
        for (j=0;; j++) {
            int gray = j ^ (j>>1);
            int idx = (((i+1)<<(t-1))-1) ^ gray;

            pt_to_pniels(pn_tmp, start);
            memcpy(table->table[idx], pn_tmp->n, sizeof(pn_tmp->n));
            gf_copy(zs[idx], pn_tmp->z);
			
            if (j >= (1u<<(t-1)) - 1) break;
            int delta = (j+1) ^ ((j+1)>>1) ^ gray;

            for (k=0; delta>1; k++)
                delta >>=1;
            
            if (gray & (1<<k)) {
                API_NS(point_add)(start, start, doubles[k]);
            } else {
                API_NS(point_sub)(start, start, doubles[k]);
            }
        }
    }
    
    batch_normalize_niels(table->table,(const gf *)zs,zis,n<<(t-1));
    
    decaf_bzero(zs,sizeof(zs));
    decaf_bzero(zis,sizeof(zis));
    decaf_bzero(pn_tmp,sizeof(pn_tmp));
    decaf_bzero(working,sizeof(working));
    decaf_bzero(start,sizeof(start));
    decaf_bzero(doubles,sizeof(doubles));
}

static INLINE void
constant_time_lookup_xx_niels (
    niels_s *__restrict__ ni,
    const niels_t *table,
    int nelts,
    int idx
) {
    constant_time_lookup_xx(ni, table, sizeof(niels_s), nelts, idx);
}

void API_NS(precomputed_scalarmul) (
    point_t out,
    const precomputed_s *table,
    const scalar_t scalar
) {
    int i;
    unsigned j,k;
    const unsigned int n = COMBS_N, t = COMBS_T, s = COMBS_S;
    
    scalar_t scalar1x;
    API_NS(scalar_add)(scalar1x, scalar, precomputed_scalarmul_adjustment);
    sc_halve(scalar1x,scalar1x,sc_p);
    
    niels_t ni;
    
    for (i=s-1; i>=0; i--) {
        if (i != (int)s-1) point_double_internal(out,out,0);
        
        for (j=0; j<n; j++) {
            int tab = 0;
         
            for (k=0; k<t; k++) {
                unsigned int bit = i + s*(k + j*t);
                if (bit < SCALAR_BITS) {
                    tab |= (scalar1x->limb[bit/WBITS] >> (bit%WBITS) & 1) << k;
                }
            }
            
            mask_t invert = (tab>>(t-1))-1;
            tab ^= invert;
            tab &= (1<<(t-1)) - 1;

            constant_time_lookup_xx_niels(ni, &table->table[j<<(t-1)], 1<<(t-1), tab);

            cond_neg_niels(ni, invert);
            if ((i!=(int)s-1)||j) {
                add_niels_to_pt(out, ni, j==n-1 && i);
            } else {
                niels_to_pt(out, ni);
            }
        }
    }
    
    decaf_bzero(ni,sizeof(ni));
    decaf_bzero(scalar1x,sizeof(scalar1x));
}

void API_NS(point_cond_sel) (
    point_t out,
    const point_t a,
    const point_t b,
    decaf_bool_t pick_b
) {
    constant_time_select(out,a,b,sizeof(point_t),bool_to_mask(pick_b),0);
}

void API_NS(scalar_cond_sel) (
    scalar_t out,
    const scalar_t a,
    const scalar_t b,
    decaf_bool_t pick_b
) {
    constant_time_select(out,a,b,sizeof(scalar_t),bool_to_mask(pick_b),sizeof(out->limb[0]));
}

/* FUTURE: restore Curve25519 Montgomery ladder? */
decaf_error_t API_NS(direct_scalarmul) (
    uint8_t scaled[SER_BYTES],
    const uint8_t base[SER_BYTES],
    const scalar_t scalar,
    decaf_bool_t allow_identity,
    decaf_bool_t short_circuit
) {
    point_t basep;
    decaf_error_t succ = API_NS(point_decode)(basep, base, allow_identity);
    if (short_circuit && succ != DECAF_SUCCESS) return succ;
    API_NS(point_cond_sel)(basep, API_NS(point_base), basep, succ);
    API_NS(point_scalarmul)(basep, basep, scalar);
    API_NS(point_encode)(scaled, basep);
    API_NS(point_destroy)(basep);
    return succ;
}

decaf_error_t API_NS(x_direct_scalarmul) (
    uint8_t out[X_PUBLIC_BYTES],
    const uint8_t base[X_PUBLIC_BYTES],
    const uint8_t scalar[X_PRIVATE_BYTES]
) {
    gf x1, x2, z2, x3, z3, t1, t2;
    ignore_result(gf_deserialize(x1,base));
    gf_copy(x2,ONE);
    gf_copy(z2,ZERO);
    gf_copy(x3,x1);
    gf_copy(z3,ONE);
    
    int t;
    mask_t swap = 0;
    
    for (t = X_PRIVATE_BITS-1; t>=0; t--) {
        uint8_t sb = scalar[t/8];
        
        /* Scalar conditioning */
        if (t/8==0) sb &= -(uint8_t)COFACTOR;
        else if (t == X_PRIVATE_BITS-1) sb = -1;
        
        mask_t k_t = (sb>>(t%8)) & 1;
        k_t = -k_t; /* set to all 0s or all 1s */
        
        swap ^= k_t;
        cond_swap(x2,x3,swap);
        cond_swap(z2,z3,swap);
        swap = k_t;
        
        gf_add_nr(t1,x2,z2); /* A = x2 + z2 */
        gf_sub_nr(t2,x2,z2); /* B = x2 - z2 */
        gf_sub_nr(z2,x3,z3); /* D = x3 - z3 */
        gf_mul(x2,t1,z2);    /* DA */
        gf_add_nr(z2,z3,x3); /* C = x3 + z3 */
        gf_mul(x3,t2,z2);    /* CB */
        gf_sub_nr(z3,x2,x3); /* DA-CB */
        gf_sqr(z2,z3);       /* (DA-CB)^2 */
        gf_mul(z3,x1,z2);    /* z3 = x1(DA-CB)^2 */
        gf_add_nr(z2,x2,x3); /* (DA+CB) */
        gf_sqr(x3,z2);       /* x3 = (DA+CB)^2 */
        
        gf_sqr(z2,t1);       /* AA = A^2 */
        gf_sqr(t1,t2);       /* BB = B^2 */
        gf_mul(x2,z2,t1);    /* x2 = AA*BB */
        gf_sub_nr(t2,z2,t1); /* E = AA-BB */
        
        gf_mulw_sgn(t1,t2,-EDWARDS_D); /* E*-d = a24*E */
        gf_add_nr(t1,t1,z2); /* AA + a24*E */
        gf_mul(z2,t2,t1); /* z2 = E(AA+a24*E) */
    }
    
    /* Finish */
    cond_swap(x2,x3,swap);
    cond_swap(z2,z3,swap);
    gf_invert(z2,z2);
    gf_mul(x1,x2,z2);
    gf_serialize(out,x1);
    mask_t nz = ~gf_eq(x1,ZERO);
    
    decaf_bzero(x1,sizeof(x1));
    decaf_bzero(x2,sizeof(x2));
    decaf_bzero(z2,sizeof(z2));
    decaf_bzero(x3,sizeof(x3));
    decaf_bzero(z3,sizeof(z3));
    decaf_bzero(t1,sizeof(t1));
    decaf_bzero(t2,sizeof(t2));
    
    return decaf_succeed_if(mask_to_bool(nz));
}

void API_NS(x_base_scalarmul) (
    uint8_t out[X_PUBLIC_BYTES],
    const uint8_t scalar[X_PRIVATE_BYTES]
) {
    /* Scalar conditioning */
    uint8_t scalar2[X_PRIVATE_BYTES];
    memcpy(scalar2,scalar,sizeof(scalar2));
    scalar2[0] &= -(uint8_t)COFACTOR;
    
    scalar2[X_PRIVATE_BYTES-1] &= ~(-1<<((X_PRIVATE_BITS+7)%8));
    scalar2[X_PRIVATE_BYTES-1] |= 1<<((X_PRIVATE_BITS+7)%8);
    
    scalar_t the_scalar;
    API_NS(scalar_decode_long)(the_scalar,scalar2,sizeof(scalar2));
    
    /* We're gonna isogenize by 2, so divide by 2.
     *
     * Why by 2, even though it's a 4-isogeny?
     *
     * The isogeny map looks like
     * Montgomery <-2-> Jacobi <-2-> Edwards
     *
     * Since the Jacobi base point is the PREimage of the iso to
     * the Montgomery curve, and we're going
     * Jacobi -> Edwards -> Jacobi -> Montgomery,
     * we pick up only a factor of 2 over Jacobi -> Montgomery. 
     */
    sc_halve(the_scalar,the_scalar,sc_p);
#if COFACTOR==8
    /* If the base point isn't in the prime-order subgroup (PERF:
     * guarantee that it is?) then a 4-isogeny isn't necessarily
     * enough to clear the cofactor.  So add another doubling.
     */
    sc_halve(the_scalar,the_scalar,sc_p);
#endif
    point_t p;
    API_NS(precomputed_scalarmul)(p,API_NS(precomputed_base),the_scalar);
#if COFACTOR==8
    API_NS(point_double)(p,p);
#endif
    
    /* Isogenize to Montgomery curve */
    gf_invert(p->t,p->x); /* 1/x */
    gf_mul(p->z,p->t,p->y); /* y/x */
    gf_sqr(p->y,p->z); /* (y/x)^2 */
#if IMAGINE_TWIST
    gf_sub(p->y,ZERO,p->y);
#endif
    gf_serialize(out,p->y);
        
    decaf_bzero(scalar2,sizeof(scalar2));
    API_NS(scalar_destroy)(the_scalar);
    API_NS(point_destroy)(p);
}

/**
 * @cond internal
 * Control for variable-time scalar multiply algorithms.
 */
struct smvt_control {
  int power, addend;
};

static int recode_wnaf (
    struct smvt_control *control, /* [nbits/(tableBits+1) + 3] */
    const scalar_t scalar,
    unsigned int tableBits
) {
    unsigned int table_size = SCALAR_BITS/(tableBits+1) + 3;
    int position = table_size - 1; /* at the end */
    
    /* place the end marker */
    control[position].power = -1;
    control[position].addend = 0;
    position--;

    /* PERF: Could negate scalar if it's large.  But then would need more cases
     * in the actual code that uses it, all for an expected reduction of like 1/5 op.
     * Probably not worth it.
     */
    
    uint64_t current = scalar->limb[0] & 0xFFFF;
    uint32_t mask = (1<<(tableBits+1))-1;

    unsigned int w;
    const unsigned int B_OVER_16 = sizeof(scalar->limb[0]) / 2;
    for (w = 1; w<(SCALAR_BITS-1)/16+3; w++) {
        if (w < (SCALAR_BITS-1)/16+1) {
            /* Refill the 16 high bits of current */
            current += (uint32_t)((scalar->limb[w/B_OVER_16]>>(16*(w%B_OVER_16)))<<16);
        }
        
        while (current & 0xFFFF) {
            assert(position >= 0);
            uint32_t pos = __builtin_ctz((uint32_t)current), odd = (uint32_t)current >> pos;
            int32_t delta = odd & mask;
            if (odd & 1<<(tableBits+1)) delta -= (1<<(tableBits+1));
            current -= delta << pos;
            control[position].power = pos + 16*(w-1);
            control[position].addend = delta;
            position--;
        }
        current >>= 16;
    }
    assert(current==0);
    
    position++;
    unsigned int n = table_size - position;
    unsigned int i;
    for (i=0; i<n; i++) {
        control[i] = control[i+position];
    }
    return n-1;
}

static void
prepare_wnaf_table(
    pniels_t *output,
    const point_t working,
    unsigned int tbits
) {
    point_t tmp;
    int i;
    pt_to_pniels(output[0], working);

    if (tbits == 0) return;

    API_NS(point_double)(tmp,working);
    pniels_t twop;
    pt_to_pniels(twop, tmp);

    add_pniels_to_pt(tmp, output[0],0);
    pt_to_pniels(output[1], tmp);

    for (i=2; i < 1<<tbits; i++) {
        add_pniels_to_pt(tmp, twop,0);
        pt_to_pniels(output[i], tmp);
    }
    
    API_NS(point_destroy)(tmp);
}

extern const gf API_NS(precomputed_wnaf_as_fe)[];
static const niels_t *API_NS(wnaf_base) = (const niels_t *)API_NS(precomputed_wnaf_as_fe);
const size_t API_NS(sizeof_precomputed_wnafs) __attribute((visibility("hidden")))
    = sizeof(niels_t)<<DECAF_WNAF_FIXED_TABLE_BITS;

void API_NS(precompute_wnafs) (
    niels_t out[1<<DECAF_WNAF_FIXED_TABLE_BITS],
    const point_t base
) __attribute__ ((visibility ("hidden")));

void API_NS(precompute_wnafs) (
    niels_t out[1<<DECAF_WNAF_FIXED_TABLE_BITS],
    const point_t base
) {
    pniels_t tmp[1<<DECAF_WNAF_FIXED_TABLE_BITS];
    gf zs[1<<DECAF_WNAF_FIXED_TABLE_BITS], zis[1<<DECAF_WNAF_FIXED_TABLE_BITS];
    int i;
    prepare_wnaf_table(tmp,base,DECAF_WNAF_FIXED_TABLE_BITS);
    for (i=0; i<1<<DECAF_WNAF_FIXED_TABLE_BITS; i++) {
        memcpy(out[i], tmp[i]->n, sizeof(niels_t));
        gf_copy(zs[i], tmp[i]->z);
    }
    batch_normalize_niels(out, (const gf *)zs, zis, 1<<DECAF_WNAF_FIXED_TABLE_BITS);
    
    decaf_bzero(tmp,sizeof(tmp));
    decaf_bzero(zs,sizeof(zs));
    decaf_bzero(zis,sizeof(zis));
}

void API_NS(base_double_scalarmul_non_secret) (
    point_t combo,
    const scalar_t scalar1,
    const point_t base2,
    const scalar_t scalar2
) {
    const int table_bits_var = DECAF_WNAF_VAR_TABLE_BITS,
        table_bits_pre = DECAF_WNAF_FIXED_TABLE_BITS;
    struct smvt_control control_var[SCALAR_BITS/(table_bits_var+1)+3];
    struct smvt_control control_pre[SCALAR_BITS/(table_bits_pre+1)+3];
    
    int ncb_pre = recode_wnaf(control_pre, scalar1, table_bits_pre);
    int ncb_var = recode_wnaf(control_var, scalar2, table_bits_var);
  
    pniels_t precmp_var[1<<table_bits_var];
    prepare_wnaf_table(precmp_var, base2, table_bits_var);
  
    int contp=0, contv=0, i = control_var[0].power;

    if (i < 0) {
        API_NS(point_copy)(combo, API_NS(point_identity));
        return;
    } else if (i > control_pre[0].power) {
        pniels_to_pt(combo, precmp_var[control_var[0].addend >> 1]);
        contv++;
    } else if (i == control_pre[0].power && i >=0 ) {
        pniels_to_pt(combo, precmp_var[control_var[0].addend >> 1]);
        add_niels_to_pt(combo, API_NS(wnaf_base)[control_pre[0].addend >> 1], i);
        contv++; contp++;
    } else {
        i = control_pre[0].power;
        niels_to_pt(combo, API_NS(wnaf_base)[control_pre[0].addend >> 1]);
        contp++;
    }
    
    for (i--; i >= 0; i--) {
        int cv = (i==control_var[contv].power), cp = (i==control_pre[contp].power);
        point_double_internal(combo,combo,i && !(cv||cp));

        if (cv) {
            assert(control_var[contv].addend);

            if (control_var[contv].addend > 0) {
                add_pniels_to_pt(combo, precmp_var[control_var[contv].addend >> 1], i&&!cp);
            } else {
                sub_pniels_from_pt(combo, precmp_var[(-control_var[contv].addend) >> 1], i&&!cp);
            }
            contv++;
        }

        if (cp) {
            assert(control_pre[contp].addend);

            if (control_pre[contp].addend > 0) {
                add_niels_to_pt(combo, API_NS(wnaf_base)[control_pre[contp].addend >> 1], i);
            } else {
                sub_niels_from_pt(combo, API_NS(wnaf_base)[(-control_pre[contp].addend) >> 1], i);
            }
            contp++;
        }
    }
    
    /* This function is non-secret, but whatever this is cheap. */
    decaf_bzero(control_var,sizeof(control_var));
    decaf_bzero(control_pre,sizeof(control_pre));
    decaf_bzero(precmp_var,sizeof(precmp_var));

    assert(contv == ncb_var); (void)ncb_var;
    assert(contp == ncb_pre); (void)ncb_pre;
}

void API_NS(point_destroy) (
    point_t point
) {
    decaf_bzero(point, sizeof(point_t));
}

void API_NS(precomputed_destroy) (
    precomputed_s *pre
) {
    decaf_bzero(pre, API_NS(sizeof_precomputed_s));
}
