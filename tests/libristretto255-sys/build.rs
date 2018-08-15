fn main() {
    println!("cargo:rustc-link-search=../build/lib");
    println!("cargo:rustc-link-lib=static=ristretto255");
}
