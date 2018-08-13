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
extern crate libristretto255_sys;

use std::fmt::{self, Debug};
use std::mem;
use std::ops::{Add, Mul, Neg, Sub};

use libristretto255_sys::{
    ristretto255_point_add, ristretto255_point_base, ristretto255_point_decode,
    ristretto255_point_encode, ristretto255_point_eq, ristretto255_point_from_hash_uniform,
    ristretto255_point_identity, ristretto255_point_negate, ristretto255_point_s,
    ristretto255_point_scalarmul, ristretto255_point_sub, ristretto255_scalar_add,
    ristretto255_scalar_eq, ristretto255_scalar_mul, ristretto255_scalar_s,
    ristretto255_scalar_set_unsigned, ristretto255_scalar_sub, ristretto_bool_t, ristretto_error_t,
    ristretto_error_t_RISTRETTO_FAILURE as RISTRETTO_FAILURE,
    ristretto_error_t_RISTRETTO_SUCCESS as RISTRETTO_SUCCESS, RISTRETTO_FALSE, RISTRETTO_TRUE,
};

mod constants;

/// Convert a `ristretto_bool_t` to a Rust boolean
///
/// Panics if the value is unexpected
fn convert_bool(value: ristretto_bool_t) -> bool {
    unsafe {
        if value == RISTRETTO_TRUE {
            return true;
        } else if value == RISTRETTO_FALSE {
            return false;
        }
    }

    panic!("unexpected ristretto_bool_t: {}");
}

/// Opaque error type
#[derive(Debug)]
pub struct Error;

/// Convert a `ristretto_error_t` and the given value into a `Result<T, Error`>
fn convert_result<T>(value: T, error: ristretto_error_t) -> Result<T, Error> {
    match error {
        RISTRETTO_SUCCESS => Ok(value),
        RISTRETTO_FAILURE => Err(Error),
        other => panic!("unexpected ristretto_error_t: {}", other),
    }
}

/// Create an uninitialized (i.e. zero-initialized) `ristretto_scalar_s`
fn uninitialized_scalar_s() -> ristretto255_scalar_s {
    unsafe { mem::zeroed() }
}

/// Scalars (i.e. wrapper around `ristretto255_scalar_s`)
#[derive(Copy, Clone)]
pub struct Scalar(ristretto255_scalar_s);

// ------------------------------------------------------------------------
// Equality
// ------------------------------------------------------------------------

impl PartialEq for Scalar {
    fn eq(&self, other: &Scalar) -> bool {
        // TODO: less hacky workaround around mutability problems
        convert_bool(unsafe { ristretto255_scalar_eq(&mut self.clone().0, &mut other.clone().0) })
    }
}

impl Eq for Scalar {}

// ------------------------------------------------------------------------
// Arithmetic
// ------------------------------------------------------------------------

impl Add<Scalar> for Scalar {
    type Output = Scalar;

    fn add(self, other: Scalar) -> Scalar {
        let mut result = uninitialized_scalar_s();

        // TODO: less hacky workaround around mutability problems
        unsafe {
            ristretto255_scalar_add(&mut result, &mut self.clone().0, &mut other.clone().0);
        }

        Scalar(result)
    }
}

impl Sub<Scalar> for Scalar {
    type Output = Scalar;

    fn sub(self, other: Scalar) -> Scalar {
        let mut result = uninitialized_scalar_s();

        // TODO: less hacky workaround around mutability problems
        unsafe {
            ristretto255_scalar_sub(&mut result, &mut self.clone().0, &mut other.clone().0);
        }

        Scalar(result)
    }
}

impl Mul<Scalar> for Scalar {
    type Output = Scalar;

    fn mul(self, other: Scalar) -> Scalar {
        let mut result = uninitialized_scalar_s();

        // TODO: less hacky workaround around mutability problems
        unsafe {
            ristretto255_scalar_mul(&mut result, &mut self.clone().0, &mut other.clone().0);
        }

        Scalar(result)
    }
}

// ------------------------------------------------------------------------
// Type conversions
// ------------------------------------------------------------------------

impl From<u64> for Scalar {
    fn from(x: u64) -> Scalar {
        let mut scalar = uninitialized_scalar_s();
        unsafe {
            ristretto255_scalar_set_unsigned(&mut scalar, x);
        }
        Scalar(scalar)
    }
}

// ------------------------------------------------------------------------
// Debug traits
// ------------------------------------------------------------------------

impl Debug for Scalar {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Scalar(TODO: ???)")
    }
}

/// Compressed Ristretto255 point
#[derive(Copy, Clone, Eq, PartialEq)]
pub struct CompressedRistretto(pub [u8; 32]);

impl CompressedRistretto {
    /// Copy the bytes of this `CompressedRistretto`.
    pub fn to_bytes(&self) -> [u8; 32] {
        self.0
    }

    /// View this `CompressedRistretto` as an array of bytes.
    pub fn as_bytes(&self) -> &[u8; 32] {
        &self.0
    }

    /// Attempt to decompress to an `RistrettoPoint`.
    ///
    /// # Return
    ///
    /// - `Some(RistrettoPoint)` if `self` was the canonical encoding of a point;
    ///
    /// - `None` if `self` was not the canonical encoding of a point.
    pub fn decompress(&self) -> Option<RistrettoPoint> {
        let mut point = uninitialized_point_s();

        let error = unsafe {
            ristretto255_point_decode(
                &mut point,
                self.0.as_ptr(),
                RISTRETTO_TRUE, // Allow identity for testing
            )
        };

        // TODO: check `ristretto255_point_valid`?
        convert_result(point.into(), error).ok()
    }
}

impl CompressedRistretto {
    pub fn identity() -> CompressedRistretto {
        CompressedRistretto([0u8; 32])
    }
}

impl Default for CompressedRistretto {
    fn default() -> CompressedRistretto {
        Self::identity()
    }
}

/// Create an uninitialized (i.e. zero-initialized) `ristretto_point_s`
fn uninitialized_point_s() -> ristretto255_point_s {
    unsafe { mem::zeroed() }
}

/// A `RistrettoPoint` represents a point in the Ristretto group for
/// Curve25519 (a.k.a. Ristretto255)
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct RistrettoPoint(ristretto255_point_s);

impl RistrettoPoint {
    /// Compress this point using the Ristretto encoding.
    pub fn compress(&mut self) -> CompressedRistretto {
        let mut bytes = [0u8; 32];

        unsafe {
            ristretto255_point_encode(bytes.as_mut_ptr(), &mut self.0);
        }

        CompressedRistretto(bytes)
    }

    /// Construct a `RistrettoPoint` from 64 bytes of data.
    pub fn from_uniform_bytes(bytes: &[u8; 64]) -> RistrettoPoint {
        let mut point = uninitialized_point_s();

        unsafe {
            ristretto255_point_from_hash_uniform(&mut point, bytes.as_ptr());
        }

        RistrettoPoint(point)
    }

    /// Return the coset self + E[4], for debugging.
    fn coset4(self) -> [Self; 4] {
        [
            self,
            self + constants::EIGHT_TORSION[2].into(),
            self + constants::EIGHT_TORSION[4].into(),
            self + constants::EIGHT_TORSION[6].into(),
        ]
    }
}

impl RistrettoPoint {
    pub fn basepoint() -> RistrettoPoint {
        RistrettoPoint(unsafe { ristretto255_point_base[0].clone() })
    }

    pub fn identity() -> RistrettoPoint {
        RistrettoPoint(unsafe { ristretto255_point_identity[0].clone() })
    }
}

impl Default for RistrettoPoint {
    fn default() -> RistrettoPoint {
        Self::identity()
    }
}

impl From<ristretto255_point_s> for RistrettoPoint {
    fn from(point: ristretto255_point_s) -> RistrettoPoint {
        RistrettoPoint(point)
    }
}

// ------------------------------------------------------------------------
// Equality
// ------------------------------------------------------------------------

impl PartialEq for RistrettoPoint {
    fn eq(&self, other: &RistrettoPoint) -> bool {
        // TODO: less hacky workaround around mutability problems
        convert_bool(unsafe { ristretto255_point_eq(&mut self.clone().0, &mut other.clone().0) })
    }
}

impl Eq for RistrettoPoint {}

// ------------------------------------------------------------------------
// Arithmetic
// ------------------------------------------------------------------------

impl Add<RistrettoPoint> for RistrettoPoint {
    type Output = RistrettoPoint;

    fn add(self, other: RistrettoPoint) -> RistrettoPoint {
        let mut result = uninitialized_point_s();

        // TODO: less hacky workaround around mutability problems
        unsafe {
            ristretto255_point_add(&mut result, &mut self.clone().0, &mut other.clone().0);
        }

        RistrettoPoint(result)
    }
}

impl Sub<RistrettoPoint> for RistrettoPoint {
    type Output = RistrettoPoint;

    fn sub(self, other: RistrettoPoint) -> RistrettoPoint {
        let mut result = uninitialized_point_s();

        // TODO: less hacky workaround around mutability problems
        unsafe {
            ristretto255_point_sub(&mut result, &mut self.clone().0, &mut other.clone().0);
        }

        RistrettoPoint(result)
    }
}

impl Neg for RistrettoPoint {
    type Output = RistrettoPoint;

    fn neg(self) -> RistrettoPoint {
        let mut result = uninitialized_point_s();

        // TODO: less hacky workaround around mutability problems
        unsafe {
            ristretto255_point_negate(&mut result, &mut self.clone().0);
        }

        RistrettoPoint(result)
    }
}

impl Mul<Scalar> for RistrettoPoint {
    type Output = RistrettoPoint;

    /// Scalar multiplication: compute `scalar * self`.
    fn mul(self, scalar: Scalar) -> RistrettoPoint {
        let mut result = uninitialized_point_s();

        // TODO: less hacky workaround around mutability problems
        unsafe {
            ristretto255_point_scalarmul(&mut result, &mut self.clone().0, &mut scalar.clone().0);
        }

        RistrettoPoint(result)
    }
}

impl Mul<RistrettoPoint> for Scalar {
    type Output = RistrettoPoint;

    /// Scalar multiplication: compute `self * scalar`.
    fn mul(self, point: RistrettoPoint) -> RistrettoPoint {
        point * self
    }
}

// ------------------------------------------------------------------------
// Conversions from curve25519-dalek types (for debugging/testing)
// ------------------------------------------------------------------------

impl From<curve25519_dalek::ristretto::RistrettoPoint> for RistrettoPoint {
    fn from(dalek_ristretto_point: curve25519_dalek::ristretto::RistrettoPoint) -> Self {
        panic!("dalek point is: {:?}", dalek_ristretto_point);
    }
}

// ------------------------------------------------------------------------
// Debug traits
// ------------------------------------------------------------------------

impl Debug for CompressedRistretto {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "CompressedRistretto: {:?}", self.as_bytes())
    }
}

impl Debug for RistrettoPoint {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let coset = self.coset4();
        write!(
            f,
            "RistrettoPoint: coset \n{:?}\n{:?}\n{:?}\n{:?}",
            coset[0].0, coset[1].0, coset[2].0, coset[3].0
        )
    }
}

// ------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------

#[cfg(test)]
mod test {
    use super::*;

    // TODO: fix segv
    //#[test]
    //fn scalarmult_ristrettopoint_works_both_ways() {
    //    let P = RistrettoPoint::basepoint();
    //    let s = Scalar::from(999u64);
    //
    //    let mut P1 = P * s;
    //    let mut P2 = s * P;
    //
    //    assert!(P1.compress().as_bytes() == P2.compress().as_bytes());
    //}

    // TODO: fix segv
    //#[test]
    //fn decompress_id() {
    //    let compressed_id = CompressedRistretto::identity();
    //    let id = compressed_id.decompress().unwrap();
    //    let mut identity_in_coset = false;
    //    for P in id.coset4().iter_mut() {
    //        if P.compress() == CompressedRistretto::identity() {
    //            identity_in_coset = true;
    //        }
    //    }
    //    assert!(identity_in_coset);
    //}

    // TODO: fix segv
    //#[test]
    //fn compress_id() {
    //    let mut id = RistrettoPoint::identity();
    //    assert_eq!(id.compress(), CompressedRistretto::identity());
    //}

    // TODO: fix segv
    //#[test]
    //fn basepoint_roundtrip() {
    //    let mut bp = RistrettoPoint::basepoint();
    //    let bp_compressed_ristretto = bp.compress();
    //    assert_eq!(bp_compressed_ristretto.decompress().unwrap(), bp);
    //}

    // TODO: Scalar::random() (and probably segv)
    //#[test]
    //fn random_roundtrip() {
    //    let mut rng = OsRng::new().unwrap();
    //    let B = &constants::RISTRETTO_BASEPOINT_TABLE;
    //    for _ in 0..100 {
    //        let P = B * Scalar::random(&mut rng);
    //        let compressed_P = P.compress();
    //        let Q = compressed_P.decompress().unwrap();
    //        assert_eq!(P, Q);
    //    }
    //}
}
