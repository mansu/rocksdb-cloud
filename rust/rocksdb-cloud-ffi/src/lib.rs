use libc::{c_char, c_int, size_t};
use std::fmt;
use std::path::{Path, PathBuf};

#[derive(Clone, Debug)]
pub struct CloudConfig {
    pub bucket_name: String,
    pub object_path: String,
    pub region: String,
    pub file_system_id: String,
    pub storage_provider: String,
    pub access_key: String,
    pub secret_key: String,
    pub endpoint_override: Option<String>,
    pub use_path_style: bool,
    pub cookie: String,
    pub create_bucket_if_missing: bool,
    pub keep_local_sst_files: bool,
    pub keep_local_log_files: bool,
    pub resync_on_open: bool,
    pub use_aws_transfer_manager: bool,
    pub local_db_path: PathBuf,
    pub is_writer: bool,
}

#[derive(Debug)]
pub enum Error {
    Unavailable(&'static str),
    NulError,
    Ffi(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Unavailable(msg) => write!(f, "{}", msg),
            Error::NulError => write!(f, "nul byte in input"),
            Error::Ffi(msg) => write!(f, "{}", msg),
        }
    }
}

impl std::error::Error for Error {}

impl From<std::ffi::NulError> for Error {
    fn from(_: std::ffi::NulError) -> Self {
        Error::NulError
    }
}

pub type Result<T> = std::result::Result<T, Error>;

pub struct DbCloud {
    raw: *mut ffi::rdbc_dbcloud,
}

// Safety: RocksDB Cloud is thread-safe for read-only access; guard raw usage in the FFI layer.
unsafe impl Send for DbCloud {}
unsafe impl Sync for DbCloud {}

pub struct DbIterator {
    raw: *mut ffi::rdbc_iter,
}

unsafe impl Send for DbIterator {}
unsafe impl Sync for DbIterator {}

#[cfg(feature = "ffi")]
mod ffi {
    use super::{c_char, c_int, size_t};

    #[repr(C)]
    pub struct rdbc_cloud_config {
        pub bucket: *const c_char,
        pub object_path: *const c_char,
        pub region: *const c_char,
        pub filesystem_id: *const c_char,
        pub storage_provider: *const c_char,
        pub access_key: *const c_char,
        pub secret_key: *const c_char,
        pub endpoint_override: *const c_char,
        pub cookie: *const c_char,
        pub use_path_style: c_int,
        pub create_bucket_if_missing: c_int,
        pub keep_local_sst_files: c_int,
        pub keep_local_log_files: c_int,
        pub resync_on_open: c_int,
        pub use_aws_transfer_manager: c_int,
        pub is_writer: c_int,
    }

    #[repr(C)]
    pub struct rdbc_dbcloud {
        _private: [u8; 0],
    }

    #[repr(C)]
    pub struct rdbc_iter {
        _private: [u8; 0],
    }

    #[link(name = "rocksdb_cloud_ffi")]
    extern "C" {
        pub fn rdbc_open_readonly(
            db_path: *const c_char,
            config: *const rdbc_cloud_config,
            err_out: *mut *mut c_char,
        ) -> *mut rdbc_dbcloud;
        pub fn rdbc_close(db: *mut rdbc_dbcloud);
        pub fn rdbc_get(
            db: *mut rdbc_dbcloud,
            key: *const u8,
            key_len: size_t,
            val_out: *mut *mut u8,
            val_len_out: *mut size_t,
            err_out: *mut *mut c_char,
        ) -> c_int;
        pub fn rdbc_free_bytes(bytes: *mut u8);
        pub fn rdbc_get_property(
            db: *mut rdbc_dbcloud,
            name: *const c_char,
            val_out: *mut *mut c_char,
            err_out: *mut *mut c_char,
        ) -> c_int;
        pub fn rdbc_free_str(value: *mut c_char);
        pub fn rdbc_iter_new(db: *mut rdbc_dbcloud) -> *mut rdbc_iter;
        pub fn rdbc_iter_seek_to_first(it: *mut rdbc_iter);
        pub fn rdbc_iter_seek(it: *mut rdbc_iter, key: *const u8, key_len: size_t);
        pub fn rdbc_iter_valid(it: *mut rdbc_iter) -> c_int;
        pub fn rdbc_iter_key(
            it: *mut rdbc_iter,
            key_out: *mut *mut u8,
            key_len_out: *mut size_t,
        ) -> c_int;
        pub fn rdbc_iter_next(it: *mut rdbc_iter);
        pub fn rdbc_iter_destroy(it: *mut rdbc_iter);
        pub fn rdbc_list_live_sst_files(
            db: *mut rdbc_dbcloud,
            files_out: *mut *mut *mut c_char,
            count_out: *mut size_t,
            err_out: *mut *mut c_char,
        ) -> c_int;
        pub fn rdbc_free_str_list(list: *mut *mut c_char, count: size_t);
    }
}

#[cfg(not(feature = "ffi"))]
#[allow(dead_code)]
mod ffi {
    use super::{c_char, c_int, size_t};

    #[repr(C)]
    pub struct rdbc_cloud_config {
        pub bucket: *const c_char,
        pub object_path: *const c_char,
        pub region: *const c_char,
        pub filesystem_id: *const c_char,
        pub storage_provider: *const c_char,
        pub access_key: *const c_char,
        pub secret_key: *const c_char,
        pub endpoint_override: *const c_char,
        pub cookie: *const c_char,
        pub use_path_style: c_int,
        pub create_bucket_if_missing: c_int,
        pub keep_local_sst_files: c_int,
        pub keep_local_log_files: c_int,
        pub resync_on_open: c_int,
        pub use_aws_transfer_manager: c_int,
        pub is_writer: c_int,
    }

    #[repr(C)]
    pub struct rdbc_dbcloud {
        _private: [u8; 0],
    }

    #[repr(C)]
    pub struct rdbc_iter {
        _private: [u8; 0],
    }
}

#[cfg(feature = "ffi")]
struct CloudConfigFfi {
    raw: ffi::rdbc_cloud_config,
    _strings: CloudConfigStrings,
}

#[cfg(feature = "ffi")]
#[allow(dead_code)]
struct CloudConfigStrings {
    bucket: std::ffi::CString,
    object_path: std::ffi::CString,
    region: std::ffi::CString,
    filesystem_id: std::ffi::CString,
    storage_provider: std::ffi::CString,
    access_key: std::ffi::CString,
    secret_key: std::ffi::CString,
    endpoint_override: Option<std::ffi::CString>,
    cookie: std::ffi::CString,
}

#[cfg(feature = "ffi")]
impl CloudConfigFfi {
    fn new(config: &CloudConfig) -> Result<Self> {
        let bucket = std::ffi::CString::new(config.bucket_name.as_str())?;
        let object_path = std::ffi::CString::new(config.object_path.as_str())?;
        let region = std::ffi::CString::new(config.region.as_str())?;
        let filesystem_id = std::ffi::CString::new(config.file_system_id.as_str())?;
        let storage_provider = std::ffi::CString::new(config.storage_provider.as_str())?;
        let access_key = std::ffi::CString::new(config.access_key.as_str())?;
        let secret_key = std::ffi::CString::new(config.secret_key.as_str())?;
        let endpoint_override = match config.endpoint_override.as_ref() {
            Some(value) => Some(std::ffi::CString::new(value.as_str())?),
            None => None,
        };
        let cookie = std::ffi::CString::new(config.cookie.as_str())?;

        let raw = ffi::rdbc_cloud_config {
            bucket: bucket.as_ptr(),
            object_path: object_path.as_ptr(),
            region: region.as_ptr(),
            filesystem_id: filesystem_id.as_ptr(),
            storage_provider: storage_provider.as_ptr(),
            access_key: access_key.as_ptr(),
            secret_key: secret_key.as_ptr(),
            endpoint_override: endpoint_override
                .as_ref()
                .map_or(std::ptr::null(), |value| value.as_ptr()),
            cookie: cookie.as_ptr(),
            use_path_style: if config.use_path_style { 1 } else { 0 },
            create_bucket_if_missing: if config.create_bucket_if_missing { 1 } else { 0 },
            keep_local_sst_files: if config.keep_local_sst_files { 1 } else { 0 },
            keep_local_log_files: if config.keep_local_log_files { 1 } else { 0 },
            resync_on_open: if config.resync_on_open { 1 } else { 0 },
            use_aws_transfer_manager: if config.use_aws_transfer_manager { 1 } else { 0 },
            is_writer: if config.is_writer { 1 } else { 0 },
        };

        Ok(Self {
            raw,
            _strings: CloudConfigStrings {
                bucket,
                object_path,
                region,
                filesystem_id,
                storage_provider,
                access_key,
                secret_key,
                endpoint_override,
                cookie,
            },
        })
    }
}

#[cfg(feature = "ffi")]
fn take_error(err: *mut c_char) -> Error {
    if err.is_null() {
        return Error::Ffi("ffi error".to_string());
    }
    let message = unsafe { std::ffi::CStr::from_ptr(err) }
        .to_string_lossy()
        .into_owned();
    unsafe {
        ffi::rdbc_free_str(err);
    }
    Error::Ffi(message)
}

#[cfg(feature = "ffi")]
impl DbCloud {
    pub fn open_readonly(path: &Path, config: &CloudConfig) -> Result<Self> {
        let c_path = std::ffi::CString::new(path.to_string_lossy().as_bytes())?;
        let cfg = CloudConfigFfi::new(config)?;
        let mut err: *mut c_char = std::ptr::null_mut();
        let raw = unsafe { ffi::rdbc_open_readonly(c_path.as_ptr(), &cfg.raw, &mut err) };
        if raw.is_null() {
            return Err(take_error(err));
        }
        Ok(Self { raw })
    }

    pub fn get(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
        let mut val_ptr: *mut u8 = std::ptr::null_mut();
        let mut val_len: size_t = 0;
        let mut err: *mut c_char = std::ptr::null_mut();
        let rc = unsafe {
            ffi::rdbc_get(
                self.raw,
                key.as_ptr(),
                key.len(),
                &mut val_ptr,
                &mut val_len,
                &mut err,
            )
        };
        if rc != 0 {
            return Err(take_error(err));
        }
        if val_ptr.is_null() {
            return Ok(None);
        }
        let slice = unsafe { std::slice::from_raw_parts(val_ptr as *const u8, val_len as usize) };
        let out = slice.to_vec();
        unsafe {
            ffi::rdbc_free_bytes(val_ptr);
        }
        Ok(Some(out))
    }

    pub fn get_property(&self, name: &str) -> Result<String> {
        let c_name = std::ffi::CString::new(name)?;
        let mut val_ptr: *mut c_char = std::ptr::null_mut();
        let mut err: *mut c_char = std::ptr::null_mut();
        let rc = unsafe {
            ffi::rdbc_get_property(self.raw, c_name.as_ptr(), &mut val_ptr, &mut err)
        };
        if rc != 0 {
            return Err(take_error(err));
        }
        if val_ptr.is_null() {
            return Ok(String::new());
        }
        let value = unsafe { std::ffi::CStr::from_ptr(val_ptr) }
            .to_string_lossy()
            .into_owned();
        unsafe {
            ffi::rdbc_free_str(val_ptr);
        }
        Ok(value)
    }

    pub fn iter(&self) -> Result<DbIterator> {
        let raw = unsafe { ffi::rdbc_iter_new(self.raw) };
        if raw.is_null() {
            return Err(Error::Ffi("iterator init failed".to_string()));
        }
        Ok(DbIterator { raw })
    }

    pub fn list_live_sst_files(&self) -> Result<Vec<String>> {
        let mut files_ptr: *mut *mut c_char = std::ptr::null_mut();
        let mut count: size_t = 0;
        let mut err: *mut c_char = std::ptr::null_mut();
        let rc = unsafe {
            ffi::rdbc_list_live_sst_files(self.raw, &mut files_ptr, &mut count, &mut err)
        };
        if rc != 0 {
            return Err(take_error(err));
        }
        if files_ptr.is_null() || count == 0 {
            return Ok(Vec::new());
        }
        let slice = unsafe { std::slice::from_raw_parts(files_ptr, count as usize) };
        let mut out = Vec::with_capacity(slice.len());
        for &ptr in slice {
            if ptr.is_null() {
                continue;
            }
            let value = unsafe { std::ffi::CStr::from_ptr(ptr) }
                .to_string_lossy()
                .into_owned();
            out.push(value);
        }
        unsafe {
            ffi::rdbc_free_str_list(files_ptr, count);
        }
        Ok(out)
    }
}

#[cfg(not(feature = "ffi"))]
impl DbCloud {
    pub fn open_readonly(_path: &Path, _config: &CloudConfig) -> Result<Self> {
        Err(Error::Unavailable("rocksdb-cloud-ffi stub is enabled"))
    }

    pub fn get(&self, _key: &[u8]) -> Result<Option<Vec<u8>>> {
        Err(Error::Unavailable("rocksdb-cloud-ffi stub is enabled"))
    }

    pub fn get_property(&self, _name: &str) -> Result<String> {
        Err(Error::Unavailable("rocksdb-cloud-ffi stub is enabled"))
    }

    pub fn iter(&self) -> Result<DbIterator> {
        Err(Error::Unavailable("rocksdb-cloud-ffi stub is enabled"))
    }

    pub fn list_live_sst_files(&self) -> Result<Vec<String>> {
        Err(Error::Unavailable("rocksdb-cloud-ffi stub is enabled"))
    }
}

#[cfg(feature = "ffi")]
impl Drop for DbCloud {
    fn drop(&mut self) {
        unsafe {
            ffi::rdbc_close(self.raw);
        }
    }
}

#[cfg(not(feature = "ffi"))]
impl Drop for DbCloud {
    fn drop(&mut self) {}
}

#[cfg(feature = "ffi")]
impl DbIterator {
    pub fn seek_to_first(&mut self) {
        unsafe {
            ffi::rdbc_iter_seek_to_first(self.raw);
        }
    }

    pub fn seek(&mut self, key: &[u8]) {
        unsafe {
            ffi::rdbc_iter_seek(self.raw, key.as_ptr(), key.len());
        }
    }

    pub fn valid(&self) -> bool {
        unsafe { ffi::rdbc_iter_valid(self.raw) != 0 }
    }

    pub fn key(&self) -> Result<Vec<u8>> {
        let mut key_ptr: *mut u8 = std::ptr::null_mut();
        let mut key_len: size_t = 0;
        let rc = unsafe { ffi::rdbc_iter_key(self.raw, &mut key_ptr, &mut key_len) };
        if rc != 0 {
            return Err(Error::Ffi("iterator key failed".to_string()));
        }
        if key_ptr.is_null() {
            return Err(Error::Ffi("iterator key missing".to_string()));
        }
        let slice = unsafe { std::slice::from_raw_parts(key_ptr as *const u8, key_len as usize) };
        let out = slice.to_vec();
        unsafe {
            ffi::rdbc_free_bytes(key_ptr);
        }
        Ok(out)
    }

    pub fn next(&mut self) {
        unsafe {
            ffi::rdbc_iter_next(self.raw);
        }
    }
}

#[cfg(not(feature = "ffi"))]
impl DbIterator {
    pub fn seek_to_first(&mut self) {}

    pub fn seek(&mut self, _key: &[u8]) {}

    pub fn valid(&self) -> bool {
        false
    }

    pub fn key(&self) -> Result<Vec<u8>> {
        Err(Error::Unavailable("rocksdb-cloud-ffi stub is enabled"))
    }

    pub fn next(&mut self) {}
}

#[cfg(feature = "ffi")]
impl Drop for DbIterator {
    fn drop(&mut self) {
        unsafe {
            ffi::rdbc_iter_destroy(self.raw);
        }
    }
}

#[cfg(not(feature = "ffi"))]
impl Drop for DbIterator {
    fn drop(&mut self) {}
}
