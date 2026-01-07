// Copyright (c) 2017 Rockset

#pragma once

#ifndef ROCKSDB_LITE
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "cloud/file_lifecycle_logger.h"
#include "cloud/file_lifecycle_tracker.h"
#include "rocksdb/cloud/db_cloud.h"
#include "rocksdb/db.h"

namespace ROCKSDB_NAMESPACE {

class Env;

//
// All writes to this DB can be configured to be persisted
// in cloud storage.
//
class DBCloudImpl : public DBCloud {
  friend DBCloud;

 public:
  virtual ~DBCloudImpl();
  Status Savepoint() override;

  Status CheckpointToCloud(const BucketOptions& destination,
                           const CheckpointToCloudOptions& options) override;

 protected:
  // The CloudFileSystem used by this open instance.
  CloudFileSystem* cfs_;

 private:
  Status DoCheckpointToCloud(const BucketOptions& destination,
                             const CheckpointToCloudOptions& options);

  // Maximum manifest file size
  static const uint64_t max_manifest_file_size = 4 * 1024L * 1024L;

  DBCloudImpl(DB* db, std::unique_ptr<Env> local_env, CloudFileSystem* cfs,
              std::shared_ptr<FileLifecycleLogger> lifecycle_logger,
              std::unique_ptr<FileLifecycleTracker> lifecycle_tracker);

  std::unique_ptr<Env> local_env_;
  std::shared_ptr<FileLifecycleLogger> lifecycle_logger_;
  std::unique_ptr<FileLifecycleTracker> lifecycle_tracker_;
};
}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
