## RocksDB-Cloud: A Key-Value Store for Cloud Applications

RocksDB-Cloud is a C++ library that brings the power of RocksDB to AWS, Google Cloud and Microsoft Azure.
It leverages the power of RocksDB to provide fast key-value access to data stored
in Flash and RAM systems. It provides for data durability even in the face of
machine failures by integrations with cloud services like AWS-S3 and Google Cloud
Services. It allows a cost-effective way to utilize the rich hierarchy of
storage services (based on RAM, NvMe, SSD, Disk Cold Storage, etc) that are offered by
most cloud providers. RocksDB-Cloud is developed and maintained by the engineering
team at Rockset Inc. Start with https://github.com/rockset/rocksdb-cloud/tree/master/cloud.

RocksDB-Cloud provides three main advantages for AWS environments:

1. A rocksdb instance is durable. Continuous and automatic replication of db data and metadata to S3. In the event that the rocksdb machine dies, another process on any other EC2 machine can reopen the same rocksdb database (by configuring it with the S3 bucketname where the entire db state was stored).
2. A rocksdb instance is cloneable. RocksDB-Cloud support a primitive called zero-copy-clone() that allows a slave instance of rocksdb on another machine to clone an existing db. Both master and slave rocksdb instance can run in parallel and they share some set of common database files.
3. A rocksdb instance can leverage hierarchical storage. The entire rocksdb storage footprint need not be resident on local storage. S3 contains the entire database and the local storage contains only the files that are in the working set.

### Inherits from RocksDB:

RocksDB is developed and maintained by Facebook Database Engineering Team.
It is built on earlier work on [LevelDB](https://github.com/google/leveldb) by Sanjay Ghemawat (sanjay@google.com)
and Jeff Dean (jeff@google.com)

This code is a library that forms the core building block for a fast
key-value server, especially suited for storing data on flash drives.
It has a Log-Structured-Merge-Database (LSM) design with flexible tradeoffs
between Write-Amplification-Factor (WAF), Read-Amplification-Factor (RAF)
and Space-Amplification-Factor (SAF). It has multi-threaded compactions,
making it especially suitable for storing multiple terabytes of data in a
single database.

Start with example usage here: https://github.com/facebook/rocksdb/tree/main/examples

See the [github wiki](https://github.com/facebook/rocksdb/wiki) for more explanation.

The public interface is in `include/`.  Callers should not include or
rely on the details of any other header files in this package.  Those
internal APIs may be changed without warning.

Questions and discussions are welcome on the [RocksDB Developers Public](https://www.facebook.com/groups/rocksdb.dev/) Facebook group and [email list](https://groups.google.com/g/rocksdb) on Google Groups.

### Developer
To run necessary tests, use `run_tests.sh` script
```
./run_tests.sh -h
```

## Build the JNI jar yourself (if you don’t have a prebuilt)
- Build the JNI from the cloud fork (macOS example):
  1) Clone the branch `https://github.com/mansu/rocksdb-cloud/tree/build_cloud_jni`.
  2) In that repo: `USE_AWS=1 USE_RTTI=1 make jclean clean jtest rocksdbjava`.
  3) Copy the jar into the prebuilt location used by this repo:
     `cp ./java/target/rocksdbjni-9.1.1-osx.jar <REPO_ROOT>/tools/prebuilt/rocksdbjni-9.1.1-osx.jar`
- Once the jar is present, run `make install-prebuilt-rocksdb` (or `make jni` if you prefer to build/install directly into `~/.m2`).
- Then build the POC: `make build` or `mvn -pl rocksdb-sync-poc clean package`.

## Build the JNI image (Docker)
- Build the image (defaults to your Docker host platform):
  `make docker-build`
- To force a specific architecture (example amd64):
  `DOCKER_BUILDKIT=1 docker buildx build --platform=linux/amd64 -t rocksdb-cloud:latest .`

### JNI artifacts inside the image
The Docker build copies JNI outputs into these paths:
- `/usr/lib/librocksdbjni-*.so`
- `/usr/share/java/rocksdbjni-*.jar`

You can list them with:
`docker run --rm rocksdb-cloud:latest ls -1 /usr/lib/librocksdbjni-*.so /usr/share/java/rocksdbjni-*.jar`

### Native dependencies (macOS Homebrew)
- AWS support (S3/Kinesis): `brew install aws-sdk-cpp aws-crt-cpp`
- Kafka/WAL streaming: `brew install librdkafka`
- Other common deps: `brew install gflags`

If you need Kinesis, make sure your AWS SDK is built with Kinesis enabled (e.g., `BUILD_ONLY="s3;kinesis;transfer;core"`) and then build with `USE_AWS=1`. For Kafka WAL, install `librdkafka` and build with `USE_KAFKA=1`.

### Building aws-sdk with kinesis

  # Prereqs
  brew install cmake gflags

  # Configure & build (from repo root)
  mkdir -p build && cd build
  cmake .. \
    -DBUILD_ONLY="s3;kinesis;transfer;core" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="$(pwd)/install"

  cmake --build . --config Release --target install

## License

RocksDB is dual-licensed under both the GPLv2 (found in the COPYING file in the root directory) and Apache 2.0 License (found in the LICENSE.Apache file in the root directory).  You may select, at your option, one of the above-listed licenses.
