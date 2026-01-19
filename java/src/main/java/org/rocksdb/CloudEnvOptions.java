// Copyright (c) 2024-present, Rockset, Inc.
// Licensed under the Apache License 2.0 and GPLv2; you may not use this file
// except in compliance with one of these Licenses.

package org.rocksdb;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Convenience container for building a cloud environment configuration string.
 * This mirrors the options expected by {@code CloudFileSystemEnv::CreateFromString}.
 */
public class CloudEnvOptions {
  private String id = "aws";
  private String srcBucket;
  private String srcObject;
  private String srcRegion;
  private String destBucket;
  private String destObject;
  private String destRegion;
  private String accessKeyId;
  private String secretAccessKey;
  private String awsConfigFile;
  private boolean keepLocalSstFiles = true;
  private boolean keepLocalLogFiles = true;
  private boolean resyncOnOpen = false;
  private boolean rollManifestOnOpen = true;
  private boolean useAwsTransferManager = false;
  private String endpointOverride;
  private boolean usePathStyle = false;
  private String cookieOnOpen = "";
  private String newCookieOnOpen = "";
  private boolean createBucketIfMissing = true;
  private boolean invokePrepareOptions = true;
  private String controller;
  private final Map<String, String> kafkaConfigs = new LinkedHashMap<>();

  public CloudEnvOptions() {}

  public CloudEnvOptions setId(final String id) {
    this.id = id;
    return this;
  }

  public CloudEnvOptions setSrcBucket(final String srcBucket) {
    this.srcBucket = srcBucket;
    return this;
  }

  public CloudEnvOptions setSrcObject(final String srcObject) {
    this.srcObject = srcObject;
    return this;
  }

  public CloudEnvOptions setSrcRegion(final String srcRegion) {
    this.srcRegion = srcRegion;
    return this;
  }

  public CloudEnvOptions setDestBucket(final String destBucket) {
    this.destBucket = destBucket;
    return this;
  }

  public CloudEnvOptions setDestObject(final String destObject) {
    this.destObject = destObject;
    return this;
  }

  public CloudEnvOptions setDestRegion(final String destRegion) {
    this.destRegion = destRegion;
    return this;
  }

  public CloudEnvOptions setAccessKeyId(final String accessKeyId) {
    this.accessKeyId = accessKeyId;
    return this;
  }

  public CloudEnvOptions setSecretAccessKey(final String secretAccessKey) {
    this.secretAccessKey = secretAccessKey;
    return this;
  }

  /**
   * Optional AWS shared config/credentials file path (e.g., ~/.aws/credentials).
   */
  public CloudEnvOptions setAwsConfigFile(final String awsConfigFile) {
    this.awsConfigFile = awsConfigFile;
    return this;
  }

  public CloudEnvOptions setKeepLocalSstFiles(final boolean keepLocalSstFiles) {
    this.keepLocalSstFiles = keepLocalSstFiles;
    return this;
  }

  public CloudEnvOptions setKeepLocalLogFiles(final boolean keepLocalLogFiles) {
    this.keepLocalLogFiles = keepLocalLogFiles;
    return this;
  }

  public CloudEnvOptions setResyncOnOpen(final boolean resyncOnOpen) {
    this.resyncOnOpen = resyncOnOpen;
    return this;
  }

  public CloudEnvOptions setRollManifestOnOpen(final boolean rollManifestOnOpen) {
    this.rollManifestOnOpen = rollManifestOnOpen;
    return this;
  }

  public CloudEnvOptions setUseAwsTransferManager(final boolean useAwsTransferManager) {
    this.useAwsTransferManager = useAwsTransferManager;
    return this;
  }

  /**
   * Set custom S3-compatible endpoint URL (e.g., for MinIO, LocalStack, S3Mock).
   * If not set, uses the default AWS S3 endpoint.
   */
  public CloudEnvOptions setEndpointOverride(final String endpointOverride) {
    this.endpointOverride = endpointOverride;
    return this;
  }

  /**
   * If true, use path-style addressing (e.g., endpoint/bucket/key) instead of
   * virtual-hosted-style (e.g., bucket.endpoint/key). Required for most
   * S3-compatible services like MinIO, LocalStack, and S3Mock.
   * Default: false
   */
  public CloudEnvOptions setUsePathStyle(final boolean usePathStyle) {
    this.usePathStyle = usePathStyle;
    return this;
  }

  public CloudEnvOptions setCookieOnOpen(final String cookieOnOpen) {
    this.cookieOnOpen = cookieOnOpen;
    return this;
  }

  public CloudEnvOptions setNewCookieOnOpen(final String newCookieOnOpen) {
    this.newCookieOnOpen = newCookieOnOpen;
    return this;
  }

  /**
   * Whether to create the destination bucket if it is missing.
   * Default: true.
   */
  public CloudEnvOptions setCreateBucketIfMissing(
      final boolean createBucketIfMissing) {
    this.createBucketIfMissing = createBucketIfMissing;
    return this;
  }

  /**
   * Whether to invoke option preparation when constructing the cloud env.
   * Default: true.
   */
  public CloudEnvOptions setInvokePrepareOptions(final boolean invoke) {
    this.invokePrepareOptions = invoke;
    return this;
  }

  /**
   * Set the cloud log controller (e.g., "kafka" or "kinesis").
   */
  public CloudEnvOptions setController(final String controller) {
    this.controller = controller;
    return this;
  }

  /**
   * Add a Kafka client config entry (key/value).
   */
  public CloudEnvOptions putKafkaConfig(final String key, final String value) {
    if (key == null || key.isEmpty() || value == null) {
      return this;
    }
    kafkaConfigs.put(key, value);
    return this;
  }

  boolean invokePrepareOptions() {
    return invokePrepareOptions;
  }

  /**
   * Build the config string understood by CloudEnv.
   */
  String toConfigString() {
    final StringBuilder sb = new StringBuilder();
    append(sb, "id", id);
    append(sb, "src.bucket", srcBucket);
    append(sb, "src.object", srcObject);
    append(sb, "src.region", srcRegion);
    append(sb, "dest.bucket", destBucket);
    append(sb, "dest.object", destObject);
    append(sb, "dest.region", destRegion);
    append(sb, "keep_local_sst_files", keepLocalSstFiles);
    append(sb, "keep_local_log_files", keepLocalLogFiles);
    append(sb, "resync_on_open", resyncOnOpen);
    append(sb, "roll_cloud_manifest_on_open", rollManifestOnOpen);
    append(sb, "use_aws_transfer_manager", useAwsTransferManager);
    append(sb, "endpoint_override", endpointOverride);
    append(sb, "use_path_style", usePathStyle);
    append(sb, "cookie_on_open", cookieOnOpen);
    append(sb, "new_cookie_on_open", newCookieOnOpen);
    append(sb, "create_bucket_if_missing", createBucketIfMissing);
    append(sb, "s3.access_key_id", accessKeyId);
    append(sb, "s3.secret_access_key", secretAccessKey);
    append(sb, "s3.config_file", awsConfigFile);
    append(sb, "controller", controller);
    for (Map.Entry<String, String> entry : kafkaConfigs.entrySet()) {
      append(sb, "kafka.config." + entry.getKey(), entry.getValue());
    }
    // strip trailing ';' if present
    if (sb.length() > 0 && sb.charAt(sb.length() - 1) == ';') {
      sb.setLength(sb.length() - 1);
    }
    return sb.toString();
  }

  /**
   * Construct a cloud-enabled {@link Env} using these options.
   */
  public Env createEnv(final Env baseEnv) throws RocksDBException {
    return CloudEnv.create(baseEnv, toConfigString(), invokePrepareOptions);
  }

  /**
   * Obtain a builder for {@link CloudEnvOptions}.
   */
  public static Builder newBuilder() {
    return new Builder();
  }

  /**
   * Builder for {@link CloudEnvOptions}.
   */
  public static final class Builder {
    private final CloudEnvOptions opts = new CloudEnvOptions();

    public Builder setId(final String id) {
      opts.setId(id);
      return this;
    }

    public Builder setSrcBucket(final String srcBucket) {
      opts.setSrcBucket(srcBucket);
      return this;
    }

    public Builder setSrcBucketName(final String srcBucket) {
      opts.setSrcBucket(srcBucket);
      return this;
    }

    public Builder setSrcObject(final String srcObject) {
      opts.setSrcObject(srcObject);
      return this;
    }

    public Builder setSrcObjectPath(final String srcObject) {
      opts.setSrcObject(srcObject);
      return this;
    }

    public Builder setSrcRegion(final String srcRegion) {
      opts.setSrcRegion(srcRegion);
      return this;
    }

    public Builder setDestBucket(final String destBucket) {
      opts.setDestBucket(destBucket);
      return this;
    }

    public Builder setDestBucketName(final String destBucket) {
      opts.setDestBucket(destBucket);
      return this;
    }

    public Builder setDestObject(final String destObject) {
      opts.setDestObject(destObject);
      return this;
    }

    public Builder setDestObjectPath(final String destObject) {
      opts.setDestObject(destObject);
      return this;
    }

    public Builder setDestRegion(final String destRegion) {
      opts.setDestRegion(destRegion);
      return this;
    }

    public Builder setAccessKeyId(final String accessKeyId) {
      opts.setAccessKeyId(accessKeyId);
      return this;
    }

    public Builder setSecretAccessKey(final String secretAccessKey) {
      opts.setSecretAccessKey(secretAccessKey);
      return this;
    }

    public Builder setAwsConfigFile(final String awsConfigFile) {
      opts.setAwsConfigFile(awsConfigFile);
      return this;
    }

    public Builder setInvokePrepareOptions(final boolean invoke) {
      opts.setInvokePrepareOptions(invoke);
      return this;
    }

    public Builder setKeepLocalSstFiles(final boolean keepLocalSstFiles) {
      opts.setKeepLocalSstFiles(keepLocalSstFiles);
      return this;
    }

    public Builder setKeepLocalLogFiles(final boolean keepLocalLogFiles) {
      opts.setKeepLocalLogFiles(keepLocalLogFiles);
      return this;
    }

    public Builder setResyncOnOpen(final boolean resyncOnOpen) {
      opts.setResyncOnOpen(resyncOnOpen);
      return this;
    }

    public Builder setUseAwsTransferManager(final boolean useAwsTransferManager) {
      opts.setUseAwsTransferManager(useAwsTransferManager);
      return this;
    }

    public Builder setEndpointOverride(final String endpointOverride) {
      opts.setEndpointOverride(endpointOverride);
      return this;
    }

    public Builder setUsePathStyle(final boolean usePathStyle) {
      opts.setUsePathStyle(usePathStyle);
      return this;
    }

    public Builder setRollManifestOnOpen(final boolean rollManifestOnOpen) {
      opts.setRollManifestOnOpen(rollManifestOnOpen);
      return this;
    }

    public Builder setCookieOnOpen(final String cookieOnOpen) {
      opts.setCookieOnOpen(cookieOnOpen);
      return this;
    }

    public Builder setNewCookieOnOpen(final String newCookieOnOpen) {
      opts.setNewCookieOnOpen(newCookieOnOpen);
      return this;
    }

    public Builder setCreateBucketIfMissing(final boolean createBucketIfMissing) {
      opts.setCreateBucketIfMissing(createBucketIfMissing);
      return this;
    }

    public Builder setController(final String controller) {
      opts.setController(controller);
      return this;
    }

    public Builder putKafkaConfig(final String key, final String value) {
      opts.putKafkaConfig(key, value);
      return this;
    }

    public Builder setKafkaConfigs(final Map<String, String> configs) {
      if (configs != null) {
        for (Map.Entry<String, String> entry : configs.entrySet()) {
          opts.putKafkaConfig(entry.getKey(), entry.getValue());
        }
      }
      return this;
    }

    public CloudEnvOptions build() {
      return opts;
    }
  }

  private static void append(final StringBuilder sb, final String key, final String value) {
    if (value == null || value.isEmpty()) {
      return;
    }
    sb.append(key).append('=').append(value).append(';');
  }

  private static void append(final StringBuilder sb, final String key, final boolean value) {
    sb.append(key).append('=').append(value).append(';');
  }
}
