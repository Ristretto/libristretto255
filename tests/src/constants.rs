#![allow(unknown_lints, unreadable_literal)]

use libristretto255_sys::{gf_25519_t, ristretto255_point_t};

macro_rules! field_literal {
    ($($elem:tt)+) => {
        gf_25519_t {
            limb: [$($elem)+],
            #[cfg(target_pointer_width = "64")]
            __bindgen_padding_0: [0, 0, 0],
        }
    };
}

/// Eight torsion table taken directly from curve25519-dalek
/// TODO: this is probably completely bogus and only works on 64-bit architectures
pub const EIGHT_TORSION: [ristretto255_point_t; 8] = [
    ristretto255_point_t {
        x: field_literal!(0, 0, 0, 0, 0),
        y: field_literal!(1, 0, 0, 0, 0),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(0, 0, 0, 0, 0),
    },
    ristretto255_point_t {
        x: field_literal!(
            358744748052810,
            1691584618240980,
            977650209285361,
            1429865912637724,
            560044844278676
        ),
        y: field_literal!(
            84926274344903,
            473620666599931,
            365590438845504,
            1028470286882429,
            2146499180330972
        ),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(
            1448326834587521,
            1857896831960481,
            1093722731865333,
            1677408490711241,
            1915505153018406
        ),
    },
    ristretto255_point_t {
        x: field_literal!(
            533094393274173,
            2016890930128738,
            18285341111199,
            134597186663265,
            1486323764102114
        ),
        y: field_literal!(0, 0, 0, 0, 0),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(0, 0, 0, 0, 0),
    },
    ristretto255_point_t {
        x: field_literal!(
            358744748052810,
            1691584618240980,
            977650209285361,
            1429865912637724,
            560044844278676
        ),
        y: field_literal!(
            2166873539340326,
            1778179147085316,
            1886209374839743,
            1223329526802818,
            105300633354275
        ),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(
            803472979097708,
            393902981724766,
            1158077081819914,
            574391322974006,
            336294660666841
        ),
    },
    ristretto255_point_t {
        x: field_literal!(0, 0, 0, 0, 0),
        y: field_literal!(
            2251799813685228,
            2251799813685247,
            2251799813685247,
            2251799813685247,
            2251799813685247
        ),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(0, 0, 0, 0, 0),
    },
    ristretto255_point_t {
        x: field_literal!(
            1893055065632419,
            560215195444267,
            1274149604399886,
            821933901047523,
            1691754969406571
        ),
        y: field_literal!(
            2166873539340326,
            1778179147085316,
            1886209374839743,
            1223329526802818,
            105300633354275
        ),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(
            1448326834587521,
            1857896831960481,
            1093722731865333,
            1677408490711241,
            1915505153018406
        ),
    },
    ristretto255_point_t {
        x: field_literal!(
            1718705420411056,
            234908883556509,
            2233514472574048,
            2117202627021982,
            765476049583133
        ),
        y: field_literal!(0, 0, 0, 0, 0),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(0, 0, 0, 0, 0),
    },
    ristretto255_point_t {
        x: field_literal!(
            1893055065632419,
            560215195444267,
            1274149604399886,
            821933901047523,
            1691754969406571
        ),
        y: field_literal!(
            84926274344903,
            473620666599931,
            365590438845504,
            1028470286882429,
            2146499180330972
        ),
        z: field_literal!(1, 0, 0, 0, 0),
        t: field_literal!(
            803472979097708,
            393902981724766,
            1158077081819914,
            574391322974006,
            336294660666841
        ),
    },
];
