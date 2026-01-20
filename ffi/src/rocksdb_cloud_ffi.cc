#include "rocksdb_cloud_ffi.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "rocksdb/cloud/cloud_file_system.h"
#include "rocksdb/cloud/db_cloud.h"
#include "rocksdb/env.h"
#include "rocksdb/options.h"
#include "rocksdb/convenience.h"
#include "rocksdb/configurable.h"

#ifdef USE_AWS
#include <aws/core/Aws.h>
#endif

namespace {
using ROCKSDB_NAMESPACE::BucketOptions;
using ROCKSDB_NAMESPACE::CloudFileSystem;
using ROCKSDB_NAMESPACE::CloudFileSystemEnv;
using ROCKSDB_NAMESPACE::CloudFileSystemOptions;
using ROCKSDB_NAMESPACE::ConfigOptions;
using ROCKSDB_NAMESPACE::DBCloud;
using ROCKSDB_NAMESPACE::Env;
using ROCKSDB_NAMESPACE::FileSystem;
using ROCKSDB_NAMESPACE::Iterator;
using ROCKSDB_NAMESPACE::Options;
using ROCKSDB_NAMESPACE::ReadOptions;
using ROCKSDB_NAMESPACE::Slice;
using ROCKSDB_NAMESPACE::Status;

struct DbHandle {
  std::unique_ptr<DBCloud> db;
  std::unique_ptr<CloudFileSystem> cfs;
  std::unique_ptr<Env> env;
};

struct IterHandle {
  std::unique_ptr<Iterator> iter;
};

void ClearError(char** err_out) {
  if (err_out != nullptr) {
    *err_out = nullptr;
  }
}

void SetError(char** err_out, const std::string& message) {
  if (err_out == nullptr) {
    return;
  }
  char* copy = static_cast<char*>(std::malloc(message.size() + 1));
  if (copy == nullptr) {
    *err_out = nullptr;
    return;
  }
  std::memcpy(copy, message.data(), message.size());
  copy[message.size()] = '\0';
  *err_out = copy;
}

std::string ToString(const char* value) {
  return value == nullptr ? std::string() : std::string(value);
}

std::string BuildCloudId(const rdbc_cloud_config* config) {
  std::string id = ToString(config->filesystem_id);
  if (id.empty()) {
    id = "cloud";
  }
  bool has_id = id.find("id=") != std::string::npos;
  bool has_provider = id.find("provider=") != std::string::npos;
  if (!has_id) {
    id = std::string("id=") + id;
  }
  if (!has_provider) {
    std::string provider = ToString(config->storage_provider);
    if (!provider.empty()) {
      id.append(";provider=");
      id.append(provider);
    }
  }
  return id;
}

bool UseAwsFilesystem(const std::string& fs_id) {
  if (fs_id.empty()) {
    return false;
  }
  if (fs_id == "aws") {
    return true;
  }
  if (fs_id == "id=aws") {
    return true;
  }
  if (fs_id.rfind("aws;", 0) == 0) {
    return true;
  }
  if (fs_id.find("id=aws") != std::string::npos) {
    return true;
  }
  return false;
}

bool ConfigureBuckets(const rdbc_cloud_config* config,
                      CloudFileSystemOptions* cloud_options,
                      std::string* err) {
  std::string bucket = ToString(config->bucket);
  std::string object_path = ToString(config->object_path);
  std::string region = ToString(config->region);
  if (bucket.empty()) {
    if (err != nullptr) {
      *err = "bucket name is required";
    }
    return false;
  }
  if (object_path.empty()) {
    if (err != nullptr) {
      *err = "object path is required";
    }
    return false;
  }
  if (region.empty()) {
    if (err != nullptr) {
      *err = "region is required";
    }
    return false;
  }

  cloud_options->src_bucket.SetBucketPrefix("");
  cloud_options->dest_bucket.SetBucketPrefix("");
  cloud_options->src_bucket.SetBucketName(bucket);
  cloud_options->dest_bucket.SetBucketName(bucket);
  cloud_options->src_bucket.SetObjectPath(object_path);
  cloud_options->dest_bucket.SetObjectPath(object_path);
  cloud_options->src_bucket.SetRegion(region);
  cloud_options->dest_bucket.SetRegion(region);
  return true;
}

#ifdef USE_AWS
void InitAwsIfNeeded() {
  static std::once_flag init_once;
  static Aws::SDKOptions options;
  std::call_once(init_once, []() {
    Aws::InitAPI(options);
    std::atexit([]() { Aws::ShutdownAPI(options); });
  });
}
#endif

}  // namespace

struct rdbc_dbcloud {
  DbHandle handle;
};

struct rdbc_iter {
  IterHandle handle;
};

rdbc_dbcloud* rdbc_open_readonly(const char* db_path,
                                 const rdbc_cloud_config* config,
                                 char** err_out) {
  ClearError(err_out);
  if (db_path == nullptr || config == nullptr) {
    SetError(err_out, "missing db_path or config");
    return nullptr;
  }

#ifdef USE_AWS
  InitAwsIfNeeded();
#endif

  CloudFileSystemOptions cloud_options;
  cloud_options.keep_local_sst_files = config->keep_local_sst_files != 0;
  cloud_options.keep_local_log_files = config->keep_local_log_files != 0;
  cloud_options.create_bucket_if_missing =
      config->create_bucket_if_missing != 0;
  cloud_options.resync_on_open = config->resync_on_open != 0;
  cloud_options.use_aws_transfer_manager =
      config->use_aws_transfer_manager != 0;
  cloud_options.roll_cloud_manifest_on_open = config->is_writer != 0;

  std::string cookie = ToString(config->cookie);
  cloud_options.cookie_on_open = cookie;
  if (config->is_writer != 0) {
    cloud_options.new_cookie_on_open = cookie;
  }

  std::string endpoint_override = ToString(config->endpoint_override);
  if (!endpoint_override.empty()) {
    cloud_options.endpoint_override = endpoint_override;
  }
  cloud_options.use_path_style = config->use_path_style != 0;

  std::string access_key = ToString(config->access_key);
  std::string secret_key = ToString(config->secret_key);
  if (access_key.empty() || secret_key.empty()) {
    SetError(err_out, "missing access key or secret key");
    return nullptr;
  }
  cloud_options.credentials.InitializeSimple(access_key, secret_key);

  std::string err;
  if (!ConfigureBuckets(config, &cloud_options, &err)) {
    SetError(err_out, err);
    return nullptr;
  }

  std::string id = BuildCloudId(config);
  ConfigOptions config_options;
  config_options.env = Env::Default();
  config_options.invoke_prepare_options = true;

  std::unique_ptr<CloudFileSystem> cfs;
  Status st;
  std::string fs_id = ToString(config->filesystem_id);
  if (UseAwsFilesystem(fs_id)) {
    CloudFileSystem* raw = nullptr;
    st = CloudFileSystemEnv::NewAwsFileSystem(
        FileSystem::Default(), cloud_options, nullptr, &raw);
    if (st.ok()) {
      cfs.reset(raw);
    }
  } else {
    st = CloudFileSystemEnv::CreateFromString(
        config_options, id, cloud_options, &cfs);
  }
  if (!st.ok()) {
    SetError(err_out, st.ToString());
    return nullptr;
  }

  std::unique_ptr<Env> env =
      CloudFileSystemEnv::NewCompositeEnvFromFs(cfs.get(), Env::Default());

  Options options;
  options.env = env.get();
  options.create_if_missing = false;

  DBCloud* db = nullptr;
  st = DBCloud::Open(options, db_path, "", 0, &db, true);
  if (!st.ok()) {
    SetError(err_out, st.ToString());
    return nullptr;
  }

  auto* handle = new rdbc_dbcloud();
  handle->handle.db.reset(db);
  handle->handle.cfs = std::move(cfs);
  handle->handle.env = std::move(env);
  return handle;
}

void rdbc_close(rdbc_dbcloud* db) {
  if (db == nullptr) {
    return;
  }
  delete db;
}

int rdbc_get(rdbc_dbcloud* db,
             const unsigned char* key,
             size_t key_len,
             unsigned char** val_out,
             size_t* val_len_out,
             char** err_out) {
  ClearError(err_out);
  if (db == nullptr || key == nullptr || val_out == nullptr ||
      val_len_out == nullptr) {
    SetError(err_out, "invalid arguments");
    return 1;
  }

  std::string value;
  Status st = db->handle.db->Get(ReadOptions(),
                                 Slice(reinterpret_cast<const char*>(key),
                                       key_len),
                                 &value);
  if (st.IsNotFound()) {
    *val_out = nullptr;
    *val_len_out = 0;
    return 0;
  }
  if (!st.ok()) {
    SetError(err_out, st.ToString());
    return 1;
  }

  size_t alloc_len = value.empty() ? 1 : value.size();
  auto* buffer = static_cast<unsigned char*>(std::malloc(alloc_len));
  if (buffer == nullptr) {
    SetError(err_out, "allocation failed");
    return 1;
  }
  if (!value.empty()) {
    std::memcpy(buffer, value.data(), value.size());
  }
  *val_out = buffer;
  *val_len_out = value.size();
  return 0;
}

void rdbc_free_bytes(unsigned char* bytes) {
  std::free(bytes);
}

int rdbc_get_property(rdbc_dbcloud* db,
                      const char* name,
                      char** val_out,
                      char** err_out) {
  ClearError(err_out);
  if (db == nullptr || name == nullptr || val_out == nullptr) {
    SetError(err_out, "invalid arguments");
    return 1;
  }

  std::string value;
  if (!db->handle.db->GetProperty(name, &value)) {
    value.clear();
  }
  char* copy = static_cast<char*>(std::malloc(value.size() + 1));
  if (copy == nullptr) {
    SetError(err_out, "allocation failed");
    return 1;
  }
  std::memcpy(copy, value.data(), value.size());
  copy[value.size()] = '\0';
  *val_out = copy;
  return 0;
}

void rdbc_free_str(char* value) {
  std::free(value);
}

rdbc_iter* rdbc_iter_new(rdbc_dbcloud* db) {
  if (db == nullptr) {
    return nullptr;
  }
  std::unique_ptr<Iterator> iter(db->handle.db->NewIterator(ReadOptions()));
  if (!iter) {
    return nullptr;
  }
  auto* handle = new rdbc_iter();
  handle->handle.iter = std::move(iter);
  return handle;
}

void rdbc_iter_seek_to_first(rdbc_iter* it) {
  if (it == nullptr) {
    return;
  }
  it->handle.iter->SeekToFirst();
}

void rdbc_iter_seek(rdbc_iter* it, const unsigned char* key, size_t key_len) {
  if (it == nullptr || key == nullptr) {
    return;
  }
  it->handle.iter->Seek(
      Slice(reinterpret_cast<const char*>(key), key_len));
}

int rdbc_iter_valid(rdbc_iter* it) {
  if (it == nullptr) {
    return 0;
  }
  return it->handle.iter->Valid() ? 1 : 0;
}

int rdbc_iter_key(rdbc_iter* it, unsigned char** key_out, size_t* key_len_out) {
  if (it == nullptr || key_out == nullptr || key_len_out == nullptr) {
    return 1;
  }
  if (!it->handle.iter->Valid()) {
    return 1;
  }
  Slice key = it->handle.iter->key();
  size_t alloc_len = key.size() == 0 ? 1 : key.size();
  auto* buffer = static_cast<unsigned char*>(std::malloc(alloc_len));
  if (buffer == nullptr) {
    return 1;
  }
  if (key.size() > 0) {
    std::memcpy(buffer, key.data(), key.size());
  }
  *key_out = buffer;
  *key_len_out = key.size();
  return 0;
}

void rdbc_iter_next(rdbc_iter* it) {
  if (it == nullptr) {
    return;
  }
  it->handle.iter->Next();
}

void rdbc_iter_destroy(rdbc_iter* it) {
  if (it == nullptr) {
    return;
  }
  delete it;
}

int rdbc_list_live_sst_files(rdbc_dbcloud* db,
                             char*** files_out,
                             size_t* count_out,
                             char** err_out) {
  ClearError(err_out);
  if (db == nullptr || files_out == nullptr || count_out == nullptr) {
    SetError(err_out, "invalid arguments");
    return 1;
  }

  std::vector<ROCKSDB_NAMESPACE::LiveFileMetaData> metadata;
  db->handle.db->GetLiveFilesMetaData(&metadata);

  *count_out = metadata.size();
  if (metadata.empty()) {
    *files_out = nullptr;
    return 0;
  }

  auto** list = static_cast<char**>(
      std::calloc(metadata.size(), sizeof(char*)));
  if (list == nullptr) {
    SetError(err_out, "allocation failed");
    return 1;
  }

  for (size_t i = 0; i < metadata.size(); ++i) {
    const auto& meta = metadata[i];
    const std::string& name =
        !meta.relative_filename.empty() ? meta.relative_filename : meta.name;
    char* entry = static_cast<char*>(std::malloc(name.size() + 1));
    if (entry == nullptr) {
      for (size_t j = 0; j < i; ++j) {
        std::free(list[j]);
      }
      std::free(list);
      SetError(err_out, "allocation failed");
      return 1;
    }
    std::memcpy(entry, name.data(), name.size());
    entry[name.size()] = '\0';
    list[i] = entry;
  }

  *files_out = list;
  return 0;
}

void rdbc_free_str_list(char** list, size_t count) {
  if (list == nullptr) {
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    std::free(list[i]);
  }
  std::free(list);
}
