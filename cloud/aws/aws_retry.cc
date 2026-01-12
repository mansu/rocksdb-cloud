//  Copyright (c) 2017-present, Rockset, Inc.  All rights reserved.
//
//

#include <cinttypes>
#include <cstdlib>
#include <string>

#include "cloud/aws/aws_file.h"
#include "rocksdb/cloud/cloud_file_system.h"
#ifdef USE_AWS
#include <aws/core/client/AWSError.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/client/SpecifiedRetryableErrorsRetryStrategy.h>
#include <aws/core/client/RetryStrategy.h>
#include <aws/core/http/Scheme.h>
#endif  // USE_AWS

namespace ROCKSDB_NAMESPACE {
#ifdef USE_AWS
namespace {
std::string GetEnvOrEmpty(const char* name) {
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
}

std::string ToLowerAscii(std::string value) {
  for (char& ch : value) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return value;
}
}  // namespace

//
// Ability to configure retry policies for the AWS client
//
class AwsRetryStrategy : public Aws::Client::RetryStrategy {
 public:
  AwsRetryStrategy(CloudFileSystem* fs) : cfs_(fs) {
    // In many environments, AccessDenied and ExpiredToken errors are retryable.
    // This is because HTTP requests are involved in fetching the new tokens and
    // credentials, which can fail.
    Aws::Vector<Aws::String> retryableErrors;
    retryableErrors.push_back("AccessDenied");
    retryableErrors.push_back("ExpiredToken");
    retryableErrors.push_back("InternalError");
    default_strategy_ =
        std::make_shared<Aws::Client::SpecifiedRetryableErrorsRetryStrategy>(
            retryableErrors);
    Log(InfoLogLevel::INFO_LEVEL, cfs_->GetLogger(),
        "[aws] Configured custom retry policy");
  }

  ~AwsRetryStrategy() override {}

  // Returns true if the error can be retried given the error and the number of
  // times already tried.
  bool ShouldRetry(const Aws::Client::AWSError<Aws::Client::CoreErrors>& error,
                   long attemptedRetries) const override;

  // Calculates the time in milliseconds the client should sleep before
  // attempting another request based on the error and attemptedRetries count.
  long CalculateDelayBeforeNextRetry(
      const Aws::Client::AWSError<Aws::Client::CoreErrors>& error,
      long attemptedRetries) const override;

 private:
  // rocksdb retries, etc
  CloudFileSystem* cfs_;

  // The default strategy implemented by AWS client
  std::shared_ptr<Aws::Client::RetryStrategy> default_strategy_;

  // The number of times an internal-error failure should be retried
  const int internal_failure_num_retries_{10};
};

//
// Returns true if the error can be retried given the error and the number of
// times already tried.
//
bool AwsRetryStrategy::ShouldRetry(
    const Aws::Client::AWSError<Aws::Client::CoreErrors>& error,
    long attemptedRetries) const {
  auto ce = error.GetErrorType();
  const Aws::String errmsg = error.GetMessage();
  const Aws::String exceptionMsg = error.GetExceptionName();
  std::string err(errmsg.c_str(), errmsg.size());
  std::string emsg(exceptionMsg.c_str(), exceptionMsg.size());

  // Internal errors are unknown errors and we try harder to fix them
  //
  if (ce == Aws::Client::CoreErrors::INTERNAL_FAILURE ||
      ce == Aws::Client::CoreErrors::UNKNOWN ||
      err.find("try again") != std::string::npos) {
    if (attemptedRetries <= internal_failure_num_retries_) {
      Log(InfoLogLevel::INFO_LEVEL, cfs_->GetLogger(),
          "[aws] Encountered retriable failure: %s (code %d, http %d). "
          "Exception %s. retry attempt %ld is lesser than max retries %d. "
          "Retrying...",
          err.c_str(), static_cast<int>(ce),
          static_cast<int>(error.GetResponseCode()), emsg.c_str(),
          attemptedRetries, internal_failure_num_retries_);
      return true;
    }
    Log(InfoLogLevel::INFO_LEVEL, cfs_->GetLogger(),
        "[aws] Encountered retriable failure: %s (code %d, http %d). Exception "
        "%s. retry attempt %ld exceeds max retries %d. Aborting...",
        err.c_str(), static_cast<int>(ce),
        static_cast<int>(error.GetResponseCode()), emsg.c_str(),
        attemptedRetries, internal_failure_num_retries_);
    return false;
  }
  Log(InfoLogLevel::WARN_LEVEL, cfs_->GetLogger(),
      "[aws] Encountered S3 failure %s (code %d, http %d). Exception %s."
      " retry attempt %ld max retries %d. Using default retry policy...",
      err.c_str(), static_cast<int>(ce),
      static_cast<int>(error.GetResponseCode()), emsg.c_str(), attemptedRetries,
      internal_failure_num_retries_);
  return default_strategy_->ShouldRetry(error, attemptedRetries);
}

//
// Calculates the time in milliseconds the client should sleep before
// attempting another request based on the error and attemptedRetries count.
//
long AwsRetryStrategy::CalculateDelayBeforeNextRetry(
    const Aws::Client::AWSError<Aws::Client::CoreErrors>& error,
    long attemptedRetries) const {
  return default_strategy_->CalculateDelayBeforeNextRetry(error,
                                                          attemptedRetries);
}

Status AwsCloudOptions::GetClientConfiguration(
    CloudFileSystem* fs, const std::string& region,
    Aws::Client::ClientConfiguration* config) {
  config->connectTimeoutMs = 30000;
  config->requestTimeoutMs = 600000;

  const auto& cloud_fs_options = fs->GetCloudFileSystemOptions();
  // Setup how retries need to be done
  config->retryStrategy = std::make_shared<AwsRetryStrategy>(fs);
  if (cloud_fs_options.request_timeout_ms != 0) {
    config->requestTimeoutMs = cloud_fs_options.request_timeout_ms;
  }

  config->region = ToAwsString(region);
  std::string endpoint = GetEnvOrEmpty("ROCKSDB_CLOUD_S3_ENDPOINT");
  if (endpoint.empty()) {
    endpoint = GetEnvOrEmpty("ROCKSDB_AWS_S3_ENDPOINT");
  }
  if (!endpoint.empty()) {
    std::string endpoint_lower = ToLowerAscii(endpoint);
    const char* http_prefix = "http://";
    const char* https_prefix = "https://";
    if (endpoint_lower.rfind(http_prefix, 0) == 0) {
      config->scheme = Aws::Http::Scheme::HTTP;
      endpoint.erase(0, std::string(http_prefix).size());
    } else if (endpoint_lower.rfind(https_prefix, 0) == 0) {
      config->scheme = Aws::Http::Scheme::HTTPS;
      endpoint.erase(0, std::string(https_prefix).size());
    }
    config->endpointOverride = Aws::String(endpoint.c_str());
  }
  return Status::OK();
}
#else
Status AwsCloudOptions::GetClientConfiguration(
    CloudFileSystem*, const std::string&, Aws::Client::ClientConfiguration*) {
  return Status::NotSupported("Not configured for AWS support");
}
#endif /* USE_AWS */

}  // namespace ROCKSDB_NAMESPACE
