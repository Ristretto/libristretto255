/** @brief A group of prime order p, based on $(iso_to). */

#include <decaf/point_$(gf_bits).h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of bytes in an EdDSA public key. */
#define DECAF_EDDSA_$(gf_shortname)_PUBLIC_BYTES $((gf_bits)//8 + 1)

/** Number of bytes in an EdDSA private key. */
#define DECAF_EDDSA_$(gf_shortname)_PRIVATE_BYTES DECAF_EDDSA_$(gf_shortname)_PUBLIC_BYTES

/** Number of bytes in an EdDSA private key. */
#define DECAF_EDDSA_$(gf_shortname)_SIGNATURE_BYTES (DECAF_EDDSA_$(gf_shortname)_PUBLIC_BYTES + DECAF_EDDSA_$(gf_shortname)_PRIVATE_BYTES)

#ifdef __cplusplus
} /* extern "C" */
#endif
