// Copyright (c) 2024-present, Rockset, Inc.  All rights reserved.
#pragma once

#ifndef ROCKSDB_LITE
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "rocksdb/env.h"
#include "rocksdb/file_system.h"

namespace ROCKSDB_NAMESPACE {

class FileLifecycleLogger {
 public:
  class JsonWriter {
   public:
    JsonWriter();
    void AddString(const char* key, const std::string& value);
    void AddUint64(const char* key, uint64_t value);
    void AddInt64(const char* key, int64_t value);
    void AddBool(const char* key, bool value);
    void AddDouble(const char* key, double value);
    void AddStringList(const char* key,
                       const std::vector<std::string>& values);
    void AddUint64List(const char* key,
                       const std::vector<uint64_t>& values);
    std::string Finish();

   private:
    void AddKey(const char* key);
    std::string out_;
    bool first_;
  };

  static std::shared_ptr<FileLifecycleLogger> Create(
      const std::shared_ptr<FileSystem>& fs, Env* env,
      const std::string& path, const std::string& db_name,
      const std::string& role, const std::string& db_id, bool verbose);

  void LogEvent(const std::string& event,
                const std::function<void(JsonWriter*)>& add_fields);
  void SetDbId(const std::string& db_id);
  const std::string& path() const { return path_; }
  bool enabled() const { return file_ != nullptr; }

 private:
  FileLifecycleLogger(const std::shared_ptr<FileSystem>& fs, Env* env,
                      std::string path, std::string db_name, std::string role,
                      std::string db_id, bool verbose);
  IOStatus Open();
  void AppendLine(const std::string& line);

  std::shared_ptr<FileSystem> fs_;
  Env* env_;
  std::string path_;
  std::string db_name_;
  std::string role_;
  std::string db_id_;
  bool verbose_;
  std::unique_ptr<FSWritableFile> file_;
  std::mutex mu_;
  std::atomic<uint64_t> sequence_;
};

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
