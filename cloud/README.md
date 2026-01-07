## RocksDB-Cloud on Amazon Web Services (AWS)

This directory contains the extensions needed to make rocksdb store
files in AWS environment.

### Example
Here is an [example](https://github.com/rockset/rocksdb-cloud/blob/master/cloud/examples/cloud_durable_example.cc)  of code that uses rocksdb-cloud. The Makefile in that directory shows how you can link your application with the rocksdb-cloud library.

### Compile
The compilation process assumes that the AWS c++ SDK is installed in
the default location of /usr/local. You can follow the steps listed
here https://github.com/aws/aws-sdk-cpp (version 1.7.325 or later)
to install the c++ AWS sdk.

If you want to compile rocksdb with AWS support, please set the following
environment variable USE_AWS=1. 

If you want to compile rocksdb so that the write ahead log is stored
in Kafka, then set environment variable USE_KAFKA=1. You have to use
the C++ kafka client by downloading and installing the code from
https://github.com/edenhill/librdkafka.

Then run

   make clean all db_bench

This will create the libraries that you can link into your application.

The cloud unit tests need a AWS S3 bucket to store files. Please set the
following environment variables to run the cloud unit tests:

AWS_ACCESS_KEY_ID     : your aws access credentials

AWS_SECRET_ACCESS_KEY : your secret key

AWS_DEFAULT_REGION : the AWS region of your client (e.g. us-west-2)

### Run Unit Tests

make check J=1

### Measure Performance
To run dbbench,
   db_bench --env_uri="s3://" --aws_access_id=xxx and --aws_secret_key=yyy
This will create files in a bucket named rockset.dbbench.$USER where $USER is the name of the user who is running the benchmark.

### File Lifecycle Logging (SST visibility, compaction, deletion, S3 ops)
RocksDB-Cloud can emit a JSONL timeline of file lifecycle events (both reader
and writer). This includes SST create/compact/delete, cloud visibility changes,
S3 list/get/put/delete calls, and periodic snapshots of live files.

Enable via CloudFileSystemOptions:
- enable_file_lifecycle_logging=true
- file_lifecycle_log_path=/path/to/file_lifecycle.jsonl (optional)
  - If empty, defaults to db_log_dir (or local dbname if db_log_dir is empty)
    with filename file_lifecycle.jsonl.
- file_lifecycle_snapshot_period_sec=60 (0 disables snapshots)
- file_lifecycle_verbose=false|true (verbose adds per-op cloud/local FS events)

Each JSONL line includes:
- ts_us, seq (monotonic per-process), event, db_name, db_id, role (reader/writer)
- mode (compact|verbose), thread_id
- event-specific fields (file_number, file_path, reason, status, etc)

Quick filtering:
- compact-only view from a verbose run:
  jq -c 'select(.mode=="compact")' /path/to/file_lifecycle.jsonl
- state chains by file_number:
  tools/file_lifecycle_timeline.sh /path/to/file_lifecycle.jsonl --state-chain
- timeline filtered by file_number:
  tools/file_lifecycle_timeline.sh /path/to/file_lifecycle.jsonl --file-number 123

Common events (non-exhaustive):
- flush_begin/flush_completed
- compaction_begin/compaction_completed
- table_file_creation_started/table_file_created/table_file_deleted
- blob_file_created/blob_file_deleted
- cloud_invisible_local_delete/cloud_invisible_cloud_delete
- cloud_delete_scheduled/cloud_delete_done/cloud_upload
- snapshot_begin/snapshot_file/snapshot_end


