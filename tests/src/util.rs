use libristretto255_sys::{
    ristretto_bool_t, ristretto_error_t, RISTRETTO_FAILURE, RISTRETTO_FALSE, RISTRETTO_SUCCESS,
    RISTRETTO_TRUE,
};

/// Convert a `ristretto_bool_t` to a Rust boolean
///
/// Panics if the value is unexpected
pub fn convert_bool(value: ristretto_bool_t) -> bool {
    unsafe {
        if value == RISTRETTO_TRUE {
            return true;
        } else if value == RISTRETTO_FALSE {
            return false;
        }
    }

    panic!("unexpected ristretto_bool_t: {}", value);
}

/// Opaque error type
#[derive(Debug)]
pub struct Error;

/// Convert a `ristretto_error_t` and the given value into a `Result<T, Error`>
pub fn convert_result<T>(value: T, error: ristretto_error_t) -> Result<T, Error> {
    match error {
        RISTRETTO_SUCCESS => Ok(value),
        RISTRETTO_FAILURE => Err(Error),
        other => panic!("unexpected ristretto_error_t: {}", other),
    }
}
