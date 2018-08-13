extern crate bindgen;

use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rustc-link-search=../build/lib");
    println!("cargo:rustc-link-lib=ristretto255");

    let bindings = bindgen::Builder::default()
        .clang_arg("-I../../include")
        .header("src/wrapper.hpp")
        .generate()
        .unwrap();

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .unwrap();
}
