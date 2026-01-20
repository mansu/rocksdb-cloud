use std::env;

fn main() {
    if env::var_os("CARGO_FEATURE_FFI").is_none() {
        return;
    }
    let dir = env::var("ROCKSDB_CLOUD_FFI_DIR")
        .expect("ROCKSDB_CLOUD_FFI_DIR must point to the directory with librocksdb_cloud_ffi");
    println!("cargo:rustc-link-search=native={}", dir);
    println!("cargo:rustc-link-lib=dylib=rocksdb_cloud_ffi");
    println!("cargo:rerun-if-env-changed=ROCKSDB_CLOUD_FFI_DIR");
}
