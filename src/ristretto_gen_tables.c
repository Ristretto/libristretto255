/**
 * @file curve25519/ristretto_gen_tables.c
 * @author Mike Hamburg
 *
 * @copyright
 *   Copyright (c) 2015-2018 Ristretto Developers, Cryptography Research, Inc.  \n
 *   Released under the MIT License.  See LICENSE.txt for license information.
 *
 * @brief Ristretto255 global constant table precomputation.
 */
#define _XOPEN_SOURCE 600 /* for posix_memalign */
#include <stdio.h>
#include <stdlib.h>

#include "field.h"
#include "f_field.h"
#include "ristretto255/common.h"
#include "ristretto255/point.h"

static const unsigned char base_point_ser_for_pregen[SER_BYTES] = {
    0xe2, 0xf2, 0xae, 0x0a, 0x6a, 0xbc, 0x4e, 0x71, 0xa8, 0x84, 0xa9, 0x61, 0xc5, 0x00, 0x51, 0x5f, 0x58, 0xe3, 0x0b, 0x6a, 0xa5, 0x82, 0xdd, 0x8d, 0xb6, 0xa6, 0x59, 0x45, 0xe0, 0x8d, 0x2d, 0x76
};

/* To satisfy linker. */
const gf ristretto255_precomputed_base_as_fe[1];
const ristretto255_point_t ristretto255_point_base;

struct niels_s;
const gf_s *ristretto255_precomputed_wnaf_as_fe;
extern const size_t ristretto255_sizeof_precomputed_wnafs;

void ristretto255_precompute_wnafs (
    struct niels_s *out,
    const ristretto255_point_t base
);
static void field_print(const gf f) {
    unsigned char ser[X_SER_BYTES];
    gf_serialize(ser,f,1);
    int b=0, i, comma=0;
    unsigned long long limb = 0;
    printf("{FIELD_LITERAL(");
    for (i=0; i<X_SER_BYTES; i++) {
        limb |= ((uint64_t)ser[i])<<b;
        b += 8;
        if (b >= GF_LIT_LIMB_BITS || i == SER_BYTES-1) {
            limb &= (1ull<<GF_LIT_LIMB_BITS) -1;
            b -= GF_LIT_LIMB_BITS;
            if (comma) printf(",");
            comma = 1;
            printf("0x%016llx", limb);
            limb = ((uint64_t)ser[i])>>(8-b);
        }
    }
    printf(")}");
    assert(b<8);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    ristretto255_point_t real_point_base;
    int ret = ristretto255_point_decode(real_point_base,base_point_ser_for_pregen,0);
    if (ret != RISTRETTO_SUCCESS) {
        fprintf(stderr, "Can't decode base point!\n");
        return 1;
    }

    ristretto255_precomputed_s *pre;
    ret = posix_memalign((void**)&pre, ristretto255_alignof_precomputed_s, ristretto255_sizeof_precomputed_s);
    if (ret || !pre) {
        fprintf(stderr, "Can't allocate space for precomputed table\n");
        return 1;
    }
    ristretto255_precompute(pre, real_point_base);

    struct niels_s *pre_wnaf;
    ret = posix_memalign((void**)&pre_wnaf, ristretto255_alignof_precomputed_s, ristretto255_sizeof_precomputed_wnafs);
    if (ret || !pre_wnaf) {
        fprintf(stderr, "Can't allocate space for precomputed WNAF table\n");
        return 1;
    }
    ristretto255_precompute_wnafs(pre_wnaf, real_point_base);

    const gf_s *output;
    unsigned i;

    printf("/** @warning: this file was automatically generated. */\n");
    printf("#include \"field.h\"\n\n");
    printf("#include <ristretto255/common.h>\n\n");
    printf("#include <ristretto255/point.h>\n\n");
    printf("#define ristretto255__id ristretto255_##_id\n");

    output = (const gf_s *)real_point_base;
    printf("const ristretto255_point_t ristretto255_point_base = {{\n");
    for (i=0; i < sizeof(ristretto255_point_t); i+=sizeof(gf)) {
        if (i) printf(",\n  ");
        field_print(output++);
    }
    printf("\n}};\n");

    output = (const gf_s *)pre;
    printf("const gf ristretto255_precomputed_base_as_fe[%d]\n",
        (int)(ristretto255_sizeof_precomputed_s / sizeof(gf)));
    printf("VECTOR_ALIGNED __attribute__((visibility(\"hidden\"))) = {\n  ");

    for (i=0; i < ristretto255_sizeof_precomputed_s; i+=sizeof(gf)) {
        if (i) printf(",\n  ");
        field_print(output++);
    }
    printf("\n};\n");

    output = (const gf_s *)pre_wnaf;
    printf("const gf ristretto255_precomputed_wnaf_as_fe[%d]\n",
        (int)(ristretto255_sizeof_precomputed_wnafs / sizeof(gf)));
    printf("VECTOR_ALIGNED __attribute__((visibility(\"hidden\"))) = {\n  ");
    for (i=0; i < ristretto255_sizeof_precomputed_wnafs; i+=sizeof(gf)) {
        if (i) printf(",\n  ");
        field_print(output++);
    }
    printf("\n};\n");

    return 0;
}
