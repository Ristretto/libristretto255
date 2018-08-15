use curve25519_dalek;
use libristretto255_sys::*;
use std::{
    fmt::{self, Debug},
    mem,
    ops::{Add, Mul, Neg, Sub},
};

use constants;
use scalar::Scalar;
use util::{convert_bool, convert_result};

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
        let mut point = uninitialized_point_t();

        let error = unsafe {
            ristretto255_point_decode(
                &mut point,
                self.0.as_ptr(),
                RISTRETTO_TRUE, // Allow identity for testing
            )
        };

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

/// Create an uninitialized (i.e. zero-initialized) `ristretto255_point_t`
fn uninitialized_point_t() -> ristretto255_point_t {
    unsafe { mem::zeroed() }
}

/// A `RistrettoPoint` represents a point in the Ristretto group for
/// Curve25519 (a.k.a. Ristretto255)
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct RistrettoPoint(ristretto255_point_t);

impl RistrettoPoint {
    /// Compress this point using the Ristretto encoding.
    pub fn compress(&self) -> CompressedRistretto {
        let mut bytes = [0u8; 32];

        unsafe {
            ristretto255_point_encode(bytes.as_mut_ptr(), &self.0);
        }

        CompressedRistretto(bytes)
    }

    /// Construct a `RistrettoPoint` from 64 bytes of data.
    pub fn from_uniform_bytes(bytes: &[u8; 64]) -> RistrettoPoint {
        let mut point = uninitialized_point_t();

        unsafe {
            ristretto255_point_from_hash_uniform(&mut point, bytes.as_ptr());
        }

        RistrettoPoint(point)
    }

    /// Return the coset self + E[4], for debugging.
    /// TODO: double check the `EIGHT_TORSION` table is correct
    pub fn coset4(self) -> [Self; 4] {
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
        RistrettoPoint(unsafe { ristretto255_point_base })
    }

    pub fn identity() -> RistrettoPoint {
        RistrettoPoint(unsafe { ristretto255_point_identity })
    }
}

impl Default for RistrettoPoint {
    fn default() -> RistrettoPoint {
        Self::identity()
    }
}

impl From<ristretto255_point_t> for RistrettoPoint {
    fn from(point: ristretto255_point_t) -> RistrettoPoint {
        RistrettoPoint(point)
    }
}

// ------------------------------------------------------------------------
// Equality
// ------------------------------------------------------------------------

impl PartialEq for RistrettoPoint {
    fn eq(&self, other: &RistrettoPoint) -> bool {
        convert_bool(unsafe { ristretto255_point_eq(&self.0, &other.0) })
    }
}

impl Eq for RistrettoPoint {}

// ------------------------------------------------------------------------
// Arithmetic
// ------------------------------------------------------------------------

impl Add<RistrettoPoint> for RistrettoPoint {
    type Output = RistrettoPoint;

    fn add(self, other: RistrettoPoint) -> RistrettoPoint {
        let mut result = uninitialized_point_t();

        unsafe {
            ristretto255_point_add(&mut result, &self.0, &other.0);
        }

        RistrettoPoint(result)
    }
}

impl Sub<RistrettoPoint> for RistrettoPoint {
    type Output = RistrettoPoint;

    fn sub(self, other: RistrettoPoint) -> RistrettoPoint {
        let mut result = uninitialized_point_t();

        unsafe {
            ristretto255_point_sub(&mut result, &self.0, &other.0);
        }

        RistrettoPoint(result)
    }
}

impl Neg for RistrettoPoint {
    type Output = RistrettoPoint;

    fn neg(self) -> RistrettoPoint {
        let mut result = uninitialized_point_t();

        unsafe {
            ristretto255_point_negate(&mut result, &self.0);
        }

        RistrettoPoint(result)
    }
}

impl Mul<Scalar> for RistrettoPoint {
    type Output = RistrettoPoint;

    /// Scalar multiplication: compute `scalar * self`.
    fn mul(self, scalar: Scalar) -> RistrettoPoint {
        let mut result = uninitialized_point_t();

        unsafe {
            ristretto255_point_scalarmul(&mut result, &self.0, &scalar.0);
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
        panic!("unimplemented: dalek point is: {:?}", dalek_ristretto_point);
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
