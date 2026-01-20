ROCKSDB_DIR ?= ..
OUT_DIR ?= $(CURDIR)/build
CXX ?= c++
USE_AWS ?= 1
AWS_SDK_INCLUDE ?=
AWS_SDK_LIB_DIR ?=
AWS_SDK ?=
AWS_SDK_LIBS ?= -laws-cpp-sdk-core -laws-crt-cpp -laws-c-auth -laws-c-cal -laws-c-common -laws-c-compression -laws-c-event-stream -laws-c-http -laws-c-io -laws-c-mqtt -laws-c-s3 -laws-c-sdkutils -laws-checksums -lcurl -lssl -lcrypto -lz

ifneq ($(AWS_SDK),)
  AWS_SDK_INCLUDE ?= $(AWS_SDK)/include
  AWS_SDK_LIB_DIR ?= $(AWS_SDK)/lib
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  SHLIB_EXT = dylib
  SHLIB_FLAGS = -dynamiclib -Wl,-install_name,@rpath/librocksdb_cloud_ffi.$(SHLIB_EXT)
  AWS_SDK_FRAMEWORKS = -framework CoreFoundation -framework Security -framework SystemConfiguration -framework Network
else
  SHLIB_EXT = so
  SHLIB_FLAGS = -shared -Wl,-soname,librocksdb_cloud_ffi.$(SHLIB_EXT)
endif

CXXFLAGS ?= -std=c++17 -O2 -fPIC
CXXFLAGS += -I$(ROCKSDB_DIR)/include -I$(ROCKSDB_DIR)
ifeq ($(USE_AWS),1)
  CXXFLAGS += -DUSE_AWS
endif
ifneq ($(AWS_SDK_INCLUDE),)
  CXXFLAGS += -I$(AWS_SDK_INCLUDE)
endif

LDFLAGS ?=
LDLIBS ?= -L$(ROCKSDB_DIR) -lrocksdb
ifneq ($(AWS_SDK_LIB_DIR),)
  LDFLAGS += -L$(AWS_SDK_LIB_DIR)
endif
ifeq ($(USE_AWS),1)
  LDLIBS += $(AWS_SDK_LIBS) $(AWS_SDK_FRAMEWORKS)
endif

SRC = src/rocksdb_cloud_ffi.cc
OBJ = $(OUT_DIR)/rocksdb_cloud_ffi.o
LIB = $(OUT_DIR)/librocksdb_cloud_ffi.$(SHLIB_EXT)

all: $(LIB)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(OBJ): $(SRC) | $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -Iinclude -c $(SRC) -o $(OBJ)

$(LIB): $(OBJ)
	$(CXX) $(SHLIB_FLAGS) -o $(LIB) $(OBJ) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean build-aws

build-aws:
	$(MAKE) AWS_SDK_INCLUDE=/Users/suman/Projects/aws-sdk-cpp/build-legacy/install/include \
		AWS_SDK_LIB_DIR=/Users/suman/Projects/aws-sdk-cpp/build-legacy/install/lib \
		LDFLAGS='-L/opt/homebrew/opt/openssl@3/lib -L/Users/suman/Projects/aws-sdk-cpp/build-legacy/install/lib'
