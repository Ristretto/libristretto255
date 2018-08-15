//! Tests for `libristretto255`.
//!
//! NOTE: If you are looking for a production-quality Rust implementation
//! of Ristretto then these are not the droids, or rather daleks, that you
//! are looking for!
//!
//! Please use curve25519-dalek's upstream pure-Rust implementation instead!
//!
//! https://docs.rs/curve25519-dalek/latest/curve25519_dalek/ristretto/index.html

// Adapted from curve25519-dalek's ristretto.rs
// Copyright (c) 2016-2018 Isis Agora Lovecruft, Henry de Valence. All rights reserved.
//
// BSD Revised License (https://spdx.org/licenses/BSD-3-Clause.html)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Authors:
// - Isis Agora Lovecruft <isis@patternsinthevoid.net>
// - Henry de Valence <hdevalence@hdevalence.ca>

extern crate curve25519_dalek;
extern crate hex;
extern crate libristretto255_sys;
extern crate rand;
extern crate sha2;

pub mod constants;
pub mod ristretto;
pub mod scalar;
pub mod util;
#[cfg(test)]
pub mod vectors;

#[cfg(test)]
#[allow(non_snake_case)]
mod test {
    use rand::OsRng;

    use ristretto::{CompressedRistretto, RistrettoPoint};
    use scalar::Scalar;

    #[test]
    fn scalarmult_ristrettopoint_works_both_ways() {
        let P = RistrettoPoint::basepoint();
        let s = Scalar::from(999u64);

        let P1 = P * s;
        let P2 = s * P;

        assert!(P1.compress().as_bytes() == P2.compress().as_bytes());
    }

    #[test]
    fn decompress_id() {
        let compressed_id = CompressedRistretto::identity();
        let id = compressed_id.decompress().unwrap();
        let mut identity_in_coset = false;

        for P in id.coset4().iter_mut() {
            if P.compress() == CompressedRistretto::identity() {
                identity_in_coset = true;
            }
        }

        assert!(identity_in_coset);
    }

    #[test]
    fn compress_id() {
        let id = RistrettoPoint::identity();
        assert_eq!(id.compress(), CompressedRistretto::identity());
    }

    #[test]
    fn basepoint_roundtrip() {
        let bp = RistrettoPoint::basepoint();
        let bp_compressed_ristretto = bp.compress();
        assert_eq!(bp_compressed_ristretto.decompress().unwrap(), bp);
    }

    #[test]
    fn random_roundtrip() {
        let mut rng = OsRng::new().unwrap();
        let B = RistrettoPoint::basepoint();

        for _ in 0..100 {
            let P = B * Scalar::random(&mut rng);
            let compressed_P = P.compress();
            let Q = compressed_P.decompress().unwrap();

            assert_eq!(P, Q);
        }
    }
}
