/** @brief Ristretto$(gf_bits) global constant table precomputation. */

#define _XOPEN_SOURCE 600 /* for posix_memalign */
#include <stdio.h>
#include <stdlib.h>

#include "field.h"
#include "f_field.h"
#include "ristretto$(gf_bits)/common.h"
#include "ristretto$(gf_bits)/point.h"

static const unsigned char base_point_ser_for_pregen[SER_BYTES] = {
    $(ser(rist_base_decoded,8))
};

 /* To satisfy linker. */
const gf ristretto$(gf_bits)_precomputed_base_as_fe[1];
const ristretto$(gf_bits)_point_t ristretto$(gf_bits)_point_base;

struct niels_s;
const gf_s *ristretto$(gf_bits)_precomputed_wnaf_as_fe;
extern const size_t ristretto$(gf_bits)_sizeof_precomputed_wnafs;

void ristretto$(gf_bits)_precompute_wnafs (
    struct niels_s *out,
    const ristretto$(gf_bits)_point_t base
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
    
    ristretto$(gf_bits)_point_t real_point_base;
    int ret = ristretto$(gf_bits)_point_decode(real_point_base,base_point_ser_for_pregen,0);
    if (ret != RISTRETTO_SUCCESS) {
        fprintf(stderr, "Can't decode base point!\n");
        return 1;
    }
    
    ristretto$(gf_bits)_precomputed_s *pre;
    ret = posix_memalign((void**)&pre, ristretto$(gf_bits)_alignof_precomputed_s, ristretto$(gf_bits)_sizeof_precomputed_s);
    if (ret || !pre) {
        fprintf(stderr, "Can't allocate space for precomputed table\n");
        return 1;
    }
    ristretto$(gf_bits)_precompute(pre, real_point_base);
    
    struct niels_s *pre_wnaf;
    ret = posix_memalign((void**)&pre_wnaf, ristretto$(gf_bits)_alignof_precomputed_s, ristretto$(gf_bits)_sizeof_precomputed_wnafs);
    if (ret || !pre_wnaf) {
        fprintf(stderr, "Can't allocate space for precomputed WNAF table\n");
        return 1;
    }
    ristretto$(gf_bits)_precompute_wnafs(pre_wnaf, real_point_base);

    const gf_s *output;
    unsigned i;
    
    printf("/** @warning: this file was automatically generated. */\n");
    printf("#include \"field.h\"\n\n");
    printf("#include <ristretto$(gf_bits)/common.h>\n\n");
    printf("#include <ristretto$(gf_bits)/point.h>\n\n");
    printf("#define ristretto$(gf_bits)__id ristretto$(gf_bits)_##_id\n");
    
    output = (const gf_s *)real_point_base;
    printf("const ristretto$(gf_bits)_point_t ristretto$(gf_bits)_point_base = {{\n");
    for (i=0; i < sizeof(ristretto$(gf_bits)_point_t); i+=sizeof(gf)) {
        if (i) printf(",\n  ");
        field_print(output++);
    }
    printf("\n}};\n");
    
    output = (const gf_s *)pre;
    printf("const gf ristretto$(gf_bits)_precomputed_base_as_fe[%d]\n", 
        (int)(ristretto$(gf_bits)_sizeof_precomputed_s / sizeof(gf)));
    printf("VECTOR_ALIGNED __attribute__((visibility(\"hidden\"))) = {\n  ");
    
    for (i=0; i < ristretto$(gf_bits)_sizeof_precomputed_s; i+=sizeof(gf)) {
        if (i) printf(",\n  ");
        field_print(output++);
    }
    printf("\n};\n");
    
    output = (const gf_s *)pre_wnaf;
    printf("const gf ristretto$(gf_bits)_precomputed_wnaf_as_fe[%d]\n", 
        (int)(ristretto$(gf_bits)_sizeof_precomputed_wnafs / sizeof(gf)));
    printf("VECTOR_ALIGNED __attribute__((visibility(\"hidden\"))) = {\n  ");
    for (i=0; i < ristretto$(gf_bits)_sizeof_precomputed_wnafs; i+=sizeof(gf)) {
        if (i) printf(",\n  ");
        field_print(output++);
    }
    printf("\n};\n");
    
    return 0;
}
