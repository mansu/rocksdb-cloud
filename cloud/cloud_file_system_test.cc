// Copyright (c) 2017 Rockset

#include "rocksdb/cloud/cloud_file_system.h"

#include "cloud/cloud_log_controller_impl.h"
#include "rocksdb/cloud/cloud_file_system_impl.h"
#include "rocksdb/cloud/cloud_log_controller.h"
#include "rocksdb/cloud/cloud_storage_provider.h"
#include "rocksdb/cloud/cloud_storage_provider_impl.h"
#include "rocksdb/convenience.h"
#include "rocksdb/env.h"
#include "sys/wait.h"
#include "unistd.h"
#include "test_util/testharness.h"
#include "util/string_util.h"
#include "rocksdb/utilities/object_registry.h"

namespace ROCKSDB_NAMESPACE {

namespace {

class TestLogWritableFile final : public CloudLogWritableFile {
 public:
  using CloudLogWritableFile::Append;

  TestLogWritableFile(Env* env, CloudFileSystem* cloud_fs,
                      const std::string& fname, const FileOptions& options)
      : CloudLogWritableFile(env, cloud_fs, fname, options), size_(0) {}

  IOStatus LogDelete() override { return IOStatus::OK(); }

  IOStatus Append(const Slice& data, const IOOptions& /*options*/,
                  IODebugContext* /*dbg*/) override {
    size_ += data.size();
    return IOStatus::OK();
  }

  IOStatus Append(const Slice& data, const IOOptions& options,
                  const DataVerificationInfo& /*verification_info*/,
                  IODebugContext* dbg) override {
    return Append(data, options, dbg);
  }

  IOStatus Flush(const IOOptions& /*options*/,
                 IODebugContext* /*dbg*/) override {
    return IOStatus::OK();
  }

  IOStatus Sync(const IOOptions& /*options*/,
                IODebugContext* /*dbg*/) override {
    return IOStatus::OK();
  }

  IOStatus Close(const IOOptions& /*options*/,
                 IODebugContext* /*dbg*/) override {
    return IOStatus::OK();
  }

  uint64_t GetFileSize(const IOOptions& /*options*/,
                       IODebugContext* /*dbg*/) override {
    return size_;
  }

 private:
  uint64_t size_;
};

class TestLogController final : public CloudLogControllerImpl {
 public:
  static const char* kName() { return "test-log"; }
  const char* Name() const override { return kName(); }

  IOStatus CreateStream(const std::string& /*topic*/) override {
    return IOStatus::OK();
  }

  IOStatus WaitForStreamReady(const std::string& /*topic*/) override {
    return IOStatus::OK();
  }

  IOStatus TailStream() override { return IOStatus::OK(); }

  CloudLogWritableFile* CreateWritableFile(const std::string& fname,
                                           const FileOptions& options,
                                           IODebugContext* /*dbg*/) override {
    return new TestLogWritableFile(env_, cloud_fs_, fname, options);
  }

  IOStatus StartTailingStream(const std::string& /*topic*/) override {
    return IOStatus::OK();
  }
};

class TestStorageProvider final : public CloudStorageProvider {
 public:
  static const char* kName() { return "test-provider"; }
  const char* Name() const override { return kName(); }

  IOStatus CreateBucket(const std::string& /*bucket_name*/) override {
    return IOStatus::OK();
  }
  IOStatus ExistsBucket(const std::string& /*bucket_name*/) override {
    return IOStatus::OK();
  }
  IOStatus EmptyBucket(const std::string& /*bucket_name*/,
                       const std::string& /*object_path*/) override {
    return IOStatus::OK();
  }
  IOStatus DeleteCloudObject(const std::string& /*bucket_name*/,
                             const std::string& /*object_path*/) override {
    return IOStatus::OK();
  }
  IOStatus ListCloudObjects(const std::string& /*bucket_name*/,
                            const std::string& /*object_path*/,
                            std::vector<std::string>* path_names) override {
    path_names->clear();
    return IOStatus::OK();
  }
  IOStatus ExistsCloudObject(const std::string& /*bucket_name*/,
                             const std::string& /*object_path*/) override {
    return IOStatus::OK();
  }
  IOStatus GetCloudObjectSize(const std::string& /*bucket_name*/,
                              const std::string& /*object_path*/,
                              uint64_t* filesize) override {
    *filesize = 0;
    return IOStatus::OK();
  }
  IOStatus GetCloudObjectModificationTime(
      const std::string& /*bucket_name*/, const std::string& /*object_path*/,
      uint64_t* time) override {
    *time = 0;
    return IOStatus::OK();
  }
  IOStatus GetCloudObjectMetadata(
      const std::string& /*bucket_name*/, const std::string& /*object_path*/,
      CloudObjectInformation* info) override {
    info->size = 0;
    info->modification_time = 0;
    info->content_hash.clear();
    info->metadata.clear();
    return IOStatus::OK();
  }
  IOStatus CopyCloudObject(const std::string& /*src_bucket_name*/,
                           const std::string& /*src_object_path*/,
                           const std::string& /*dest_bucket_name*/,
                           const std::string& /*dest_object_path*/) override {
    return IOStatus::OK();
  }
  IOStatus GetCloudObject(const std::string& /*bucket_name*/,
                          const std::string& /*object_path*/,
                          const std::string& /*local_path*/) override {
    return IOStatus::OK();
  }
  IOStatus PutCloudObject(const std::string& /*local_path*/,
                          const std::string& /*bucket_name*/,
                          const std::string& /*object_path*/,
                          const PutObjectOptions& /*options*/) override {
    return IOStatus::OK();
  }
  IOStatus PutCloudObjectMetadata(
      const std::string& /*bucket_name*/, const std::string& /*object_path*/,
      const std::unordered_map<std::string, std::string>& /*metadata*/) override {
    return IOStatus::OK();
  }
  IOStatus NewCloudWritableFile(
      const std::string& /*local_path*/, const std::string& /*bucket_name*/,
      const std::string& /*object_path*/, const FileOptions& /*options*/,
      std::unique_ptr<CloudStorageWritableFile>* /*result*/,
      IODebugContext* /*dbg*/) override {
    return IOStatus::NotSupported("Test provider");
  }
  IOStatus NewCloudReadableFile(
      const std::string& /*bucket*/, const std::string& /*fname*/,
      const FileOptions& /*options*/,
      std::unique_ptr<CloudStorageReadableFile>* /*result*/,
      IODebugContext* /*dbg*/) override {
    return IOStatus::NotSupported("Test provider");
  }
};

}  // namespace

TEST(CloudFileSystemTest, TestBucket) {
  CloudFileSystemOptions copts;
  copts.src_bucket.SetRegion("North");
  copts.src_bucket.SetBucketName("Input", "src.");
  ASSERT_FALSE(copts.src_bucket.IsValid());
  copts.src_bucket.SetObjectPath("Here");
  ASSERT_TRUE(copts.src_bucket.IsValid());

  copts.dest_bucket.SetRegion("South");
  copts.dest_bucket.SetObjectPath("There");
  ASSERT_FALSE(copts.dest_bucket.IsValid());
  copts.dest_bucket.SetBucketName("Output", "dest.");
  ASSERT_TRUE(copts.dest_bucket.IsValid());
}

TEST(CloudFileSystemTest, ConfigureOptions) {
  ConfigOptions config_options;
  CloudFileSystemOptions copts, copy;
  copts.keep_local_sst_files = false;
  copts.keep_local_log_files = false;
  copts.create_bucket_if_missing = false;
  copts.validate_filesize = false;
  copts.skip_dbid_verification = false;
  copts.resync_on_open = false;
  copts.skip_cloud_files_in_getchildren = false;
  copts.constant_sst_file_size_in_sst_file_manager = 100;
  copts.run_purger = false;
  copts.purger_periodicity_millis = 101;

  std::string str;
  ASSERT_OK(copts.Serialize(config_options, &str));
  ASSERT_OK(copy.Configure(config_options, str));
  ASSERT_FALSE(copy.keep_local_sst_files);
  ASSERT_FALSE(copy.keep_local_log_files);
  ASSERT_FALSE(copy.create_bucket_if_missing);
  ASSERT_FALSE(copy.validate_filesize);
  ASSERT_FALSE(copy.skip_dbid_verification);
  ASSERT_FALSE(copy.resync_on_open);
  ASSERT_FALSE(copy.skip_cloud_files_in_getchildren);
  ASSERT_FALSE(copy.run_purger);
  ASSERT_EQ(copy.constant_sst_file_size_in_sst_file_manager, 100);
  ASSERT_EQ(copy.purger_periodicity_millis, 101);

  // Now try a different value
  copts.keep_local_sst_files = true;
  copts.keep_local_log_files = true;
  copts.create_bucket_if_missing = true;
  copts.validate_filesize = true;
  copts.skip_dbid_verification = true;
  copts.resync_on_open = true;
  copts.skip_cloud_files_in_getchildren = true;
  copts.constant_sst_file_size_in_sst_file_manager = 200;
  copts.run_purger = true;
  copts.purger_periodicity_millis = 201;

  ASSERT_OK(copts.Serialize(config_options, &str));
  ASSERT_OK(copy.Configure(config_options, str));
  ASSERT_TRUE(copy.keep_local_sst_files);
  ASSERT_TRUE(copy.keep_local_log_files);
  ASSERT_TRUE(copy.create_bucket_if_missing);
  ASSERT_TRUE(copy.validate_filesize);
  ASSERT_TRUE(copy.skip_dbid_verification);
  ASSERT_TRUE(copy.resync_on_open);
  ASSERT_TRUE(copy.skip_cloud_files_in_getchildren);
  ASSERT_TRUE(copy.run_purger);
  ASSERT_EQ(copy.constant_sst_file_size_in_sst_file_manager, 200);
  ASSERT_EQ(copy.purger_periodicity_millis, 201);
}

TEST(CloudFileSystemTest, ConfigureBucketOptions) {
  ConfigOptions config_options;
  CloudFileSystemOptions copts, copy;
  std::string str;
  copts.src_bucket.SetBucketName("source", "src.");
  copts.src_bucket.SetObjectPath("foo");
  copts.src_bucket.SetRegion("north");
  copts.dest_bucket.SetBucketName("dest");
  copts.dest_bucket.SetObjectPath("bar");
  ASSERT_OK(copts.Serialize(config_options, &str));

  ASSERT_OK(copy.Configure(config_options, str));
  ASSERT_EQ(copts.src_bucket.GetBucketName(), copy.src_bucket.GetBucketName());
  ASSERT_EQ(copts.src_bucket.GetObjectPath(), copy.src_bucket.GetObjectPath());
  ASSERT_EQ(copts.src_bucket.GetRegion(), copy.src_bucket.GetRegion());

  ASSERT_EQ(copts.dest_bucket.GetBucketName(),
            copy.dest_bucket.GetBucketName());
  ASSERT_EQ(copts.dest_bucket.GetObjectPath(),
            copy.dest_bucket.GetObjectPath());
  ASSERT_EQ(copts.dest_bucket.GetRegion(), copy.dest_bucket.GetRegion());
}

TEST(CloudFileSystemTest, ConfigureEnv) {
  std::unique_ptr<CloudFileSystem> cfs;

  ConfigOptions config_options;
  config_options.invoke_prepare_options = false;
  ASSERT_OK(CloudFileSystemEnv::CreateFromString(
      config_options, "keep_local_sst_files=true", &cfs));
  ASSERT_NE(cfs, nullptr);
  ASSERT_STREQ(cfs->Name(), "cloud");
  auto copts = cfs->GetOptions<CloudFileSystemOptions>();
  ASSERT_NE(copts, nullptr);
  ASSERT_TRUE(copts->keep_local_sst_files);
}

TEST(CloudFileSystemTest, TestInitialize) {
  std::unique_ptr<CloudFileSystem> cfs;
  BucketOptions bucket;
  ConfigOptions config_options;
  config_options.invoke_prepare_options = false;
  ASSERT_OK(CloudFileSystemEnv::CreateFromString(
      config_options, "id=cloud; TEST=cloudenvtest:/test/path", &cfs));
  ASSERT_NE(cfs, nullptr);
  ASSERT_STREQ(cfs->Name(), "cloud");

  ASSERT_TRUE(StartsWith(cfs->GetSrcBucketName(),
                         bucket.GetBucketPrefix() + "cloudenvtest."));
  ASSERT_EQ(cfs->GetSrcObjectPath(), "/test/path");
  ASSERT_TRUE(cfs->SrcMatchesDest());

  ASSERT_OK(CloudFileSystemEnv::CreateFromString(
      config_options, "id=cloud; TEST=cloudenvtest2:/test/path2?here", &cfs));
  ASSERT_NE(cfs, nullptr);
  ASSERT_STREQ(cfs->Name(), "cloud");
  ASSERT_TRUE(StartsWith(cfs->GetSrcBucketName(),
                         bucket.GetBucketPrefix() + "cloudenvtest2."));
  ASSERT_EQ(cfs->GetSrcObjectPath(), "/test/path2");
  ASSERT_EQ(cfs->GetCloudFileSystemOptions().src_bucket.GetRegion(), "here");
  ASSERT_TRUE(cfs->SrcMatchesDest());

  ASSERT_OK(CloudFileSystemEnv::CreateFromString(
      config_options,
      "id=cloud; TEST=cloudenvtest3:/test/path3; "
      "src.bucket=my_bucket; dest.object=/my_path",
      &cfs));
  ASSERT_NE(cfs, nullptr);
  ASSERT_STREQ(cfs->Name(), "cloud");
  ASSERT_EQ(cfs->GetSrcBucketName(), bucket.GetBucketPrefix() + "my_bucket");
  ASSERT_EQ(cfs->GetSrcObjectPath(), "/test/path3");
  ASSERT_TRUE(StartsWith(cfs->GetDestBucketName(),
                         bucket.GetBucketPrefix() + "cloudenvtest3."));
  ASSERT_EQ(cfs->GetDestObjectPath(), "/my_path");
}

TEST(CloudFileSystemTest, ConfigureAwsEnv) {
  std::unique_ptr<CloudFileSystem> cfs;

  ConfigOptions config_options;
  Status s = CloudFileSystemEnv::CreateFromString(
      config_options, "id=aws; keep_local_sst_files=true", &cfs);
#ifdef USE_AWS
  ASSERT_OK(s);
  ASSERT_NE(cfs, nullptr);
  ASSERT_STREQ(cfs->Name(), "aws");
  auto copts = cfs->GetOptions<CloudFileSystemOptions>();
  ASSERT_NE(copts, nullptr);
  ASSERT_TRUE(copts->keep_local_sst_files);
  ASSERT_NE(cfs->GetStorageProvider(), nullptr);
  ASSERT_STREQ(cfs->GetStorageProvider()->Name(),
               CloudStorageProviderImpl::kS3());
#else
  ASSERT_NOK(s);
  ASSERT_EQ(cfs, nullptr);
#endif
}

TEST(CloudFileSystemTest, ConfigureS3Provider) {
  std::unique_ptr<CloudFileSystem> cfs;

  ConfigOptions config_options;
  Status s =
      CloudFileSystemEnv::CreateFromString(config_options, "provider=s3", &cfs);
  ASSERT_NOK(s);
  ASSERT_EQ(cfs, nullptr);

#ifdef USE_AWS
  ASSERT_OK(CloudFileSystemEnv::CreateFromString(config_options,
                                              "id=aws; provider=s3", &cfs));
  ASSERT_STREQ(cfs->Name(), "aws");
  ASSERT_NE(cfs->GetStorageProvider(), nullptr);
  ASSERT_STREQ(cfs->GetStorageProvider()->Name(),
               CloudStorageProviderImpl::kS3());
#endif
}

// Test is disabled until we have a mock provider and authentication issues are
// resolved
TEST(CloudFileSystemTest, DISABLED_ConfigureKinesisController) {
  std::unique_ptr<CloudFileSystem> cfs;

  ConfigOptions config_options;
  Status s = CloudFileSystemEnv::CreateFromString(
      config_options, "provider=mock; controller=kinesis", &cfs);
  ASSERT_NOK(s);
  ASSERT_EQ(cfs, nullptr);

#ifdef USE_AWS
  ASSERT_OK(CloudFileSystemEnv::CreateFromString(
      config_options, "id=aws; controller=kinesis; TEST=dbcloud:/test", &cfs));
  ASSERT_STREQ(cfs->Name(), "aws");
  ASSERT_NE(cfs->GetCloudFileSystemOptions().cloud_log_controller, nullptr);
  ASSERT_STREQ(cfs->GetCloudFileSystemOptions().cloud_log_controller->Name(),
               CloudLogControllerImpl::kKinesis());
#endif
}

TEST(CloudFileSystemTest, ConfigureKafkaController) {
  std::unique_ptr<CloudFileSystem> cfs;

  ConfigOptions config_options;
  Status s = CloudFileSystemEnv::CreateFromString(
      config_options, "provider=mock; controller=kafka", &cfs);
#ifdef USE_KAFKA
  ASSERT_OK(s);
  ASSERT_NE(cfs, nullptr);
  ASSERT_NE(cfs->GetCloudFileSystemOptions().cloud_log_controller, nullptr);
  ASSERT_STREQ(cfs->GetCloudFileSystemOptions().cloud_log_controller->Name(),
               CloudLogControllerImpl::kKafka());
#else
  ASSERT_NOK(s);
  ASSERT_EQ(cfs, nullptr);
#endif
}

TEST(CloudFileSystemTest, LogControllerEnvLifetime) {
  pid_t pid = fork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    ObjectLibrary::Default()->AddFactory<CloudLogController>(
        TestLogController::kName(),
        [](const std::string& /*name*/,
           std::unique_ptr<CloudLogController>* guard,
           std::string* /*errmsg*/) {
          guard->reset(new TestLogController());
          return guard->get();
        });

    ObjectLibrary::Default()->AddFactory<CloudStorageProvider>(
        TestStorageProvider::kName(),
        [](const std::string& /*name*/,
           std::unique_ptr<CloudStorageProvider>* guard,
           std::string* /*errmsg*/) {
          guard->reset(new TestStorageProvider());
          return guard->get();
        });

    ConfigOptions config_options;
    config_options.env = Env::Default();
    config_options.invoke_prepare_options = true;

    std::unique_ptr<CloudFileSystem> cfs;
    if (!CloudFileSystemEnv::CreateFromString(
             config_options,
             "id=cloud; TEST=cloudenvtest:/test/path; keep_local_log_files=false; "
             "controller=test-log; provider=test-provider",
             &cfs)
             .ok()) {
      _exit(2);
    }
    if (cfs == nullptr) {
      _exit(3);
    }

    auto controller = cfs->GetCloudFileSystemOptions().cloud_log_controller;
    if (controller == nullptr || controller->GetCacheDir().empty()) {
      _exit(4);
    }

    std::string file_path = controller->GetCacheDir() + "/000001.log";
    std::unique_ptr<FSWritableFile> tmp;
    IOStatus create_status = cfs->GetBaseFileSystem()->NewWritableFile(
        file_path, FileOptions(), &tmp, nullptr);
    if (!create_status.ok()) {
      _exit(5);
    }
    if (!tmp->Append(Slice("data"), IOOptions(), nullptr).ok()) {
      _exit(6);
    }
    tmp.reset();

    uint64_t size = 0;
    IOStatus size_status = controller->GetFileSize("000001.log", &size);
    if (!size_status.ok() || size == 0) {
      _exit(7);
    }
    _exit(0);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status))
      << "Child terminated by signal " << WTERMSIG(status);
  ASSERT_EQ(WEXITSTATUS(status), 0);
}

}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
