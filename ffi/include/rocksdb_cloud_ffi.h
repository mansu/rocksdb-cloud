#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rdbc_dbcloud rdbc_dbcloud;
typedef struct rdbc_iter rdbc_iter;

typedef struct {
  const char* bucket;
  const char* object_path;
  const char* region;
  const char* filesystem_id;
  const char* storage_provider;
  const char* access_key;
  const char* secret_key;
  const char* endpoint_override;
  const char* cookie;
  int use_path_style;
  int create_bucket_if_missing;
  int keep_local_sst_files;
  int keep_local_log_files;
  int resync_on_open;
  int use_aws_transfer_manager;
  int is_writer;
} rdbc_cloud_config;

rdbc_dbcloud* rdbc_open_readonly(const char* db_path,
                                 const rdbc_cloud_config* config,
                                 char** err_out);

void rdbc_close(rdbc_dbcloud* db);

int rdbc_get(rdbc_dbcloud* db,
             const unsigned char* key,
             size_t key_len,
             unsigned char** val_out,
             size_t* val_len_out,
             char** err_out);

void rdbc_free_bytes(unsigned char* bytes);

int rdbc_get_property(rdbc_dbcloud* db,
                      const char* name,
                      char** val_out,
                      char** err_out);

void rdbc_free_str(char* value);

rdbc_iter* rdbc_iter_new(rdbc_dbcloud* db);
void rdbc_iter_seek_to_first(rdbc_iter* it);
void rdbc_iter_seek(rdbc_iter* it, const unsigned char* key, size_t key_len);
int rdbc_iter_valid(rdbc_iter* it);
int rdbc_iter_key(rdbc_iter* it, unsigned char** key_out, size_t* key_len_out);
void rdbc_iter_next(rdbc_iter* it);
void rdbc_iter_destroy(rdbc_iter* it);

int rdbc_list_live_sst_files(rdbc_dbcloud* db,
                             char*** files_out,
                             size_t* count_out,
                             char** err_out);

void rdbc_free_str_list(char** list, size_t count);

#ifdef __cplusplus
}
#endif
