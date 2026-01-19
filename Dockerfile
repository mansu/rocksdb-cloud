# ---- Dependencies (toolchain + AWS SDK stack) ----
FROM ubuntu:24.04 AS deps
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential cmake git pkg-config perl wget ca-certificates \
  libbz2-dev zlib1g-dev libzstd-dev liblz4-dev libsnappy-dev libgflags-dev \
  libssl-dev libcurl4-openssl-dev \
  gcc-11 g++-11 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/src

# Build AWS C libraries + CRT + SDK against system OpenSSL/curl.
RUN git clone --depth 1 https://github.com/awslabs/aws-c-common.git && \
    cmake -S aws-c-common -B acc-build \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_DEPS=OFF -DCRYPTO_BACKEND=openssl \
      -DCMAKE_PREFIX_PATH="/usr" \
      -DCMAKE_INSTALL_PREFIX=/opt/aws-sdk && \
    cmake --build acc-build --target install -j"$(nproc)"

RUN git clone --recursive --depth 1 --branch v0.27.5 https://github.com/awslabs/aws-crt-cpp.git && \
    git -C aws-crt-cpp submodule update --init --recursive
RUN cmake -S aws-crt-cpp -B crt-build \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_DEPS=ON -DLEGACY_BUILD=ON \
      -DCRYPTO_BACKEND=openssl -DUSE_OPENSSL=ON -DAWS_LC_USE_OPENSSL=1 \
      -DCMAKE_PREFIX_PATH="/opt/aws-sdk;/usr" \
      -DCMAKE_INSTALL_PREFIX=/opt/aws-sdk && \
      cmake --build crt-build --target install -j"$(nproc)"

RUN git clone --depth 1 --branch 1.11.375 https://github.com/aws/aws-sdk-cpp.git && \
    git -C aws-sdk-cpp submodule update --init --recursive --depth 1
RUN cmake -S aws-sdk-cpp -B sdk-build \
      -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_ONLY="s3;kinesis;transfer;core" \
      -DBUILD_DEPS=ON -DENABLE_TESTING=OFF -DBUILD_TESTING=OFF \
      -DCRYPTO_BACKEND=openssl \
      -DCMAKE_PREFIX_PATH="/opt/aws-sdk;/usr" \
      -DCMAKE_INSTALL_PREFIX=/opt/aws-sdk \
      -DCMAKE_C_FLAGS="-Wno-error -DOPENSSL_SUPPRESS_DEPRECATED=1" \
      -DCMAKE_CXX_FLAGS="-Wno-error -Wno-error=deprecated-declarations -Wno-error=maybe-uninitialized -DOPENSSL_SUPPRESS_DEPRECATED=1" && \
      cmake --build sdk-build --target install -j"$(nproc)"

# ---- Builder (RocksDB build + Java artifacts) ----
FROM ubuntu:24.04 AS builder
ARG DEBIAN_FRONTEND=noninteractive
ARG USE_KAFKA=0
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential cmake git pkg-config ccache perl wget ca-certificates curl \
  libbz2-dev zlib1g-dev libzstd-dev liblz4-dev libsnappy-dev libgflags-dev \
  libssl-dev libcurl4-openssl-dev \
  openjdk-21-jdk gcc-11 g++-11 && \
  if [ "${USE_KAFKA}" = "1" ]; then \
    apt-get install -y --no-install-recommends librdkafka-dev librdkafka1 librdkafka++1; \
  fi && \
  rm -rf /var/lib/apt/lists/*

COPY --from=deps /opt/aws-sdk /opt/aws-sdk

WORKDIR /src/rocksdb-cloud
COPY . .
ENV AWS_SDK=/opt/aws-sdk AWS_CRT=/opt/aws-sdk \
    LD_LIBRARY_PATH=/opt/aws-sdk/lib:/opt/aws-sdk/lib64 \
    TMPDIR=/tmp
# Detect JAVA_HOME dynamically based on architecture (amd64 vs arm64).
RUN JAVA_HOME=$(dirname $(dirname $(readlink -f $(which javac)))) && \
    test -x "$JAVA_HOME/bin/javac" && \
    "$JAVA_HOME/bin/javac" -version && \
    "$JAVA_HOME/bin/java" -version && \
    echo "$JAVA_HOME" > /etc/java_home
ENV CC="ccache /usr/bin/gcc-11" CXX="ccache /usr/bin/g++-11" \
    CCACHE_COMPILERCHECK=content CCACHE_DIR=/tmp/ccache-gcc11 \
    CCACHE_BASEDIR=/src/rocksdb-cloud CCACHE_NOHASHDIR=1
RUN mkdir -p /tmp/ccache-gcc11
RUN make clean && make jclean
RUN JAVA_HOME="$(cat /etc/java_home)" USE_AWS=1 USE_RTTI=1 USE_KAFKA=${USE_KAFKA} \
    make -j"$(nproc)" rocksdbjava && \
    cd java && JAVA_HOME="$(cat /etc/java_home)" make sample

# ---- Runtime (minimal image with JNI + deps) ----
FROM ubuntu:24.04
ARG USE_KAFKA=0
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
  openjdk-21-jre-headless libzstd1 liblz4-1 libsnappy1v5 libbz2-1.0 \
  libssl3 libcurl4 && \
  if [ "${USE_KAFKA}" = "1" ]; then \
    apt-get install -y --no-install-recommends librdkafka1 librdkafka++1; \
  fi && \
  rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/aws-sdk /opt/aws-sdk
COPY --from=builder /src/rocksdb-cloud/java/target/librocksdbjni-*.so /usr/lib/
COPY --from=builder /src/rocksdb-cloud/java/target/rocksdbjni-*.jar /usr/share/java/

ENV LD_LIBRARY_PATH=/opt/aws-sdk/lib:/opt/aws-sdk/lib64
ENV TMPDIR=/tmp
CMD ["/bin/bash"]
