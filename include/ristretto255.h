/**
 * Master header for libristretto255.
 *
 * This library implements cryptographic operations on elliptic curve groups
 * of prime order p.  It accomplishes this by using a twisted Edwards curve
 * (i.e. "edwards25519", the Edwards form of Curve25519) and wiping out the
 * cofactor.
 *
 * The formulas are all complete and have no special cases.  However, some
 * functions can fail.  For example, decoding functions can fail because not
 * every string is the encoding of a valid group element.
 *
 * The formulas contain no data-dependent branches, timing or memory accesses,
 * except for ristretto255_base_double_scalarmul_non_secret.
 */

#ifndef __RISTRETTO255_H__
#define __RISTRETTO255_H__ 1

#include <ristretto255/common.h>
#include <ristretto255/point.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __RISTRETTO255_H__ */
