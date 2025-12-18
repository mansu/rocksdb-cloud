// Copyright (c) 2024-present, Rockset, Inc.  All rights reserved.
// Licensed under the Apache License 2.0 and GPLv2; you may not use this file
// except in compliance with one of these Licenses.

package org.rocksdb;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * RocksDB variant with cloud support. Requires {@link Options#setEnv(Env)} to
 * be configured with a cloud-enabled {@link Env}, for example via
 * Use with a cloud-enabled {@link Env}, for example one created via
 * {@link CloudEnvOptions#createEnv(Env)}.
 */
public class DBCloud extends RocksDB {
  /**
   * If this DBCloud instance created its own {@link Env} (e.g. via the
   * {@link #open(Options, String, CloudEnvOptions, String, long, boolean)}
   * overload), it is retained here and closed when this DBCloud is closed.
   */
  private Env ownedEnv_;

  private DBCloud(final long nativeHandle) {
    super(nativeHandle);
  }

  @Override
  public void close() {
    try {
      super.close();
    } finally {
      final Env env = ownedEnv_;
      ownedEnv_ = null;
      if (env != null) {
        env.close();
      }
    }
  }

  /**
   * Open a cloud-enabled database.
   *
   * @param options database options (must include a cloud-aware Env).
   * @param path local path.
   * @param persistentCachePath path for persistent cache (may be empty).
   * @param persistentCacheSizeGb cache size in GB.
   * @param readOnly whether to open read-only.
   * @return opened {@link DBCloud}.
   * @throws RocksDBException on error.
   */
  public static DBCloud open(final Options options, final String path,
      final String persistentCachePath, final long persistentCacheSizeGb,
      final boolean readOnly) throws RocksDBException {
    final DBCloud db = new DBCloud(openInternal(options.nativeHandle_, path,
        persistentCachePath, persistentCacheSizeGb, readOnly));
    db.storeOptionsInstance(options);
    db.storeDefaultColumnFamilyHandle(db.makeDefaultColumnFamilyHandle());
    return db;
  }

  /**
   * Open a cloud-enabled database with default cache settings.
   *
   * @param options database options (must include a cloud-aware Env).
   * @param path local path.
   * @return opened {@link DBCloud}.
   * @throws RocksDBException on error.
   */
  public static DBCloud open(final Options options, final String path)
      throws RocksDBException {
    return open(options, path, "", 0, false);
  }

  /**
   * Open a cloud-enabled database with column families.
   *
   * @param options database options (must include a cloud-aware Env).
   * @param path local path.
   * @param persistentCachePath path for persistent cache (may be empty).
   * @param persistentCacheSizeGb cache size in GB.
   * @param columnFamilyDescriptors column family descriptors.
   * @param columnFamilyHandles filled with opened handles.
   * @param readOnly whether to open read-only.
   * @return opened {@link DBCloud}.
   * @throws RocksDBException on error.
   */
  public static DBCloud open(final Options options, final String path,
      final String persistentCachePath, final long persistentCacheSizeGb,
      final List<ColumnFamilyDescriptor> columnFamilyDescriptors,
      final List<ColumnFamilyHandle> columnFamilyHandles,
      final boolean readOnly) throws RocksDBException {
    int defaultColumnFamilyIndex = -1;
    final byte[][] cfNames = new byte[columnFamilyDescriptors.size()][];
    final long[] cfOptionHandles = new long[columnFamilyDescriptors.size()];
    for (int i = 0; i < columnFamilyDescriptors.size(); i++) {
      final ColumnFamilyDescriptor cfDescriptor =
          columnFamilyDescriptors.get(i);
      cfNames[i] = cfDescriptor.getName();
      cfOptionHandles[i] = cfDescriptor.getOptions().nativeHandle_;
      if (Arrays.equals(cfDescriptor.getName(),
          RocksDB.DEFAULT_COLUMN_FAMILY)) {
        defaultColumnFamilyIndex = i;
      }
    }
    if (defaultColumnFamilyIndex < 0) {
      throw new IllegalArgumentException(
          "You must provide the default column family in your columnFamilyDescriptors");
    }

    final long[] handles = openColumnFamilies(options.nativeHandle_, path,
        cfNames, cfOptionHandles, persistentCachePath,
        persistentCacheSizeGb, readOnly);
    final DBCloud db = new DBCloud(handles[0]);

    db.storeOptionsInstance(options);

    for (int i = 1; i < handles.length; i++) {
      columnFamilyHandles.add(new ColumnFamilyHandle(db, handles[i]));
    }
    db.ownedColumnFamilyHandles.addAll(columnFamilyHandles);
    db.storeDefaultColumnFamilyHandle(
        columnFamilyHandles.get(defaultColumnFamilyIndex));

    return db;
  }

  /**
   * Open a cloud-enabled database with column families (read/write).
   */
  public static DBCloud open(final Options options, final String path,
      final List<ColumnFamilyDescriptor> columnFamilyDescriptors,
      final List<ColumnFamilyHandle> columnFamilyHandles)
      throws RocksDBException {
    return open(options, path, "", 0, columnFamilyDescriptors,
        columnFamilyHandles, false);
  }

  /**
   * Open a cloud-enabled database with column families (read-only).
   */
  public static DBCloud openReadOnly(final Options options, final String path,
      final List<ColumnFamilyDescriptor> columnFamilyDescriptors,
      final List<ColumnFamilyHandle> columnFamilyHandles)
      throws RocksDBException {
    return open(options, path, "", 0, columnFamilyDescriptors,
        columnFamilyHandles, true);
  }

  /**
   * Run a savepoint, synchronously copying relevant files to destination
   * cloud storage.
   *
   * @throws RocksDBException on error.
   */
  public void savepoint() throws RocksDBException {
    savepoint(nativeHandle_);
  }

  /**
   * Resync local clone state with the cloud manifest.
   * This is intended for ephemeral clones (no destination bucket).
   *
   * @throws RocksDBException on error.
   */
  public void resync() throws RocksDBException {
    resync(nativeHandle_);
  }

  /**
   * List column families for a cloud database.
   *
   * @param dbOptions database options (must include a cloud-aware Env).
   * @param name path of the database.
   * @return list of column family names.
   * @throws RocksDBException on error.
   */
  public static List<byte[]> listColumnFamilies(final DBOptions dbOptions,
      final String name) throws RocksDBException {
    final byte[][] list = listColumnFamilies(
        dbOptions.nativeHandle_, name);
    final ArrayList<byte[]> result = new ArrayList<>(list.length);
    for (final byte[] cf : list) {
      result.add(cf);
    }
    return result;
  }

  private static native long openInternal(long optionsHandle, String path,
      String persistentCachePath, long persistentCacheSizeGb,
      boolean readOnly) throws RocksDBException;

  private static native long[] openColumnFamilies(long optionsHandle,
      String path, byte[][] columnFamilies, long[] columnFamilyOptions,
      String persistentCachePath, long persistentCacheSizeGb,
      boolean readOnly) throws RocksDBException;

  private static native void savepoint(long nativeHandle)
      throws RocksDBException;

  private static native void resync(long nativeHandle)
      throws RocksDBException;

  private static native byte[][] listColumnFamilies(long dbOptionsHandle,
      String name) throws RocksDBException;

  /**
   * Convenience overload to open using {@link CloudEnvOptions}. This will set
   * the Env on the provided {@link Options} instance.
   */
  public static DBCloud open(final Options options, final String path,
      final CloudEnvOptions cloudEnvOptions, final String persistentCachePath,
      final long persistentCacheSizeGb, final boolean readOnly)
      throws RocksDBException {
    final Env env = cloudEnvOptions.createEnv(Env.getDefault());
    options.setEnv(env);
    final DBCloud db = open(options, path, persistentCachePath, persistentCacheSizeGb, readOnly);
    db.ownedEnv_ = env;
    return db;
  }
}
