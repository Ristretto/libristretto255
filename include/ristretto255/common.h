/**
 * @file ristretto255/common.h
 * @author Mike Hamburg
 *
 * @copyright
 *   Copyright (c) 2015-2018 Ristretto Developers, Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 *
 * @brief Common utility headers for the Ristretto255 library.
 */

#ifndef __RISTRETTO255_COMMON_H__
#define __RISTRETTO255_COMMON_H__ 1

#include <stdint.h>
#if defined (__GNUC__)  // File only exists for GNU compilers
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Aliasing MSVC preprocessing to GNU preprocessing */
#if defined _MSC_VER
#   define __attribute__(x)        // Turn off attribute code
#   define __attribute(x)
#   define __restrict__ __restrict  // Use MSVC restrict code
//#   define RISTRETTO_NOINLINE __declspec(noinline) // MSVC for noinline
//#   define RISTRETTO_INLINE __forceinline // MSVC for always inline
//#   define RISTRETTO_WARN_UNUSED _Check_return_
#else // MSVC
#define RISTRETTO_API_IMPORT
#endif

// The following are disabled for MSVC
#define RISTRETTO_NOINLINE  __attribute__((noinline))
#define RISTRETTO_INLINE inline __attribute__((always_inline,unused))
#define RISTRETTO_WARN_UNUSED __attribute__((warn_unused_result))
#define RISTRETTO_NONNULL __attribute__((nonnull))
// Cribbed from libnotmuch
#if defined (__clang_major__) && __clang_major__ >= 3 \
    || defined (__GNUC__) && __GNUC__ >= 5 \
    || defined (__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ >= 5
#define RISTRETTO_DEPRECATED(msg) __attribute__ ((deprecated(msg)))
#else
#define RISTRETTO_DEPRECATED(msg) __attribute__ ((deprecated))
#endif
/** @endcond */

/* Internal word types.
 *
 * Somewhat tricky.  This could be decided separately per platform.  However,
 * the structs do need to be all the same size and alignment on a given
 * platform to support dynamic linking, since even if you header was built
 * with eg arch_neon, you might end up linking a library built with arch_arm32.
 */
#ifndef RISTRETTO_WORD_BITS
    #if (defined(__ILP64__) || defined(__amd64__) || defined(__x86_64__) || (((__UINT_FAST32_MAX__)>>30)>>30))
        #define RISTRETTO_WORD_BITS 64 /**< The number of bits in a word */
    #else
        #define RISTRETTO_WORD_BITS 32 /**< The number of bits in a word */
    #endif
#endif

#if RISTRETTO_WORD_BITS == 64
typedef uint64_t ristretto_word_t;      /**< Word size for internal computations */
typedef int64_t ristretto_sword_t;      /**< Signed word size for internal computations */
typedef uint64_t ristretto_bool_t;      /**< "Boolean" type, will be set to all-zero or all-one (i.e. -1u) */
typedef __uint128_t ristretto_dword_t;  /**< Double-word size for internal computations */
typedef __int128_t ristretto_dsword_t;  /**< Signed double-word size for internal computations */
#elif RISTRETTO_WORD_BITS == 32         /**< The number of bits in a word */
typedef uint32_t ristretto_word_t;      /**< Word size for internal computations */
typedef int32_t ristretto_sword_t;      /**< Signed word size for internal computations */
typedef uint32_t ristretto_bool_t;      /**< "Boolean" type, will be set to all-zero or all-one (i.e. -1u) */
typedef uint64_t ristretto_dword_t;     /**< Double-word size for internal computations */
typedef int64_t ristretto_dsword_t;     /**< Signed double-word size for internal computations */
#else
#error "Only supporting RISTRETTO_WORD_BITS = 32 or 64 for now"
#endif

/** RISTRETTO_TRUE = -1 so that RISTRETTO_TRUE & x = x */
static const ristretto_bool_t RISTRETTO_TRUE = -(ristretto_bool_t)1;

/** RISTRETTO_FALSE = 0 so that RISTRETTO_FALSE & x = 0 */
static const ristretto_bool_t RISTRETTO_FALSE = 0;

/** Another boolean type used to indicate success or failure. */
typedef enum {
    RISTRETTO_SUCCESS = -1, /**< The operation succeeded. */
    RISTRETTO_FAILURE = 0   /**< The operation failed. */
} ristretto_error_t;


/** Return success if x is true */
static RISTRETTO_INLINE ristretto_error_t
ristretto_succeed_if(ristretto_bool_t x) {
    return (ristretto_error_t)x;
}

/** Return RISTRETTO_TRUE iff x == RISTRETTO_SUCCESS */
static RISTRETTO_INLINE ristretto_bool_t
ristretto_successful(ristretto_error_t e) {
    ristretto_dword_t w = ((ristretto_word_t)e) ^ ((ristretto_word_t)RISTRETTO_SUCCESS);
    return (w-1)>>RISTRETTO_WORD_BITS;
}

/** Overwrite data with zeros.  Uses memset_s if available. */
void ristretto_bzero (
    void *data,
    size_t size
) RISTRETTO_NONNULL;

/** Compare two buffers, returning RISTRETTO_TRUE if they are equal. */
ristretto_bool_t ristretto_memeq (
    const void *data1,
    const void *data2,
    size_t size
) RISTRETTO_NONNULL RISTRETTO_WARN_UNUSED;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __RISTRETTO255_COMMON_H__ */
