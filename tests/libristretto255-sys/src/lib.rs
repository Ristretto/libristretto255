//! Rust binding to libristretto255 intended for testing

#![crate_name = "libristretto255_sys"]
#![crate_type = "rlib"]
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
