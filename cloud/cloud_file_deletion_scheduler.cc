//  Copyright (c) 2016-present, Rockset, Inc.  All rights reserved.

#include "rocksdb/cloud/cloud_file_deletion_scheduler.h"

#include "cloud/cloud_scheduler.h"
#include "test_util/sync_point.h"

namespace ROCKSDB_NAMESPACE {

std::shared_ptr<CloudFileDeletionScheduler> CloudFileDeletionScheduler::Create(
     const std::shared_ptr<CloudScheduler>& scheduler,
     std::chrono::seconds file_deletion_delay) {
  return std::make_shared<CloudFileDeletionScheduler>(PrivateTag(), scheduler,
                                                      file_deletion_delay);
}

CloudFileDeletionScheduler::~CloudFileDeletionScheduler() {
  TEST_SYNC_POINT(
      "CloudFileDeletionScheduler::~CloudFileDeletionScheduler:"
      "BeforeCancelJobs");
  // NOTE: no need to cancel jobs here. These jobs won't be executed
  // as longs as `CloudFileDeletionScheduler` is destructed. Also,
  // `LocalCloudScheduler` will remove the jobs in the queue when destructed
}

void CloudFileDeletionScheduler::SetEventCallback(EventCallback cb) {
  std::lock_guard<std::mutex> lk(event_cb_mutex_);
  event_cb_ = std::move(cb);
}

CloudFileDeletionScheduler::EventCallback
CloudFileDeletionScheduler::GetEventCallback() const {
  std::lock_guard<std::mutex> lk(event_cb_mutex_);
  return event_cb_;
}

void CloudFileDeletionScheduler::EmitEvent(const char* event,
                                           const std::string& filename,
                                           const std::string& detail) {
  auto cb = GetEventCallback();
  if (cb) {
    cb(event, filename, detail, GetQueueSize());
  }
}

uint64_t CloudFileDeletionScheduler::GetQueueSize() const {
  std::lock_guard<std::mutex> lk(files_to_delete_mutex_);
  return files_to_delete_.size();
}

void CloudFileDeletionScheduler::UnscheduleFileDeletion(const std::string& filename) {
  bool canceled = false;
  std::lock_guard<std::mutex> lk(files_to_delete_mutex_);
  auto itr = files_to_delete_.find(filename);
  if (itr != files_to_delete_.end()) {
    scheduler_->CancelJob(itr->second);
    files_to_delete_.erase(itr);
    canceled = true;
  }
  if (canceled) {
    EmitEvent("cloud_delete_job_canceled", filename, "");
  } else {
    EmitEvent("cloud_delete_job_cancel_miss", filename, "not_scheduled");
  }
}

rocksdb::IOStatus CloudFileDeletionScheduler::ScheduleFileDeletion(
    const std::string& fname, FileDeletionRunnable runnable) {
  auto wp = this->weak_from_this();
  auto doDeleteFile = [wp = std::move(wp), fname, runnable = std::move(runnable)](void*) {
    TEST_SYNC_POINT(
        "CloudFileDeletionScheduler::ScheduleFileDeletion:BeforeFileDeletion");
    auto sp = wp.lock();
    bool file_deleted = false;
    if (sp) {
      file_deleted = true;
      sp->DoDeleteFile(std::move(fname), std::move(runnable));
    }
    TEST_SYNC_POINT_CALLBACK(
        "CloudFileDeletionScheduler::ScheduleFileDeletion:AfterFileDeletion",
        &file_deleted);
    (void) file_deleted;
  };

  bool already_scheduled = false;
  bool scheduled = false;
  {
    std::lock_guard<std::mutex> lk(files_to_delete_mutex_);
    if (files_to_delete_.find(fname) != files_to_delete_.end()) {
      // already in the queue
      already_scheduled = true;
    } else {
      auto handle = scheduler_->ScheduleJob(file_deletion_delay_,
                                            std::move(doDeleteFile), nullptr);
      files_to_delete_.emplace(fname, std::move(handle));
      scheduled = true;
    }
  }
  if (already_scheduled) {
    EmitEvent("cloud_delete_job_already_scheduled", fname, "");
  } else if (scheduled) {
    EmitEvent("cloud_delete_job_scheduled", fname,
              "delay_sec=" + std::to_string(file_deletion_delay_.count()));
  }
  return IOStatus::OK();
}

void CloudFileDeletionScheduler::DoDeleteFile(const std::string& fname,
                                              FileDeletionRunnable runnable) {
  bool missing = false;
  {
    std::lock_guard<std::mutex> lk(files_to_delete_mutex_);
    auto itr = files_to_delete_.find(fname);
    if (itr == files_to_delete_.end()) {
      // File was removed from files_to_delete_, do not delete!
      missing = true;
    } else {
      files_to_delete_.erase(itr);
    }
  }

  if (missing) {
    EmitEvent("cloud_delete_job_missing", fname, "not_in_queue");
    return;
  }
  EmitEvent("cloud_delete_job_fired", fname, "");
  runnable();
}

#ifndef NDEBUG
size_t CloudFileDeletionScheduler::TEST_NumScheduledJobs() const {
  return scheduler_->TEST_NumScheduledJobs();
}
#endif

}
