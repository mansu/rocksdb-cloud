// Copyright (c) 2024-present, Rockset, Inc.  All rights reserved.
#pragma once

#ifndef ROCKSDB_LITE
#include <atomic>
#include <memory>
#include <thread>

#include "cloud/file_lifecycle_logger.h"
#include "rocksdb/db.h"

namespace ROCKSDB_NAMESPACE {

class CloudFileSystemImpl;

class FileLifecycleTracker {
 public:
  FileLifecycleTracker(DB* db, CloudFileSystemImpl* cfs,
                       const std::shared_ptr<FileLifecycleLogger>& logger,
                       Env* env, uint64_t snapshot_period_sec);
  ~FileLifecycleTracker();

  void Start();
  void Stop();

 private:
  void Run();
  void LogSnapshot();

  DB* db_;
  CloudFileSystemImpl* cfs_;
  std::shared_ptr<FileLifecycleLogger> logger_;
  Env* env_;
  uint64_t snapshot_period_sec_;
  std::atomic<bool> stop_;
  std::thread thread_;
  std::atomic<uint64_t> snapshot_id_;
};

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
