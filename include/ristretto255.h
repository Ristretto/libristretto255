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
extern "C" {
#endif

/**
 * @brief Encode a Ristretto255 point as a 32-byte array, i.e. serialize the
 * point using the Ristretto255 wire format.
 *
 * @param [out] enc The encoded point.
 * @param [in] p The point.
 */
void ristretto255_point_encode (unsigned char enc[RISTRETTO255_SER_BYTES], const ristretto255_point_t p);

/**
 * @brief Decode a Ristretto255 point from a 32-byte array, i.e. deserialize a
 * Ristretto255 point from the wire format.
 *
 * @param [out] enc The encoded point.
 * @param [in] p The point.
 * @param [in] allow_identity Is the identity point allowed?
 */
ristretto_error_t ristretto255_point_decode (
    ristretto255_point_t p,
    const unsigned char enc[RISTRETTO255_SER_BYTES],
    ristretto_bool_t allow_identity
);

/**
 * @brief Subtract one Ristretto255 point from another, i.e. add the inverse
 * of point r to point q, storing the result as point p.
 *
 * @param [out] p The resulting point.
 * @param [in] q The point to subtract from.
 * @param [in] r The point to be subtracted (i.e. add the inverse of this point).
 */
void ristretto255_point_sub (
    ristretto255_point_t p,
    const ristretto255_point_t q,
    const ristretto255_point_t r
);

/**
 * @brief Add two Ristretto255 points q and r, storing the result in p.
 *
 * @param [out] p The resulting point.
 * @param [in] q Point to add.
 * @param [in] r Other point to add.
 */
void ristretto255_point_add (
    ristretto255_point_t p,
    const ristretto255_point_t q,
    const ristretto255_point_t r
);

/**
 * @brief Double a Ristretto255 point q, storing the result in p.
 *
 * @param [out] p The resulting point.
 * @param [in] q Point to be doubled.
 */
void ristretto255_point_double (ristretto255_point_t p, const ristretto255_point_t q);

/**
 * @brief Negate a Ristretto255 point a, storing the result in nega.
 *
 * @param [out] nega The resulting point.
 * @param [in] a Point to be negated.
 */
void ristretto255_point_negate (
   ristretto255_point_t nega,
   const ristretto255_point_t a
);

/**
 * @brief Multiply a Ristretto255 point b by the given scalar value, storing
 * the result as Ristretto255 point a.
 *
 * @param [out] a The resulting point.
 * @param [in] b The point to be multiplied.
 * @param [in] scalar Scalar to multiply by point b.
 */
void ristretto255_point_scalarmul (
    ristretto255_point_t a,
    const ristretto255_point_t b,
    const ristretto255_scalar_t scalar
);

/**
 * @brief TODO
 */
void ristretto255_point_double_scalarmul (
    ristretto255_point_t a,
    const ristretto255_point_t b,
    const ristretto255_scalar_t scalarb,
    const ristretto255_point_t c,
    const ristretto255_scalar_t scalarc
);

/**
 * @brief TODO
 */
void ristretto255_point_dual_scalarmul (
    ristretto255_point_t a1,
    ristretto255_point_t a2,
    const ristretto255_point_t b,
    const ristretto255_scalar_t scalar1,
    const ristretto255_scalar_t scalar2
);

/**
 * @brief Are the two Ristretto255 points equal?
 *
 * @param [in] p Point to compare.
 * @param [in] q Return true if this point is equal to p.
 */
ristretto_bool_t ristretto255_point_eq (const ristretto255_point_t p, const ristretto255_point_t q);

/**
 * @brief Is the given Ristretto255 point valid?
 *
 * @param [in] p Point to test for validity.
 */
ristretto_bool_t ristretto255_point_valid (const ristretto255_point_t p);

/**
 * @brief Compute a Ristretto255 point from the given hashed data (i.e. output
 * of a digest function such as SHA-256) using Elligator 2 on the Jacobi
 * quartic then translating this point to the Edwards curve using the isogeny.
 *
 * This version of the transform takes an input the same size as an encoded
 * Ristretto255 point (i.e. 32-bytes), but does not uphold the Elligator
 * property of producing encoded points which are indistinguishable from uniform
 * random strings. See the "ristretto255_point_from_hash_uniform" if your
 * usage requires that.
 *
 * Unlike Elligator 2 as specified for the "edwards25519" curve, the Ristretto
 * method for computing the Elligator map is constant time.
 *
 * @param [out] p Point computed using the Elligator 2 hash-to-curve method.
 * @param [in] hashed_data Digest output from which a point is computed.
 */
void ristretto255_point_from_hash_nonuniform (
    ristretto255_point_t p,
    const unsigned char hashed_data[RISTRETTO255_SER_BYTES]
);

/**
 * @brief Compute a Ristretto255 point from the given hashed data (i.e. output
 * of a digest function such as SHA-512) using Elligator 2 on the Jacobi
 * quartic then translating this point to the Edwards curve using the isogeny.
 *
 * This version of the transform upholds the Elligator property of producing
 * encoded points which are indistinguishable from uniform random strings.
 *
 * Unlike Elligator 2 as specified for the "edwards25519" curve, the Ristretto
 * method for computing the Elligator map is constant time.
 *
 * @param [out] p Point computed using the Elligator 2 hash-to-curve method.
 * @param [in] hashed_data Digest output from which a point is computed.
 */
void ristretto255_point_from_hash_uniform (
    ristretto255_point_t pt,
    const unsigned char hashed_data[2*RISTRETTO255_SER_BYTES]
);

/**
 * @brief Invert the Ristretto255 Elligator 2 map, recovering the input data
 * which was used to compute a given Ristretto255 point.
 *
 * This version recovers Ristretto255 points computed using the non-uniform
 * Elligator 2 method.
 *
 * @param [out] recovered_hash Hash value recovered from Ristretto255 point p.
 * @param [in] p Ristretto255 point computed using Elligator2 to invert.
 * @param [in] hint Hint value (???).
 */
ristretto_error_t ristretto255_invert_elligator_nonuniform (
    unsigned char recovered_hash[RISTRETTO255_SER_BYTES],
    const ristretto255_point_t p,
    uint32_t hint
);

/**
 * @brief Invert the Ristretto255 Elligator 2 map, recovering the input data
 * which was used to compute a given Ristretto255 point.
 *
 * This version recovers Ristretto255 points computed using the uniformly
 * random Elligator 2 method.
 *
 * @param [out] recovered_hash Hash value recovered from Ristretto255 point p.
 * @param [in] p Ristretto255 point computed using Elligator2 to invert.
 * @param [in] hint Hint value (???).
 */

ristretto_error_t ristretto255_invert_elligator_uniform (
    unsigned char partial_hash[2*RISTRETTO255_SER_BYTES],
    const ristretto255_point_t p,
    uint32_t hint
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __RISTRETTO255_H__ */
