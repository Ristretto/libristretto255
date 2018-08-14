/**
 * @file ristretto.c
 * @author Mike Hamburg
 *
 * @copyright
 *   Copyright (c) 2015-2018 Ristretto Developers, Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 *
 * @brief Ristretto255 high-level functions.
 */

#define _XOPEN_SOURCE 600 /* for posix_memalign */

#include <ristretto255.h>
#include "word.h"
#include "field.h"

#define SCALAR_BITS RISTRETTO255_SCALAR_BITS
#define SCALAR_SER_BYTES RISTRETTO255_SCALAR_BYTES
#define SCALAR_LIMBS RISTRETTO255_SCALAR_LIMBS
#define scalar_t ristretto255_scalar_t
#define point_t ristretto255_point_t
#define precomputed_s ristretto255_precomputed_s

/* Comb config: number of combs, n, t, s. */
#define COMBS_N 3
#define COMBS_T 5
#define COMBS_S 17
#define RISTRETTO_WINDOW_BITS 4
#define RISTRETTO_WNAF_FIXED_TABLE_BITS 5
#define RISTRETTO_WNAF_VAR_TABLE_BITS 3

static const int EDWARDS_D = -121665;
static const scalar_t point_scalarmul_adjustment = {{{
    SC_LIMB(0xd6ec31748d98951c), SC_LIMB(0xc6ef5bf4737dcf70), SC_LIMB(0xfffffffffffffffe), SC_LIMB(0x0fffffffffffffff)
}}}, precomputed_scalarmul_adjustment = {{{
    SC_LIMB(0x977f4a4775473484), SC_LIMB(0x6de72ae98b3ab623), SC_LIMB(0xffffffffffffffff), SC_LIMB(0x0fffffffffffffff)
}}};

const gf RISTRETTO255_FACTOR = {FIELD_LITERAL(
    0x702557fa2bf03, 0x514b7d1a82cc6, 0x7f89efd8b43a7, 0x1aef49ec23700, 0x079376fa30500
)};

#define TWISTED_D (-(EDWARDS_D))

#if TWISTED_D < 0
#define EFF_D (-(TWISTED_D))
#define NEG_D 1
#else
#define EFF_D TWISTED_D
#define NEG_D 0
#endif

extern const gf SQRT_MINUS_ONE;

#define WBITS RISTRETTO_WORD_BITS /* NB this may be different from ARCH_WORD_BITS */

extern const point_t ristretto255_point_base;

/* Projective Niels coordinates */
typedef struct { gf a, b, c; } niels_s, niels_t[1];
typedef struct { niels_t n; gf z; } VECTOR_ALIGNED pniels_s, pniels_t[1];

/* Precomputed base */
struct precomputed_s { niels_t table [COMBS_N<<(COMBS_T-1)]; };

extern const gf ristretto255_precomputed_base_as_fe[];
const precomputed_s *ristretto255_precomputed_base =
    (const precomputed_s *) &ristretto255_precomputed_base_as_fe;

const size_t ristretto255_sizeof_precomputed_s = sizeof(precomputed_s);
const size_t ristretto255_alignof_precomputed_s = sizeof(big_register_t);

/** Inverse. */
static void
gf_invert(gf y, const gf x, int assert_nonzero) {
    gf t1, t2;
    gf_sqr(t1, x); // o^2
    mask_t ret = gf_isr(t2, t1); // +-1/sqrt(o^2) = +-1/o
    (void)ret;
    if (assert_nonzero) assert(ret);
    gf_sqr(t1, t2);
    gf_mul(t2, t1, x); // not direct to y in case of alias.
    gf_copy(y, t2);
}

/** identity = (0,1) */
const point_t ristretto255_point_identity = {{{{{0}}},{{{1}}},{{{1}}},{{{0}}}}};

/* Predeclare because not static: called by elligator */
void ristretto255_deisogenize (
    gf_s *__restrict__ s,
    gf_s *__restrict__ inv_el_sum,
    gf_s *__restrict__ inv_el_m1,
    const point_t p,
    mask_t toggle_s,
    mask_t toggle_altx,
    mask_t toggle_rotation
);

void ristretto255_deisogenize (
    gf_s *__restrict__ s,
    gf_s *__restrict__ inv_el_sum,
    gf_s *__restrict__ inv_el_m1,
    const point_t p,
    mask_t toggle_s,
    mask_t toggle_altx,
    mask_t toggle_rotation
) {
    /* More complicated because of rotation */
    gf t1,t2,t3,t4,t5;
    gf_add(t1,p->z,p->y);
    gf_sub(t2,p->z,p->y);
    gf_mul(t3,t1,t2);      /* t3 = num */
    gf_mul(t2,p->x,p->y);  /* t2 = den */
    gf_sqr(t1,t2);
    gf_mul(t4,t1,t3);
    gf_mulw(t1,t4,-1-TWISTED_D);
    gf_isr(t4,t1);         /* isqrt(num*(a-d)*den^2) */
    gf_mul(t1,t2,t4);
    gf_mul(t2,t1,RISTRETTO255_FACTOR); /* t2 = "iden" in ristretto.sage */
    gf_mul(t1,t3,t4);                 /* t1 = "inum" in ristretto.sage */

    /* Calculate altxy = iden*inum*i*t^2*(d-a) */
    gf_mul(t3,t1,t2);
    gf_mul_i(t4,t3);
    gf_mul(t3,t4,p->t);
    gf_mul(t4,t3,p->t);
    gf_mulw(t3,t4,TWISTED_D+1);      /* iden*inum*i*t^2*(d-1) */
    mask_t rotate = toggle_rotation ^ gf_lobit(t3);

    /* Rotate if altxy is negative */
    gf_cond_swap(t1,t2,rotate);
    gf_mul_i(t4,p->x);
    gf_cond_sel(t4,p->y,t4,rotate);  /* t4 = "fac" = ix if rotate, else y */

    gf_mul_i(t5,RISTRETTO255_FACTOR); /* t5 = imi */
    gf_mul(t3,t5,t2);                /* iden * imi */
    gf_mul(t2,t5,t1);
    gf_mul(t5,t2,p->t);              /* "altx" = iden*imi*t */
    mask_t negx = gf_lobit(t5) ^ toggle_altx;

    gf_cond_neg(t1,negx^rotate);
    gf_mul(t2,t1,p->z);
    gf_add(t2,t2,ONE);
    gf_mul(inv_el_sum,t2,t4);
    gf_mul(s,inv_el_sum,t3);

    mask_t negs = gf_lobit(s);
    gf_cond_neg(s,negs);

    mask_t negz = ~negs ^ toggle_s ^ negx;
    gf_copy(inv_el_m1,p->z);
    gf_cond_neg(inv_el_m1,negz);
    gf_sub(inv_el_m1,inv_el_m1,t4);
}

void ristretto255_point_encode( unsigned char ser[SER_BYTES], const point_t p ) {
    gf s,ie1,ie2;
    ristretto255_deisogenize(s,ie1,ie2,p,0,0,0);
    gf_serialize(ser,s,1);
}

ristretto_error_t ristretto255_point_decode (
    point_t p,
    const unsigned char ser[SER_BYTES],
    ristretto_bool_t allow_identity
) {
    gf s, s2, num, tmp;
    gf_s *tmp2=s2, *ynum=p->z, *isr=p->x, *den=p->t;

    mask_t succ = gf_deserialize(s, ser, 1, 0);
    succ &= bool_to_mask(allow_identity) | ~gf_eq(s, ZERO);
    succ &= ~gf_lobit(s);

    gf_sqr(s2,s);                  /* s^2 = -as^2 */
    gf_sub(s2,ZERO,s2);            /* -as^2 */
    gf_sub(den,ONE,s2);            /* 1+as^2 */
    gf_add(ynum,ONE,s2);           /* 1-as^2 */
    gf_mulw(num,s2,-4*TWISTED_D);
    gf_sqr(tmp,den);               /* tmp = den^2 */
    gf_add(num,tmp,num);           /* num = den^2 - 4*d*s^2 */
    gf_mul(tmp2,num,tmp);          /* tmp2 = num*den^2 */
    succ &= gf_isr(isr,tmp2);      /* isr = 1/sqrt(num*den^2) */
    gf_mul(tmp,isr,den);           /* isr*den */
    gf_mul(p->y,tmp,ynum);         /* isr*den*(1-as^2) */
    gf_mul(tmp2,tmp,s);            /* s*isr*den */
    gf_add(tmp2,tmp2,tmp2);        /* 2*s*isr*den */
    gf_mul(tmp,tmp2,isr);          /* 2*s*isr^2*den */
    gf_mul(p->x,tmp,num);          /* 2*s*isr^2*den*num */
    gf_mul(tmp,tmp2,RISTRETTO255_FACTOR); /* 2*s*isr*den*magic */
    gf_cond_neg(p->x,gf_lobit(tmp)); /* flip x */

    /* Additionally check y != 0 and x*y*isomagic nonegative */
    succ &= ~gf_eq(p->y,ZERO);
    gf_mul(tmp,p->x,p->y);
    gf_mul(tmp2,tmp,RISTRETTO255_FACTOR);
    succ &= ~gf_lobit(tmp2);

    gf_copy(tmp,p->x);
    gf_mul_i(p->x,tmp);

    /* Fill in z and t */
    gf_copy(p->z,ONE);
    gf_mul(p->t,p->x,p->y);

    assert(ristretto255_point_valid(p) | ~succ);
    return ristretto_succeed_if(mask_to_bool(succ));
}

void ristretto255_point_sub (
    point_t p,
    const point_t q,
    const point_t r
) {
    gf a, b, c, d;
    gf_sub_nr ( b, q->y, q->x ); /* 3+e */
    gf_sub_nr ( d, r->y, r->x ); /* 3+e */
    gf_add_nr ( c, r->y, r->x ); /* 2+e */
    gf_mul ( a, c, b );
    gf_add_nr ( b, q->y, q->x ); /* 2+e */
    gf_mul ( p->y, d, b );
    gf_mul ( b, r->t, q->t );
    gf_mulw ( p->x, b, 2*EFF_D );
    gf_add_nr ( b, a, p->y );    /* 2+e */
    gf_sub_nr ( c, p->y, a );    /* 3+e */
    gf_mul ( a, q->z, r->z );
    gf_add_nr ( a, a, a );       /* 2+e */
    if (GF_HEADROOM <= 3) gf_weak_reduce(a); /* or 1+e */
#if NEG_D
    gf_sub_nr ( p->y, a, p->x ); /* 4+e or 3+e */
    gf_add_nr ( a, a, p->x );    /* 3+e or 2+e */
#else
    gf_add_nr ( p->y, a, p->x ); /* 3+e or 2+e */
    gf_sub_nr ( a, a, p->x );    /* 4+e or 3+e */
#endif
    gf_mul ( p->z, a, p->y );
    gf_mul ( p->x, p->y, c );
    gf_mul ( p->y, a, b );
    gf_mul ( p->t, b, c );
}

void ristretto255_point_add (
    point_t p,
    const point_t q,
    const point_t r
) {
    gf a, b, c, d;
    gf_sub_nr ( b, q->y, q->x ); /* 3+e */
    gf_sub_nr ( c, r->y, r->x ); /* 3+e */
    gf_add_nr ( d, r->y, r->x ); /* 2+e */
    gf_mul ( a, c, b );
    gf_add_nr ( b, q->y, q->x ); /* 2+e */
    gf_mul ( p->y, d, b );
    gf_mul ( b, r->t, q->t );
    gf_mulw ( p->x, b, 2*EFF_D );
    gf_add_nr ( b, a, p->y );    /* 2+e */
    gf_sub_nr ( c, p->y, a );    /* 3+e */
    gf_mul ( a, q->z, r->z );
    gf_add_nr ( a, a, a );       /* 2+e */
    if (GF_HEADROOM <= 3) gf_weak_reduce(a); /* or 1+e */
#if NEG_D
    gf_add_nr ( p->y, a, p->x ); /* 3+e or 2+e */
    gf_sub_nr ( a, a, p->x );    /* 4+e or 3+e */
#else
    gf_sub_nr ( p->y, a, p->x ); /* 4+e or 3+e */
    gf_add_nr ( a, a, p->x );    /* 3+e or 2+e */
#endif
    gf_mul ( p->z, a, p->y );
    gf_mul ( p->x, p->y, c );
    gf_mul ( p->y, a, b );
    gf_mul ( p->t, b, c );
}

static RISTRETTO_NOINLINE void
point_double_internal (
    point_t p,
    const point_t q,
    int before_double
) {
    gf a, b, c, d;
    gf_sqr ( c, q->x );
    gf_sqr ( a, q->y );
    gf_add_nr ( d, c, a );             /* 2+e */
    gf_add_nr ( p->t, q->y, q->x );    /* 2+e */
    gf_sqr ( b, p->t );
    gf_subx_nr ( b, b, d, 3 );         /* 4+e */
    gf_sub_nr ( p->t, a, c );          /* 3+e */
    gf_sqr ( p->x, q->z );
    gf_add_nr ( p->z, p->x, p->x );    /* 2+e */
    gf_subx_nr ( a, p->z, p->t, 4 );   /* 6+e */
    if (GF_HEADROOM == 5) gf_weak_reduce(a); /* or 1+e */
    gf_mul ( p->x, a, b );
    gf_mul ( p->z, p->t, a );
    gf_mul ( p->y, p->t, d );
    if (!before_double) gf_mul ( p->t, b, d );
}

void ristretto255_point_double(point_t p, const point_t q) {
    point_double_internal(p,q,0);
}

void ristretto255_point_negate (
   point_t nega,
   const point_t a
) {
    gf_sub(nega->x, ZERO, a->x);
    gf_copy(nega->y, a->y);
    gf_copy(nega->z, a->z);
    gf_sub(nega->t, ZERO, a->t);
}

/* Operations on [p]niels */
static RISTRETTO_INLINE void
cond_neg_niels (
    niels_t n,
    mask_t neg
) {
    gf_cond_swap(n->a, n->b, neg);
    gf_cond_neg(n->c, neg);
}

static RISTRETTO_NOINLINE void pt_to_pniels (
    pniels_t b,
    const point_t a
) {
    gf_sub ( b->n->a, a->y, a->x );
    gf_add ( b->n->b, a->x, a->y );
    gf_mulw ( b->n->c, a->t, 2*TWISTED_D );
    gf_add ( b->z, a->z, a->z );
}

static RISTRETTO_NOINLINE void pniels_to_pt (
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

static RISTRETTO_NOINLINE void
niels_to_pt (
    point_t e,
    const niels_t n
) {
    gf_add ( e->y, n->b, n->a );
    gf_sub ( e->x, n->b, n->a );
    gf_mul ( e->t, e->y, e->x );
    gf_copy ( e->z, ONE );
}

static RISTRETTO_NOINLINE void
add_niels_to_pt (
    point_t d,
    const niels_t e,
    int before_double
) {
    gf a, b, c;
    gf_sub_nr ( b, d->y, d->x ); /* 3+e */
    gf_mul ( a, e->a, b );
    gf_add_nr ( b, d->x, d->y ); /* 2+e */
    gf_mul ( d->y, e->b, b );
    gf_mul ( d->x, e->c, d->t );
    gf_add_nr ( c, a, d->y );    /* 2+e */
    gf_sub_nr ( b, d->y, a );    /* 3+e */
    gf_sub_nr ( d->y, d->z, d->x ); /* 3+e */
    gf_add_nr ( a, d->x, d->z ); /* 2+e */
    gf_mul ( d->z, a, d->y );
    gf_mul ( d->x, d->y, b );
    gf_mul ( d->y, a, c );
    if (!before_double) gf_mul ( d->t, b, c );
}

static RISTRETTO_NOINLINE void
sub_niels_from_pt (
    point_t d,
    const niels_t e,
    int before_double
) {
    gf a, b, c;
    gf_sub_nr ( b, d->y, d->x ); /* 3+e */
    gf_mul ( a, e->b, b );
    gf_add_nr ( b, d->x, d->y ); /* 2+e */
    gf_mul ( d->y, e->a, b );
    gf_mul ( d->x, e->c, d->t );
    gf_add_nr ( c, a, d->y );    /* 2+e */
    gf_sub_nr ( b, d->y, a );    /* 3+e */
    gf_add_nr ( d->y, d->z, d->x ); /* 2+e */
    gf_sub_nr ( a, d->z, d->x ); /* 3+e */
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

static RISTRETTO_NOINLINE void
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
    ristretto255_point_copy(tmp, b);
    for (i=1; i<ntable; i++) {
        add_pniels_to_pt(tmp, pn, 0);
        pt_to_pniels(multiples[i], tmp);
    }

    ristretto_bzero(pn,sizeof(pn));
    ristretto_bzero(tmp,sizeof(tmp));
}

void ristretto255_point_scalarmul (
    point_t a,
    const point_t b,
    const scalar_t scalar
) {

    const int WINDOW = RISTRETTO_WINDOW_BITS,
        WINDOW_MASK = (1<<WINDOW)-1,
        WINDOW_T_MASK = WINDOW_MASK >> 1,
        NTABLE = 1<<(WINDOW-1);

    scalar_t scalar1x;
    ristretto255_scalar_add(scalar1x, scalar, point_scalarmul_adjustment);
    ristretto255_scalar_halve(scalar1x,scalar1x);

    /* Set up a precomputed table with odd multiples of b. */
    pniels_t pn, multiples[1<<((int)(RISTRETTO_WINDOW_BITS)-1)];  // == NTABLE (MSVC compatibility issue)
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
        constant_time_lookup(pn, multiples, sizeof(pn), NTABLE, bits & WINDOW_T_MASK);
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
    ristretto255_point_copy(a,tmp);

    ristretto_bzero(scalar1x,sizeof(scalar1x));
    ristretto_bzero(pn,sizeof(pn));
    ristretto_bzero(multiples,sizeof(multiples));
    ristretto_bzero(tmp,sizeof(tmp));
}

void ristretto255_point_double_scalarmul (
    point_t a,
    const point_t b,
    const scalar_t scalarb,
    const point_t c,
    const scalar_t scalarc
) {

    const int WINDOW = RISTRETTO_WINDOW_BITS,
        WINDOW_MASK = (1<<WINDOW)-1,
        WINDOW_T_MASK = WINDOW_MASK >> 1,
        NTABLE = 1<<(WINDOW-1);

    scalar_t scalar1x, scalar2x;
    ristretto255_scalar_add(scalar1x, scalarb, point_scalarmul_adjustment);
    ristretto255_scalar_halve(scalar1x,scalar1x);
    ristretto255_scalar_add(scalar2x, scalarc, point_scalarmul_adjustment);
    ristretto255_scalar_halve(scalar2x,scalar2x);

    /* Set up a precomputed table with odd multiples of b. */
    pniels_t pn, multiples1[1<<((int)(RISTRETTO_WINDOW_BITS)-1)], multiples2[1<<((int)(RISTRETTO_WINDOW_BITS)-1)];
    // Array size above equal NTABLE (MSVC compatibility issue)
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
        constant_time_lookup(pn, multiples1, sizeof(pn), NTABLE, bits1 & WINDOW_T_MASK);
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
        constant_time_lookup(pn, multiples2, sizeof(pn), NTABLE, bits2 & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv2);
        add_pniels_to_pt(tmp, pn, i?-1:0);
    }

    /* Write out the answer */
    ristretto255_point_copy(a,tmp);


    ristretto_bzero(scalar1x,sizeof(scalar1x));
    ristretto_bzero(scalar2x,sizeof(scalar2x));
    ristretto_bzero(pn,sizeof(pn));
    ristretto_bzero(multiples1,sizeof(multiples1));
    ristretto_bzero(multiples2,sizeof(multiples2));
    ristretto_bzero(tmp,sizeof(tmp));
}

void ristretto255_point_dual_scalarmul (
    point_t a1,
    point_t a2,
    const point_t b,
    const scalar_t scalar1,
    const scalar_t scalar2
) {

    const int WINDOW = RISTRETTO_WINDOW_BITS,
        WINDOW_MASK = (1<<WINDOW)-1,
        WINDOW_T_MASK = WINDOW_MASK >> 1,
        NTABLE = 1<<(WINDOW-1);


    scalar_t scalar1x, scalar2x;
    ristretto255_scalar_add(scalar1x, scalar1, point_scalarmul_adjustment);
    ristretto255_scalar_halve(scalar1x,scalar1x);
    ristretto255_scalar_add(scalar2x, scalar2, point_scalarmul_adjustment);
    ristretto255_scalar_halve(scalar2x,scalar2x);

    /* Set up a precomputed table with odd multiples of b. */
    point_t multiples1[1<<((int)(RISTRETTO_WINDOW_BITS)-1)], multiples2[1<<((int)(RISTRETTO_WINDOW_BITS)-1)], working, tmp;
    // Array sizes above equal NTABLE (MSVC compatibility issue)

    pniels_t pn;

    ristretto255_point_copy(working, b);

    /* Initialize. */
    int i,j;

    for (i=0; i<NTABLE; i++) {
        ristretto255_point_copy(multiples1[i], ristretto255_point_identity);
        ristretto255_point_copy(multiples2[i], ristretto255_point_identity);
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

        constant_time_lookup(tmp, multiples1, sizeof(tmp), NTABLE, bits1 & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv1);
        /* add_pniels_to_pt(multiples1[bits1 & WINDOW_T_MASK], pn, 0); */
        add_pniels_to_pt(tmp, pn, 0);
        constant_time_insert(multiples1, tmp, sizeof(tmp), NTABLE, bits1 & WINDOW_T_MASK);


        constant_time_lookup(tmp, multiples2, sizeof(tmp), NTABLE, bits2 & WINDOW_T_MASK);
        cond_neg_niels(pn->n, inv1^inv2);
        /* add_pniels_to_pt(multiples2[bits2 & WINDOW_T_MASK], pn, 0); */
        add_pniels_to_pt(tmp, pn, 0);
        constant_time_insert(multiples2, tmp, sizeof(tmp), NTABLE, bits2 & WINDOW_T_MASK);
    }

    if (NTABLE > 1) {
        ristretto255_point_copy(working, multiples1[NTABLE-1]);
        ristretto255_point_copy(tmp    , multiples2[NTABLE-1]);

        for (i=NTABLE-1; i>1; i--) {
            ristretto255_point_add(multiples1[i-1], multiples1[i-1], multiples1[i]);
            ristretto255_point_add(multiples2[i-1], multiples2[i-1], multiples2[i]);
            ristretto255_point_add(working, working, multiples1[i-1]);
            ristretto255_point_add(tmp,     tmp,     multiples2[i-1]);
        }

        ristretto255_point_add(multiples1[0], multiples1[0], multiples1[1]);
        ristretto255_point_add(multiples2[0], multiples2[0], multiples2[1]);
        point_double_internal(working, working, 0);
        point_double_internal(tmp,         tmp, 0);
        ristretto255_point_add(a1, working, multiples1[0]);
        ristretto255_point_add(a2, tmp,     multiples2[0]);
    } else {
        ristretto255_point_copy(a1, multiples1[0]);
        ristretto255_point_copy(a2, multiples2[0]);
    }

    ristretto_bzero(scalar1x,sizeof(scalar1x));
    ristretto_bzero(scalar2x,sizeof(scalar2x));
    ristretto_bzero(pn,sizeof(pn));
    ristretto_bzero(multiples1,sizeof(multiples1));
    ristretto_bzero(multiples2,sizeof(multiples2));
    ristretto_bzero(tmp,sizeof(tmp));
    ristretto_bzero(working,sizeof(working));
}

ristretto_bool_t ristretto255_point_eq ( const point_t p, const point_t q ) {
    /* equality mod 2-torsion compares x/y */
    gf a, b;
    gf_mul ( a, p->y, q->x );
    gf_mul ( b, q->y, p->x );
    mask_t succ = gf_eq(a,b);

    gf_mul ( a, p->y, q->y );
    gf_mul ( b, q->x, p->x );

    /* Interesting note: the 4tor would normally be rotation.
     * But because of the *i twist, it's actually
     * (x,y) <-> (iy,ix)
     */

    succ |= gf_eq(a,b);

    return mask_to_bool(succ);
}

ristretto_bool_t ristretto255_point_valid (
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
    gf_mulw(c,b,TWISTED_D);
    gf_sqr(b,p->z);
    gf_add(b,b,c);
    out &= gf_eq(a,b);
    out &= ~gf_eq(p->z,ZERO);
    return mask_to_bool(out);
}

void ristretto255_point_debugging_torque (
    point_t q,
    const point_t p
) {
    gf tmp;
    gf_mul(tmp,p->x,SQRT_MINUS_ONE);
    gf_mul(q->x,p->y,SQRT_MINUS_ONE);
    gf_copy(q->y,tmp);
    gf_copy(q->z,p->z);
    gf_sub(q->t,ZERO,p->t);
}

void ristretto255_point_debugging_pscale (
    point_t q,
    const point_t p,
    const uint8_t factor[SER_BYTES]
) {
    gf gfac,tmp;
    ignore_result(gf_deserialize(gfac,factor,0,0));
    gf_cond_sel(gfac,gfac,ONE,gf_eq(gfac,ZERO));
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

    gf_invert(out[0], out[0], 1);

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

    ristretto_bzero(product,sizeof(product));
}

void ristretto255_precompute (
    precomputed_s *table,
    const point_t base
) {
    const unsigned int n = COMBS_N, t = COMBS_T, s = COMBS_S;
    assert(n*t*s >= SCALAR_BITS);

    point_t working, start, doubles[COMBS_T-1];
    ristretto255_point_copy(working, base);
    pniels_t pn_tmp;

    gf zs[(unsigned int)(COMBS_N)<<(unsigned int)(COMBS_T-1)], zis[(unsigned int)(COMBS_N)<<(unsigned int)(COMBS_T-1)];

    unsigned int i,j,k;

    /* Compute n tables */
    for (i=0; i<n; i++) {

        /* Doubling phase */
        for (j=0; j<t; j++) {
            if (j) ristretto255_point_add(start, start, working);
            else ristretto255_point_copy(start, working);

            if (j==t-1 && i==n-1) break;

            point_double_internal(working, working,0);
            if (j<t-1) ristretto255_point_copy(doubles[j], working);

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
                ristretto255_point_add(start, start, doubles[k]);
            } else {
                ristretto255_point_sub(start, start, doubles[k]);
            }
        }
    }

    batch_normalize_niels(table->table,(const gf *)zs,zis,n<<(t-1));

    ristretto_bzero(zs,sizeof(zs));
    ristretto_bzero(zis,sizeof(zis));
    ristretto_bzero(pn_tmp,sizeof(pn_tmp));
    ristretto_bzero(working,sizeof(working));
    ristretto_bzero(start,sizeof(start));
    ristretto_bzero(doubles,sizeof(doubles));
}

static RISTRETTO_INLINE void
constant_time_lookup_niels (
    niels_s *__restrict__ ni,
    const niels_t *table,
    int nelts,
    int idx
) {
    constant_time_lookup(ni, table, sizeof(niels_s), nelts, idx);
}

void ristretto255_precomputed_scalarmul (
    point_t out,
    const precomputed_s *table,
    const scalar_t scalar
) {
    int i;
    unsigned j,k;
    const unsigned int n = COMBS_N, t = COMBS_T, s = COMBS_S;

    scalar_t scalar1x;
    ristretto255_scalar_add(scalar1x, scalar, precomputed_scalarmul_adjustment);
    ristretto255_scalar_halve(scalar1x,scalar1x);

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

            constant_time_lookup_niels(ni, &table->table[j<<(t-1)], 1<<(t-1), tab);

            cond_neg_niels(ni, invert);
            if ((i!=(int)s-1)||j) {
                add_niels_to_pt(out, ni, j==n-1 && i);
            } else {
                niels_to_pt(out, ni);
            }
        }
    }

    ristretto_bzero(ni,sizeof(ni));
    ristretto_bzero(scalar1x,sizeof(scalar1x));
}

void ristretto255_point_cond_sel (
    point_t out,
    const point_t a,
    const point_t b,
    ristretto_bool_t pick_b
) {
    constant_time_select(out,a,b,sizeof(point_t),bool_to_mask(pick_b),0);
}

/* FUTURE: restore Curve25519 Montgomery ladder? */
ristretto_error_t ristretto255_direct_scalarmul (
    uint8_t scaled[SER_BYTES],
    const uint8_t base[SER_BYTES],
    const scalar_t scalar,
    ristretto_bool_t allow_identity,
    ristretto_bool_t short_circuit
) {
    point_t basep;
    ristretto_error_t succ = ristretto255_point_decode(basep, base, allow_identity);
    if (short_circuit && succ != RISTRETTO_SUCCESS) return succ;
    ristretto255_point_cond_sel(basep, ristretto255_point_base, basep, succ);
    ristretto255_point_scalarmul(basep, basep, scalar);
    ristretto255_point_encode(scaled, basep);
    ristretto255_point_destroy(basep);
    return succ;
}

/**
 * @cond internal
 * Control for variable-time scalar multiply algorithms.
 */
struct smvt_control {
  int power, addend;
};

static int recode_wnaf (
    struct smvt_control *control, /* [nbits/(table_bits+1) + 3] */
    const scalar_t scalar,
    unsigned int table_bits
) {
    unsigned int table_size = SCALAR_BITS/(table_bits+1) + 3;
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
    uint32_t mask = (1<<(table_bits+1))-1;

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
            if (odd & 1<<(table_bits+1)) delta -= (1<<(table_bits+1));
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

/* MSVC has no builtint ctz, this is a fix as in
https://stackoverflow.com/questions/355967/how-to-use-msvc-intrinsics-to-get-the-equivalent-of-this-gcc-code/5468852#5468852
*/
#ifdef _MSC_VER
#include <intrin.h>

uint32_t __inline ctz(uint32_t value)
{
    DWORD trailing_zero = 0;
    if ( _BitScanForward( &trailing_zero, value ) )
        return trailing_zero;
    else
        return 32;  // This is undefined, I better choose 32 than 0
}
#define __builtin_ctz(x) ctz(x)
#endif

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

    ristretto255_point_double(tmp,working);
    pniels_t twop;
    pt_to_pniels(twop, tmp);

    add_pniels_to_pt(tmp, output[0],0);
    pt_to_pniels(output[1], tmp);

    for (i=2; i < 1<<tbits; i++) {
        add_pniels_to_pt(tmp, twop,0);
        pt_to_pniels(output[i], tmp);
    }

    ristretto255_point_destroy(tmp);
    ristretto_bzero(twop,sizeof(twop));
}

extern const gf ristretto255_precomputed_wnaf_as_fe[];
static const niels_t *ristretto255_wnaf_base = (const niels_t *)ristretto255_precomputed_wnaf_as_fe;
const size_t ristretto255_sizeof_precomputed_wnafs
    = sizeof(niels_t)<<RISTRETTO_WNAF_FIXED_TABLE_BITS;

void ristretto255_precompute_wnafs (
    niels_t out[1<<RISTRETTO_WNAF_FIXED_TABLE_BITS],
    const point_t base
);

void ristretto255_precompute_wnafs (
    niels_t out[1<<RISTRETTO_WNAF_FIXED_TABLE_BITS],
    const point_t base
) {
    pniels_t tmp[1<<RISTRETTO_WNAF_FIXED_TABLE_BITS];
    gf zs[1<<RISTRETTO_WNAF_FIXED_TABLE_BITS], zis[1<<RISTRETTO_WNAF_FIXED_TABLE_BITS];
    int i;
    prepare_wnaf_table(tmp,base,RISTRETTO_WNAF_FIXED_TABLE_BITS);
    for (i=0; i<1<<RISTRETTO_WNAF_FIXED_TABLE_BITS; i++) {
        memcpy(out[i], tmp[i]->n, sizeof(niels_t));
        gf_copy(zs[i], tmp[i]->z);
    }
    batch_normalize_niels(out, (const gf *)zs, zis, 1<<RISTRETTO_WNAF_FIXED_TABLE_BITS);

    ristretto_bzero(tmp,sizeof(tmp));
    ristretto_bzero(zs,sizeof(zs));
    ristretto_bzero(zis,sizeof(zis));
}

void ristretto255_base_double_scalarmul_non_secret (
    point_t combo,
    const scalar_t scalar1,
    const point_t base2,
    const scalar_t scalar2
) {
    const int table_bits_var = RISTRETTO_WNAF_VAR_TABLE_BITS,
        table_bits_pre = RISTRETTO_WNAF_FIXED_TABLE_BITS;
    struct smvt_control control_var[SCALAR_BITS/((int)(RISTRETTO_WNAF_VAR_TABLE_BITS)+1)+3];
    struct smvt_control control_pre[SCALAR_BITS/((int)(RISTRETTO_WNAF_FIXED_TABLE_BITS)+1)+3];

    int ncb_pre = recode_wnaf(control_pre, scalar1, table_bits_pre);
    int ncb_var = recode_wnaf(control_var, scalar2, table_bits_var);

    pniels_t precmp_var[1<<(int)(RISTRETTO_WNAF_VAR_TABLE_BITS)];
    prepare_wnaf_table(precmp_var, base2, table_bits_var);

    int contp=0, contv=0, i = control_var[0].power;

    if (i < 0) {
        ristretto255_point_copy(combo, ristretto255_point_identity);
        return;
    } else if (i > control_pre[0].power) {
        pniels_to_pt(combo, precmp_var[control_var[0].addend >> 1]);
        contv++;
    } else if (i == control_pre[0].power && i >=0 ) {
        pniels_to_pt(combo, precmp_var[control_var[0].addend >> 1]);
        add_niels_to_pt(combo, ristretto255_wnaf_base[control_pre[0].addend >> 1], i);
        contv++; contp++;
    } else {
        i = control_pre[0].power;
        niels_to_pt(combo, ristretto255_wnaf_base[control_pre[0].addend >> 1]);
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
                add_niels_to_pt(combo, ristretto255_wnaf_base[control_pre[contp].addend >> 1], i);
            } else {
                sub_niels_from_pt(combo, ristretto255_wnaf_base[(-control_pre[contp].addend) >> 1], i);
            }
            contp++;
        }
    }

    /* This function is non-secret, but whatever this is cheap. */
    ristretto_bzero(control_var,sizeof(control_var));
    ristretto_bzero(control_pre,sizeof(control_pre));
    ristretto_bzero(precmp_var,sizeof(precmp_var));

    assert(contv == ncb_var); (void)ncb_var;
    assert(contp == ncb_pre); (void)ncb_pre;
}

void ristretto255_point_destroy (
    point_t point
) {
    ristretto_bzero(point, sizeof(point_t));
}

void ristretto255_precomputed_destroy (
    precomputed_s *pre
) {
    ristretto_bzero(pre, ristretto255_sizeof_precomputed_s);
}
