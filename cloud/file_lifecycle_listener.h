// Copyright (c) 2024-present, Rockset, Inc.  All rights reserved.
#pragma once

#ifndef ROCKSDB_LITE
#include <memory>

#include "cloud/file_lifecycle_logger.h"
#include "rocksdb/listener.h"

namespace ROCKSDB_NAMESPACE {

class FileLifecycleListener : public EventListener {
 public:
  explicit FileLifecycleListener(
      const std::shared_ptr<FileLifecycleLogger>& logger);

  void OnFlushBegin(DB* db, const FlushJobInfo& info) override;
  void OnFlushCompleted(DB* db, const FlushJobInfo& info) override;
  void OnCompactionBegin(DB* db, const CompactionJobInfo& info) override;
  void OnCompactionCompleted(DB* db, const CompactionJobInfo& info) override;
  void OnTableFileCreationStarted(
      const TableFileCreationBriefInfo& info) override;
  void OnTableFileCreated(const TableFileCreationInfo& info) override;
  void OnTableFileDeleted(const TableFileDeletionInfo& info) override;
  void OnBlobFileCreated(const BlobFileCreationInfo& info) override;
  void OnBlobFileDeleted(const BlobFileDeletionInfo& info) override;

 private:
  std::shared_ptr<FileLifecycleLogger> logger_;
};

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
