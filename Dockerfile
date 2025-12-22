# ---- Builder ----
FROM ubuntu:22.04 AS builder
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential cmake git pkg-config ccache perl wget ca-certificates \
  libbz2-dev zlib1g-dev libzstd-dev liblz4-dev libsnappy-dev libgflags-dev \
  openjdk-21-jdk-headless gcc-11 g++-11 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/src

# Pin curl and aws libs to use openssl 1.1 so there are no conflicts
RUN wget -q https://www.openssl.org/source/openssl-1.1.1w.tar.gz && \
    tar xzf openssl-1.1.1w.tar.gz && \
    cd openssl-1.1.1w && \
    ./config --prefix=/opt/openssl11 --openssldir=/opt/openssl11 shared && \
    make -j"$(nproc)" && \
    make install_sw && \
    cd /opt/src && rm -rf openssl-1.1.1w openssl-1.1.1w.tar.gz

RUN wget -q https://curl.se/download/curl-8.11.1.tar.gz && \
    tar xzf curl-8.11.1.tar.gz && \
    cd curl-8.11.1 && \
    LDFLAGS="-Wl,-rpath,/opt/openssl11/lib" ./configure --with-ssl=/opt/openssl11 --prefix=/opt/curl11 --with-ca-path=/etc/ssl/certs --without-libpsl && \
    make -j"$(nproc)" && \
    make install && \
    cd /opt/src && rm -rf curl-8.11.1 curl-8.11.1.tar.gz

ENV PATH="/opt/curl11/bin:${PATH}"
ENV PKG_CONFIG_PATH="/opt/curl11/lib/pkgconfig:/opt/openssl11/lib/pkgconfig"
ENV LIBRARY_PATH="/opt/curl11/lib:/opt/openssl11/lib"
ENV LDFLAGS="-L/opt/curl11/lib -Wl,-rpath,/opt/curl11/lib -L/opt/openssl11/lib -Wl,-rpath,/opt/openssl11/lib"
ENV CPPFLAGS="-I/opt/curl11/include -I/opt/openssl11/include"
RUN echo "/opt/openssl11/lib" > /etc/ld.so.conf.d/openssl11.conf && \
    echo "/opt/curl11/lib" > /etc/ld.so.conf.d/curl11.conf && \
    ldconfig

RUN git clone --depth 1 https://github.com/awslabs/aws-c-common.git && \
    cmake -S aws-c-common -B acc-build \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_DEPS=OFF -DCRYPTO_BACKEND=openssl \
      -DOPENSSL_ROOT_DIR=/opt/openssl11 -DCMAKE_PREFIX_PATH="/opt/openssl11;/opt/curl11;/usr" \
      -DCMAKE_INSTALL_PREFIX=/opt/aws-sdk && \
    cmake --build acc-build --target install -j"$(nproc)"

RUN git clone --recursive --depth 1 --branch v0.27.5 https://github.com/awslabs/aws-crt-cpp.git && \
    git -C aws-crt-cpp submodule update --init --recursive
  RUN cmake -S aws-crt-cpp -B crt-build \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_DEPS=ON -DLEGACY_BUILD=ON \
      -DCRYPTO_BACKEND=openssl -DUSE_OPENSSL=ON -DAWS_LC_USE_OPENSSL=1 \
      -DOPENSSL_ROOT_DIR=/opt/openssl11 -DCMAKE_PREFIX_PATH="/opt/aws-sdk;/opt/openssl11;/opt/curl11;/usr" \
      -DCMAKE_INSTALL_PREFIX=/opt/aws-sdk && \
      cmake --build crt-build --target install -j"$(nproc)"

RUN git clone --depth 1 --branch 1.11.375 https://github.com/aws/aws-sdk-cpp.git && \
    git -C aws-sdk-cpp submodule update --init --recursive --depth 1
  RUN cmake -S aws-sdk-cpp -B sdk-build \
      -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_ONLY="s3;kinesis;transfer;core" \
      -DBUILD_DEPS=ON -DENABLE_TESTING=OFF -DBUILD_TESTING=OFF \
      -DCRYPTO_BACKEND=openssl \
      -DOPENSSL_ROOT_DIR=/opt/openssl11 \
      -DCMAKE_PREFIX_PATH="/opt/aws-sdk;/opt/openssl11;/opt/curl11" \
      -DCMAKE_INSTALL_PREFIX=/opt/aws-sdk \
      -DCMAKE_C_FLAGS="-Wno-error -DOPENSSL_SUPPRESS_DEPRECATED=1" \
      -DCMAKE_CXX_FLAGS="-Wno-error -Wno-error=deprecated-declarations -Wno-error=maybe-uninitialized -DOPENSSL_SUPPRESS_DEPRECATED=1" && \
      cmake --build sdk-build --target install -j"$(nproc)"

RUN rm -f /opt/aws-sdk/lib/libcrypto.so* /opt/aws-sdk/lib/libssl.so* /opt/aws-sdk/lib64/libcrypto.so* /opt/aws-sdk/lib64/libssl.so*

WORKDIR /src/rocksdb-cloud
COPY . .
ENV AWS_SDK=/opt/aws-sdk AWS_CRT=/opt/aws-sdk \
    LD_LIBRARY_PATH=/opt/openssl11/lib:/opt/aws-sdk/lib:/opt/aws-sdk/lib64 \
    TMPDIR=/tmp \
    JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
ENV CC="ccache /usr/bin/gcc-11" CXX="ccache /usr/bin/g++-11" CCACHE_COMPILERCHECK=content CCACHE_DIR=/root/.cache/ccache-gcc11
RUN make clean && make jclean
RUN USE_AWS=1 USE_RTTI=1 make -j"$(nproc)" rocksdbjava && \
    cd java && JAVA_HOME=$JAVA_HOME make sample

# ---- Runtime ----
FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
  openjdk-21-jre-headless libzstd1 liblz4-1 libsnappy1v5 libbz2-1.0 && \
  rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/openssl11 /opt/openssl11
COPY --from=builder /opt/curl11 /opt/curl11
COPY --from=builder /opt/aws-sdk /opt/aws-sdk
COPY --from=builder /src/rocksdb-cloud/java/target/librocksdbjni-linux64.so /usr/lib/librocksdbjni.so
COPY --from=builder /src/rocksdb-cloud/java/target/rocksdbjni-*-linux64.jar /usr/share/java/rocksdbjni.jar
COPY --from=builder /src/rocksdb-cloud/java/target/rocksdbjni-*-linux64.jar /usr/share/java/

ENV LD_LIBRARY_PATH=/opt/openssl11/lib:/opt/curl11/lib:/opt/aws-sdk/lib:/opt/aws-sdk/lib64
ENV TMPDIR=/tmp
CMD ["/bin/bash"]
