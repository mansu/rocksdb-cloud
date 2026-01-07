// Copyright (c) 2024-present, Rockset, Inc.  All rights reserved.
#ifndef ROCKSDB_LITE

#include "cloud/file_lifecycle_tracker.h"

#include <algorithm>

#include "cloud/cloud_manifest.h"
#include "cloud/filename.h"
#include "rocksdb/cloud/cloud_file_system_impl.h"
#include "rocksdb/metadata.h"

namespace ROCKSDB_NAMESPACE {

namespace {
const char* TemperatureToString(Temperature temp) {
  switch (temp) {
    case Temperature::kUnknown:
      return "unknown";
    case Temperature::kHot:
      return "hot";
    case Temperature::kWarm:
      return "warm";
    case Temperature::kCold:
      return "cold";
    case Temperature::kLastTemperature:
      break;
  }
  return "unknown";
}

std::string GetEpochSuffix(const std::string& name) {
  auto base = basename(name);
  auto last_dash = base.rfind('-');
  if (last_dash == std::string::npos) {
    return "";
  }
  return base.substr(last_dash + 1);
}
}  // namespace

FileLifecycleTracker::FileLifecycleTracker(
    DB* db, CloudFileSystemImpl* cfs,
    const std::shared_ptr<FileLifecycleLogger>& logger, Env* env,
    uint64_t snapshot_period_sec)
    : db_(db),
      cfs_(cfs),
      logger_(logger),
      env_(env),
      snapshot_period_sec_(snapshot_period_sec),
      stop_(false),
      snapshot_id_(0) {}

FileLifecycleTracker::~FileLifecycleTracker() { Stop(); }

void FileLifecycleTracker::Start() {
  if (!logger_ || !logger_->enabled() || snapshot_period_sec_ == 0 ||
      !env_ || thread_.joinable()) {
    return;
  }
  thread_ = std::thread([this]() { Run(); });
}

void FileLifecycleTracker::Stop() {
  stop_.store(true, std::memory_order_relaxed);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void FileLifecycleTracker::Run() {
  while (!stop_.load(std::memory_order_relaxed)) {
    env_->SleepForMicroseconds(
        static_cast<int>(snapshot_period_sec_ * 1000000));
    if (stop_.load(std::memory_order_relaxed)) {
      break;
    }
    LogSnapshot();
  }
}

void FileLifecycleTracker::LogSnapshot() {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  std::vector<LiveFileMetaData> live_files;
  db_->GetLiveFilesMetaData(&live_files);
  std::sort(live_files.begin(), live_files.end(),
            [](const LiveFileMetaData& lhs, const LiveFileMetaData& rhs) {
              return lhs.file_number < rhs.file_number;
            });

  uint64_t snapshot_id =
      snapshot_id_.fetch_add(1, std::memory_order_relaxed);

  std::string epoch;
  if (cfs_ && cfs_->GetCloudManifest()) {
    epoch = cfs_->GetCloudManifest()->GetCurrentEpoch();
  }

  logger_->LogEvent("snapshot_begin", [&](FileLifecycleLogger::JsonWriter* w) {
    w->AddUint64("snapshot_id", snapshot_id);
    w->AddUint64("file_count", live_files.size());
    if (!epoch.empty()) {
      w->AddString("cloud_epoch", epoch);
    }
  });

  for (const auto& file : live_files) {
    logger_->LogEvent("snapshot_file", [&](FileLifecycleLogger::JsonWriter* w) {
      std::string state = "live";
      if (!epoch.empty()) {
        auto suffix = GetEpochSuffix(file.name);
        if (!suffix.empty() && suffix != epoch) {
          state = "invisible";
        }
      }
      w->AddUint64("snapshot_id", snapshot_id);
      w->AddUint64("file_number", file.file_number);
      w->AddString("file_name", file.name);
      w->AddString("relative_filename", file.relative_filename);
      w->AddString("db_path", file.db_path);
      w->AddString("file_state", state);
      w->AddUint64("level", file.level);
      w->AddUint64("size_bytes", file.size);
      w->AddUint64("smallest_seqno", file.smallest_seqno);
      w->AddUint64("largest_seqno", file.largest_seqno);
      w->AddUint64("num_entries", file.num_entries);
      w->AddUint64("num_deletions", file.num_deletions);
      w->AddUint64("oldest_blob_file_number", file.oldest_blob_file_number);
      w->AddInt64("epoch_number", file.epoch_number);
      w->AddString("temperature", TemperatureToString(file.temperature));
    });
  }

  logger_->LogEvent("snapshot_end", [&](FileLifecycleLogger::JsonWriter* w) {
    w->AddUint64("snapshot_id", snapshot_id);
  });
}

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
