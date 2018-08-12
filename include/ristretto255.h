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

/** Number of bytes in a Ristretto255 public key. */
#define RISTRETTO255_PUBLIC_BYTES 32

/** Number of bytes in an Ristretto255 private key. */
#define RISTRETTO255_PRIVATE_BYTES RISTRETTO255_PUBLIC_BYTES

/** Number of bytes in an Ristretto255 private key. */
#define RISTRETTO255_SIGNATURE_BYTES (RISTRETTO255_PUBLIC_BYTES + RISTRETTO255_PRIVATE_BYTES)

/** Ratio for EdDSA encoding a Ristretto255 point. */
#define RISTRETTO255_ENCODE_RATIO 4

 /** Ratio for EdDSA decoding a Ristretto255 point. */
#define RISTRETTO255_DECODE_RATIO (8 / RISTRETTO255_ENCODE_RATIO)

/**
 * @brief EdDSA point encoding.  Used internally, exposed externally.
 * Multiplies by RISTRETTO255_ENCODE_RATIO first.
 *
 * The multiplication is required because the EdDSA encoding represents
 * the cofactor information, but the Ristretto encoding ignores it (which
 * is the whole point).  So if you decode from EdDSA and re-encode to
 * EdDSA, the cofactor info must get cleared, because the intermediate
 * representation doesn't track it.
 *
 * The way libristretto255 handles this is to multiply by
 * RISTRETTO255_DECODE_RATIO when decoding, and by
 * RISTRETTO255_ENCODE_RATIO when encoding.  The product of these
 * ratios is always exactly the cofactor (8), so the cofactor
 * ends up cleared one way or another.  But exactly how that shakes
 * out depends on the base points specified in RFC 8032.
 *
 * The upshot is that if you pass the Ristretto255 base point to this function,
 * you will get RISTRETTO255_ENCODE_RATIO times the EdDSA base point.
 *
 * @param [out] enc The encoded point.
 * @param [in] p The point.
 */
void RISTRETTO_API_VIS ristretto255_point_mul_by_ratio_and_encode_like_eddsa (
    uint8_t enc[RISTRETTO255_PUBLIC_BYTES],
    const ristretto255_point_t p
) RISTRETTO_NONNULL RISTRETTO_NOINLINE;
 /**
 * @brief EdDSA point decoding.  Multiplies by RISTRETTO255_DECODE_RATIO,
 * and ignores cofactor information.
 *
 * See notes on ristretto255_point_mul_by_ratio_and_encode_like_eddsa
 *
 * @param [out] enc The encoded point.
 * @param [in] p The point.
 */
ristretto_error_t RISTRETTO_API_VIS ristretto255_point_decode_like_eddsa_and_mul_by_ratio (
    ristretto255_point_t p,
    const uint8_t enc[RISTRETTO255_PUBLIC_BYTES]
) RISTRETTO_NONNULL RISTRETTO_NOINLINE;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __RISTRETTO255_H__ */
