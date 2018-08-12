/* Copyright (c) 2015 Cryptography Research, Inc.
 * Released under the MIT License.  See LICENSE.txt for license information.
 */

/**
 * @file bzero.c
 * @author Mike Hamburg
 * @brief Memory zeroing functionality
 */

#include <ristretto255/common.h>

void ristretto255_bzero (
    void *s,
    size_t size
) {
#ifdef __STDC_LIB_EXT1__
    memset_s(s, size, 0, size);
#else
    const size_t sw = sizeof(ristretto255_word_t);
    volatile uint8_t *destroy = (volatile uint8_t *)s;
    for (; size && ((uintptr_t)destroy)%sw; size--, destroy++)
        *destroy = 0;
    for (; size >= sw; size -= sw, destroy += sw)
        *(volatile ristretto255_word_t *)destroy = 0;
    for (; size; size--, destroy++)
        *destroy = 0;
#endif
}
