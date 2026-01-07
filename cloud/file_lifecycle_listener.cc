// Copyright (c) 2024-present, Rockset, Inc.  All rights reserved.
#ifndef ROCKSDB_LITE

#include "cloud/file_lifecycle_listener.h"

#include <vector>

#include "cloud/filename.h"
#include "file/filename.h"
#include "rocksdb/listener.h"

namespace ROCKSDB_NAMESPACE {

namespace {
std::vector<std::string> FormatCompactionFileInfos(
    const std::vector<CompactionFileInfo>& infos) {
  std::vector<std::string> out;
  out.reserve(infos.size());
  for (const auto& info : infos) {
    out.push_back("L" + std::to_string(info.level) + ":" +
                  std::to_string(info.file_number));
  }
  return out;
}

const char* TableFileCreationReasonToString(TableFileCreationReason reason) {
  switch (reason) {
    case TableFileCreationReason::kFlush:
      return "flush";
    case TableFileCreationReason::kCompaction:
      return "compaction";
    case TableFileCreationReason::kRecovery:
      return "recovery";
    case TableFileCreationReason::kMisc:
      return "misc";
  }
  return "unknown";
}

const char* BlobFileCreationReasonToString(BlobFileCreationReason reason) {
  switch (reason) {
    case BlobFileCreationReason::kFlush:
      return "flush";
    case BlobFileCreationReason::kCompaction:
      return "compaction";
    case BlobFileCreationReason::kRecovery:
      return "recovery";
  }
  return "unknown";
}

void AddFileNumberIfPresent(FileLifecycleLogger::JsonWriter* writer,
                            const std::string& path) {
  uint64_t number = 0;
  FileType type = kTempFile;
  if (ParseFileName(basename(path), &number, &type) &&
      (type == kTableFile || type == kBlobFile)) {
    writer->AddUint64("file_number", number);
  }
}
}  // namespace

FileLifecycleListener::FileLifecycleListener(
    const std::shared_ptr<FileLifecycleLogger>& logger)
    : logger_(logger) {}

void FileLifecycleListener::OnFlushBegin(DB* /*db*/,
                                         const FlushJobInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("flush_begin", [&](FileLifecycleLogger::JsonWriter* w) {
    w->AddString("cf_name", info.cf_name);
    w->AddUint64("cf_id", info.cf_id);
    w->AddUint64("job_id", info.job_id);
    w->AddString("reason", GetFlushReasonString(info.flush_reason));
  });
}

void FileLifecycleListener::OnFlushCompleted(DB* /*db*/,
                                             const FlushJobInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("flush_completed", [&](FileLifecycleLogger::JsonWriter* w) {
    w->AddString("cf_name", info.cf_name);
    w->AddUint64("cf_id", info.cf_id);
    w->AddUint64("job_id", info.job_id);
    w->AddString("file_path", info.file_path);
    w->AddUint64("file_number", info.file_number);
    w->AddUint64("oldest_blob_file_number", info.oldest_blob_file_number);
    w->AddUint64("smallest_seqno", info.smallest_seqno);
    w->AddUint64("largest_seqno", info.largest_seqno);
    w->AddString("reason", GetFlushReasonString(info.flush_reason));
    w->AddBool("triggered_writes_slowdown",
               info.triggered_writes_slowdown);
    w->AddBool("triggered_writes_stop", info.triggered_writes_stop);
  });
}

void FileLifecycleListener::OnCompactionBegin(DB* /*db*/,
                                              const CompactionJobInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("compaction_begin",
                    [&](FileLifecycleLogger::JsonWriter* w) {
                      w->AddString("cf_name", info.cf_name);
                      w->AddUint64("cf_id", info.cf_id);
                      w->AddUint64("job_id", info.job_id);
                      w->AddInt64("base_input_level",
                                  static_cast<int64_t>(info.base_input_level));
                      w->AddInt64("output_level",
                                  static_cast<int64_t>(info.output_level));
                      w->AddString(
                          "reason",
                          GetCompactionReasonString(info.compaction_reason));
                      w->AddStringList("input_files", info.input_files);
                      w->AddStringList(
                          "input_file_infos",
                          FormatCompactionFileInfos(info.input_file_infos));
                    });
}

void FileLifecycleListener::OnCompactionCompleted(
    DB* /*db*/, const CompactionJobInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("compaction_completed",
                    [&](FileLifecycleLogger::JsonWriter* w) {
                      w->AddString("cf_name", info.cf_name);
                      w->AddUint64("cf_id", info.cf_id);
                      w->AddUint64("job_id", info.job_id);
                      w->AddInt64("base_input_level",
                                  static_cast<int64_t>(info.base_input_level));
                      w->AddInt64("output_level",
                                  static_cast<int64_t>(info.output_level));
                      w->AddString(
                          "reason",
                          GetCompactionReasonString(info.compaction_reason));
                      w->AddString("status", info.status.ToString());
                      w->AddStringList("input_files", info.input_files);
                      w->AddStringList(
                          "input_file_infos",
                          FormatCompactionFileInfos(info.input_file_infos));
                      w->AddStringList("output_files", info.output_files);
                      w->AddStringList(
                          "output_file_infos",
                          FormatCompactionFileInfos(info.output_file_infos));
                    });
}

void FileLifecycleListener::OnTableFileCreationStarted(
    const TableFileCreationBriefInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent(
      "table_file_creation_started",
      [&](FileLifecycleLogger::JsonWriter* w) {
        w->AddString("cf_name", info.cf_name);
        w->AddString("file_path", info.file_path);
        w->AddUint64("job_id", info.job_id);
        w->AddString("reason", TableFileCreationReasonToString(info.reason));
        AddFileNumberIfPresent(w, info.file_path);
      });
}

void FileLifecycleListener::OnTableFileCreated(
    const TableFileCreationInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("table_file_created",
                    [&](FileLifecycleLogger::JsonWriter* w) {
                      w->AddString("cf_name", info.cf_name);
                      w->AddString("file_path", info.file_path);
                      w->AddUint64("job_id", info.job_id);
                      w->AddUint64("file_size", info.file_size);
                      w->AddString("status", info.status.ToString());
                      w->AddString("reason",
                                   TableFileCreationReasonToString(
                                       info.reason));
                      w->AddString("file_checksum", info.file_checksum);
                      w->AddString("file_checksum_func_name",
                                   info.file_checksum_func_name);
                      AddFileNumberIfPresent(w, info.file_path);
                    });
}

void FileLifecycleListener::OnTableFileDeleted(
    const TableFileDeletionInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("table_file_deleted",
                    [&](FileLifecycleLogger::JsonWriter* w) {
                      w->AddString("file_path", info.file_path);
                      w->AddUint64("job_id", info.job_id);
                      w->AddString("status", info.status.ToString());
                      AddFileNumberIfPresent(w, info.file_path);
                    });
}

void FileLifecycleListener::OnBlobFileCreated(
    const BlobFileCreationInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("blob_file_created",
                    [&](FileLifecycleLogger::JsonWriter* w) {
                      w->AddString("cf_name", info.cf_name);
                      w->AddString("file_path", info.file_path);
                      w->AddUint64("job_id", info.job_id);
                      w->AddUint64("total_blob_count", info.total_blob_count);
                      w->AddUint64("total_blob_bytes", info.total_blob_bytes);
                      w->AddString(
                          "reason",
                          BlobFileCreationReasonToString(info.reason));
                      w->AddString("status", info.status.ToString());
                      w->AddString("file_checksum", info.file_checksum);
                      w->AddString("file_checksum_func_name",
                                   info.file_checksum_func_name);
                      AddFileNumberIfPresent(w, info.file_path);
                    });
}

void FileLifecycleListener::OnBlobFileDeleted(
    const BlobFileDeletionInfo& info) {
  if (!logger_ || !logger_->enabled()) {
    return;
  }
  logger_->LogEvent("blob_file_deleted",
                    [&](FileLifecycleLogger::JsonWriter* w) {
                      w->AddString("file_path", info.file_path);
                      w->AddUint64("job_id", info.job_id);
                      w->AddString("status", info.status.ToString());
                      AddFileNumberIfPresent(w, info.file_path);
                    });
}

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
