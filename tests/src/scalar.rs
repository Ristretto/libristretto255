use rand::{CryptoRng, Rng};
use std::{
    fmt::{self, Debug},
    mem,
    ops::{Add, Mul, Sub},
};

use libristretto255_sys::*;
use util::convert_bool;

/// Scalars (i.e. wrapper around `ristretto255_scalar_t`)
#[derive(Copy, Clone)]
pub struct Scalar(pub(crate) ristretto255_scalar_t);

impl Scalar {
    /// Return a randomly generated `Scalar`
    ///
    /// This implementation isn't great as it only generates random scalars
    /// within the 64-bit unsigned range.
    ///
    /// Ideally it could generate larger ones.
    pub fn random<T: Rng + CryptoRng>(rng: &mut T) -> Self {
        Scalar::from(rng.gen::<u64>())
    }
}

// ------------------------------------------------------------------------
// Equality
// ------------------------------------------------------------------------

impl PartialEq for Scalar {
    fn eq(&self, other: &Scalar) -> bool {
        convert_bool(unsafe { ristretto255_scalar_eq(&self.0, &other.0) })
    }
}

impl Eq for Scalar {}

// ------------------------------------------------------------------------
// Arithmetic
// ------------------------------------------------------------------------

impl Add<Scalar> for Scalar {
    type Output = Scalar;

    fn add(self, other: Scalar) -> Scalar {
        let mut result = uninitialized_scalar_t();

        unsafe {
            ristretto255_scalar_add(&mut result, &self.0, &other.0);
        }

        Scalar(result)
    }
}

impl Sub<Scalar> for Scalar {
    type Output = Scalar;

    fn sub(self, other: Scalar) -> Scalar {
        let mut result = uninitialized_scalar_t();

        unsafe {
            ristretto255_scalar_sub(&mut result, &self.0, &other.0);
        }

        Scalar(result)
    }
}

impl Mul<Scalar> for Scalar {
    type Output = Scalar;

    fn mul(self, other: Scalar) -> Scalar {
        let mut result = uninitialized_scalar_t();

        unsafe {
            ristretto255_scalar_mul(&mut result, &self.0, &other.0);
        }

        Scalar(result)
    }
}

// ------------------------------------------------------------------------
// Type conversions
// ------------------------------------------------------------------------

impl From<u64> for Scalar {
    fn from(x: u64) -> Scalar {
        let mut scalar = uninitialized_scalar_t();
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
        let mut bytes = [0u8; 32];
        unsafe {
            ristretto255_scalar_encode(bytes.as_mut_ptr(), &self.0);
        }
        write!(f, "Scalar({:?})", bytes)
    }
}

/// Create an uninitialized (i.e. zero-initialized) `ristretto_scalar_t`
fn uninitialized_scalar_t() -> ristretto255_scalar_t {
    unsafe { mem::zeroed() }
}
