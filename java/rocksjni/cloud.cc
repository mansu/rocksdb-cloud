// Copyright (c) 2024-present, Rockset, Inc.
// Licensed under the Apache License 2.0 and GPLv2; you may not use this file
// except in compliance with one of these Licenses.
//
// JNI bindings for RocksDB cloud APIs.

#include "rocksdb/cloud/db_cloud.h"
#include "rocksdb/cloud/cloud_file_system.h"
#include "rocksdb/env.h"
#include "rocksdb/options.h"

#include <jni.h>

#include <string>
#include <memory>
#include <vector>
#include <mutex>

#ifdef USE_AWS
#include <aws/core/Aws.h>
#endif

#include "include/org_rocksdb_CloudEnv.h"
#include "include/org_rocksdb_DBCloud.h"
#include "portal.h"
#include "rocksjni/cplusplus_to_java_convert.h"

namespace {

jobjectArray ToJavaByteArrayArray(JNIEnv* env,
                                  const std::vector<std::string>& elements) {
  jclass byte_array_class = env->FindClass("[B");
  if (byte_array_class == nullptr) {
    return nullptr;  // exception thrown
  }
  jobjectArray outer = env->NewObjectArray(
      static_cast<jsize>(elements.size()), byte_array_class, nullptr);
  if (outer == nullptr) {
    return nullptr;  // exception thrown
  }
  for (jsize i = 0; i < static_cast<jsize>(elements.size()); ++i) {
    const auto& s = elements[static_cast<size_t>(i)];
    jbyteArray inner = env->NewByteArray(static_cast<jsize>(s.size()));
    if (inner == nullptr) {
      return nullptr;  // exception thrown
    }
    env->SetByteArrayRegion(inner, 0, static_cast<jsize>(s.size()),
                            reinterpret_cast<const jbyte*>(s.data()));
    env->SetObjectArrayElement(outer, i, inner);
    env->DeleteLocalRef(inner);
  }
  return outer;
}

}  // namespace

namespace ROCKSDB_NAMESPACE {
namespace {

Status GetPersistentCacheArgs(JNIEnv* env, jstring jcache_path,
                              std::string* cache_path_out) {
  cache_path_out->clear();
  if (jcache_path == nullptr) {
    return Status::OK();
  }
  const char* cache_path = env->GetStringUTFChars(jcache_path, nullptr);
  if (cache_path == nullptr) {
    return Status::OK();  // OOM will be thrown by JVM
  }
  cache_path_out->assign(cache_path);
  env->ReleaseStringUTFChars(jcache_path, cache_path);
  return Status::OK();
}

}  // namespace
}  // namespace ROCKSDB_NAMESPACE

/*
 * Class:     org_rocksdb_CloudEnv
 * Method:    createFromString
 * Signature: (JLjava/lang/String;Z)J
 */
jlong Java_org_rocksdb_CloudEnv_createFromString(
    JNIEnv* env, jclass, jlong jbase_env, jstring jconfig,
    jboolean jinvoke_prepare) {
#ifdef USE_AWS
  static std::once_flag aws_sdk_init_once;
  std::call_once(aws_sdk_init_once, [] {
    Aws::InitAPI(Aws::SDKOptions());
  });
#endif
  auto* base_env = reinterpret_cast<ROCKSDB_NAMESPACE::Env*>(jbase_env);
  if (base_env == nullptr) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(
        env, ROCKSDB_NAMESPACE::Status::InvalidArgument(
                 "Base Env handle was null"));
    return 0;
  }

  const char* config = env->GetStringUTFChars(jconfig, nullptr);
  if (config == nullptr) {
    // OutOfMemoryError already pending
    return 0;
  }

  ROCKSDB_NAMESPACE::ConfigOptions cfg_opts;
  cfg_opts.env = base_env;
  cfg_opts.invoke_prepare_options = (jinvoke_prepare == JNI_TRUE);

  std::unique_ptr<ROCKSDB_NAMESPACE::CloudFileSystem> fs;
  ROCKSDB_NAMESPACE::Status s = ROCKSDB_NAMESPACE::CloudFileSystemEnv::
      CreateFromString(cfg_opts, config, &fs);

  env->ReleaseStringUTFChars(jconfig, config);

  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
    return 0;
  }

  std::shared_ptr<ROCKSDB_NAMESPACE::FileSystem> fs_shared(fs.release());
  auto env_uptr =
      ROCKSDB_NAMESPACE::CloudFileSystemEnv::NewCompositeEnv(base_env,
                                                             fs_shared);
  return reinterpret_cast<jlong>(env_uptr.release());
}

/*
 * Class:     org_rocksdb_DBCloud
 * Method:    openInternal
 * Signature: (JLjava/lang/String;Ljava/lang/String;JZ)J
 */
jlong Java_org_rocksdb_DBCloud_openInternal(
    JNIEnv* env, jclass, jlong jopt_handle, jstring jdb_path,
    jstring jcache_path, jlong jpersistent_cache_size_gb,
    jboolean jread_only) {
  auto* options =
      reinterpret_cast<ROCKSDB_NAMESPACE::Options*>(jopt_handle);
  const char* db_path = env->GetStringUTFChars(jdb_path, nullptr);
  if (db_path == nullptr) {
    // OutOfMemoryError already pending
    return 0;
  }

  std::string cache_path;
  ROCKSDB_NAMESPACE::Status st =
      ROCKSDB_NAMESPACE::GetPersistentCacheArgs(env, jcache_path, &cache_path);
  if (!st.ok()) {
    env->ReleaseStringUTFChars(jdb_path, db_path);
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, st);
    return 0;
  }

  ROCKSDB_NAMESPACE::DBCloud* db = nullptr;
  st = ROCKSDB_NAMESPACE::DBCloud::Open(
      *options, db_path, cache_path,
      static_cast<uint64_t>(jpersistent_cache_size_gb), &db,
      jread_only == JNI_TRUE);

  env->ReleaseStringUTFChars(jdb_path, db_path);

  if (!st.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, st);
    return 0;
  }
  return reinterpret_cast<jlong>(db);
}

/*
 * Class:     org_rocksdb_DBCloud
 * Method:    openColumnFamilies
 * Signature: (JLjava/lang/String;[[B[JLjava/lang/String;JZ)[J
 */
jlongArray Java_org_rocksdb_DBCloud_openColumnFamilies(
    JNIEnv* env, jclass, jlong jopt_handle, jstring jdb_path,
    jobjectArray jcolumn_names, jlongArray jcolumn_options,
    jstring jcache_path, jlong jpersistent_cache_size_gb,
    jboolean jread_only) {
  const char* db_path = env->GetStringUTFChars(jdb_path, nullptr);
  if (db_path == nullptr) {
    return nullptr;  // OOM thrown
  }

  jlong* jco = env->GetLongArrayElements(jcolumn_options, nullptr);
  if (jco == nullptr) {
    env->ReleaseStringUTFChars(jdb_path, db_path);
    return nullptr;  // OOM thrown
  }

  std::vector<ROCKSDB_NAMESPACE::ColumnFamilyDescriptor> column_families;
  jboolean has_exception = JNI_FALSE;
  ROCKSDB_NAMESPACE::JniUtil::byteStrings<std::string>(
      env, jcolumn_names,
      [](const char* str_data, const size_t str_len) {
        return std::string(str_data, str_len);
      },
      [&jco, &column_families](size_t idx, std::string cf_name) {
        auto* cf_options =
            reinterpret_cast<ROCKSDB_NAMESPACE::ColumnFamilyOptions*>(
                jco[idx]);
        column_families.emplace_back(cf_name, *cf_options);
      },
      &has_exception);

  env->ReleaseLongArrayElements(jcolumn_options, jco, JNI_ABORT);

  if (has_exception == JNI_TRUE) {
    env->ReleaseStringUTFChars(jdb_path, db_path);
    return nullptr;
  }

  std::string cache_path;
  ROCKSDB_NAMESPACE::Status st =
      ROCKSDB_NAMESPACE::GetPersistentCacheArgs(env, jcache_path, &cache_path);
  if (!st.ok()) {
    env->ReleaseStringUTFChars(jdb_path, db_path);
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, st);
    return nullptr;
  }

  auto* db_opt =
      reinterpret_cast<ROCKSDB_NAMESPACE::Options*>(jopt_handle);
  std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*> handles;
  ROCKSDB_NAMESPACE::DBCloud* db = nullptr;
  st = ROCKSDB_NAMESPACE::DBCloud::Open(
      *db_opt, db_path, column_families, cache_path,
      static_cast<uint64_t>(jpersistent_cache_size_gb), &handles, &db,
      jread_only == JNI_TRUE);

  env->ReleaseStringUTFChars(jdb_path, db_path);

  if (!st.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, st);
    return nullptr;
  }

  const size_t handles_len = handles.size();
  jlongArray jhandle_values = env->NewLongArray(static_cast<jsize>(handles_len + 1));
  if (jhandle_values == nullptr) {
    delete db;
    for (auto* h : handles) {
      delete h;
    }
    // OOM already pending
    return nullptr;
  }

  std::vector<jlong> jhandles(handles_len + 1);
  jhandles[0] = reinterpret_cast<jlong>(db);
  for (size_t i = 0; i < handles_len; i++) {
    jhandles[i + 1] = reinterpret_cast<jlong>(handles[i]);
  }
  env->SetLongArrayRegion(jhandle_values, 0,
                          static_cast<jsize>(jhandles.size()),
                          jhandles.data());
  return jhandle_values;
}

/*
 * Class:     org_rocksdb_DBCloud
 * Method:    savepoint
 * Signature: (J)V
 */
void Java_org_rocksdb_DBCloud_savepoint(JNIEnv* env, jclass, jlong jdb_handle) {
  auto* db = reinterpret_cast<ROCKSDB_NAMESPACE::DBCloud*>(jdb_handle);
  auto s = db->Savepoint();
  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_DBCloud
 * Method:    resync
 * Signature: (J)V
 */
void Java_org_rocksdb_DBCloud_resync(JNIEnv* env, jclass, jlong jdb_handle) {
  auto* db = reinterpret_cast<ROCKSDB_NAMESPACE::DBCloud*>(jdb_handle);
  auto s = db->Resync();
  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_DBCloud
 * Method:    listColumnFamilies
 * Signature: (JLjava/lang/String;)[[B
 */
jobjectArray Java_org_rocksdb_DBCloud_listColumnFamilies(
    JNIEnv* env, jclass, jlong jdb_options_handle, jstring jname) {
  auto* db_options =
      reinterpret_cast<ROCKSDB_NAMESPACE::DBOptions*>(jdb_options_handle);

  const char* name = env->GetStringUTFChars(jname, nullptr);
  if (name == nullptr) {
    return nullptr;  // OOM already pending
  }

  std::vector<std::string> column_families;
  auto status =
      ROCKSDB_NAMESPACE::DBCloud::ListColumnFamilies(*db_options, name,
                                                     &column_families);
  env->ReleaseStringUTFChars(jname, name);

  if (!status.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, status);
    return nullptr;
  }

  return ToJavaByteArrayArray(env, column_families);
}
