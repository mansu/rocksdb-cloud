// Copyright (c) 2024-present, Rockset, Inc.  All rights reserved.
#ifndef ROCKSDB_LITE

#include "cloud/file_lifecycle_logger.h"

#include <cstdio>
#include <sstream>

#include "cloud/filename.h"
#include "rocksdb/env.h"

namespace ROCKSDB_NAMESPACE {

namespace {
std::string JsonEscape(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 8);
  for (unsigned char ch : input) {
    switch (ch) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (ch < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", ch);
          out += buf;
        } else {
          out += static_cast<char>(ch);
        }
        break;
    }
  }
  return out;
}
}  // namespace

FileLifecycleLogger::JsonWriter::JsonWriter() : out_("{"), first_(true) {}

void FileLifecycleLogger::JsonWriter::AddKey(const char* key) {
  if (!first_) {
    out_ += ",";
  }
  first_ = false;
  out_ += "\"";
  out_ += key;
  out_ += "\":";
}

void FileLifecycleLogger::JsonWriter::AddString(const char* key,
                                                const std::string& value) {
  AddKey(key);
  out_ += "\"";
  out_ += JsonEscape(value);
  out_ += "\"";
}

void FileLifecycleLogger::JsonWriter::AddUint64(const char* key,
                                                uint64_t value) {
  AddKey(key);
  out_ += std::to_string(value);
}

void FileLifecycleLogger::JsonWriter::AddInt64(const char* key, int64_t value) {
  AddKey(key);
  out_ += std::to_string(value);
}

void FileLifecycleLogger::JsonWriter::AddBool(const char* key, bool value) {
  AddKey(key);
  out_ += value ? "true" : "false";
}

void FileLifecycleLogger::JsonWriter::AddDouble(const char* key, double value) {
  AddKey(key);
  out_ += std::to_string(value);
}

void FileLifecycleLogger::JsonWriter::AddStringList(
    const char* key, const std::vector<std::string>& values) {
  AddKey(key);
  out_ += "[";
  bool first = true;
  for (const auto& value : values) {
    if (!first) {
      out_ += ",";
    }
    first = false;
    out_ += "\"";
    out_ += JsonEscape(value);
    out_ += "\"";
  }
  out_ += "]";
}

void FileLifecycleLogger::JsonWriter::AddUint64List(
    const char* key, const std::vector<uint64_t>& values) {
  AddKey(key);
  out_ += "[";
  bool first = true;
  for (uint64_t value : values) {
    if (!first) {
      out_ += ",";
    }
    first = false;
    out_ += std::to_string(value);
  }
  out_ += "]";
}

std::string FileLifecycleLogger::JsonWriter::Finish() {
  out_ += "}";
  return out_;
}

std::shared_ptr<FileLifecycleLogger> FileLifecycleLogger::Create(
    const std::shared_ptr<FileSystem>& fs, Env* env, const std::string& path,
    const std::string& db_name, const std::string& role,
    const std::string& db_id, bool verbose) {
  if (!fs || env == nullptr || path.empty()) {
    return nullptr;
  }
  std::shared_ptr<FileLifecycleLogger> logger(
      new FileLifecycleLogger(fs, env, path, db_name, role, db_id, verbose));
  if (!logger->Open().ok()) {
    return nullptr;
  }
  return logger;
}

FileLifecycleLogger::FileLifecycleLogger(const std::shared_ptr<FileSystem>& fs,
                                         Env* env, std::string path,
                                         std::string db_name, std::string role,
                                         std::string db_id, bool verbose)
    : fs_(fs),
      env_(env),
      path_(std::move(path)),
      db_name_(std::move(db_name)),
      role_(std::move(role)),
      db_id_(std::move(db_id)),
      verbose_(verbose),
      sequence_(0) {}

IOStatus FileLifecycleLogger::Open() {
  const IOOptions io_opts;
  IODebugContext* dbg = nullptr;
  auto dir = dirname(path_);
  if (!dir.empty()) {
    fs_->CreateDirIfMissing(dir, io_opts, dbg).PermitUncheckedError();
  }
  const FileOptions file_opts;
  auto st = fs_->FileExists(path_, io_opts, dbg);
  if (st.ok()) {
    st = fs_->ReopenWritableFile(path_, file_opts, &file_, dbg);
    if (st.ok()) {
      return st;
    }
  }
  return fs_->NewWritableFile(path_, file_opts, &file_, dbg);
}

void FileLifecycleLogger::SetDbId(const std::string& db_id) {
  std::lock_guard<std::mutex> lock(mu_);
  db_id_ = db_id;
}

void FileLifecycleLogger::AppendLine(const std::string& line) {
  if (!file_) {
    return;
  }
  const IOOptions io_opts;
  IODebugContext* dbg = nullptr;
  file_->Append(line, io_opts, dbg).PermitUncheckedError();
  file_->Flush(io_opts, dbg).PermitUncheckedError();
}

void FileLifecycleLogger::LogEvent(
    const std::string& event,
    const std::function<void(JsonWriter*)>& add_fields) {
  if (!file_) {
    return;
  }
  std::string db_id_copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    db_id_copy = db_id_;
  }
  JsonWriter writer;
  writer.AddUint64("ts_us", env_->NowMicros());
  writer.AddUint64("seq", sequence_.fetch_add(1, std::memory_order_relaxed));
  writer.AddString("event", event);
  writer.AddString("db_name", db_name_);
  if (!db_id_copy.empty()) {
    writer.AddString("db_id", db_id_copy);
  }
  writer.AddString("role", role_);
  writer.AddString("mode", verbose_ ? "verbose" : "compact");
  writer.AddUint64("thread_id", env_->GetThreadID());
  if (add_fields) {
    add_fields(&writer);
  }
  std::string line = writer.Finish();
  line.push_back('\n');
  std::lock_guard<std::mutex> lock(mu_);
  AppendLine(line);
}

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
