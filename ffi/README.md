# rocksdb-cloud FFI wrapper

This folder contains a small C ABI wrapper for RocksDB Cloud and a Makefile to build
`librocksdb_cloud_ffi` for use by the Rust reader.

## Build (macOS)

If you have the AWS SDK installed at `/Users/suman/Projects/aws-sdk-cpp/build-legacy/install`,
run:

```sh
make AWS_SDK_INCLUDE=/Users/suman/Projects/aws-sdk-cpp/build-legacy/install/include \
  AWS_SDK_LIB_DIR=/Users/suman/Projects/aws-sdk-cpp/build-legacy/install/lib \
  LDFLAGS='-L/opt/homebrew/opt/openssl@3/lib -L/Users/suman/Projects/aws-sdk-cpp/build-legacy/install/lib'
```

Or use the helper target (same paths baked in):

```sh
make build-aws
```

The output library is written to `build/librocksdb_cloud_ffi.dylib`.

## Build/run the Rust reader

- export `ROCKSDB_CLOUD_FFI_DIR=/Users/suman/Projects/rocksdb-cloud/ffi/build`
- export `DYLD_LIBRARY_PATH=/Users/suman/Projects/rocksdb-cloud/ffi/build:/Users/suman/Projects/rocksdb-cloud:/Users/suman/Projects/aws-sdk-cpp/build-legacy/install/lib:/opt/homebrew/opt/openssl@3/lib`
- run `cargo run` in `reader-server-rs` (with the same `CLOUD_*` env vars you use for the Java reader)

If you want, I can run the Rust build and smoke-test the `/healthz` endpoint.

Suggestions:

1. Run `cargo build` in `reader-server-rs` with the env vars above.
2. Start the reader and verify `GET /healthz` and `GET /checkpoint` responses.
