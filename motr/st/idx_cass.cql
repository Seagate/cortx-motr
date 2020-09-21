
/* Create a keyspace for Motr Indices */
CREATE KEYSPACE IF NOT EXISTS index_keyspace
WITH REPLICATION = { 'class' : 'SimpleStrategy', 'replication_factor' : 1 };

use index_keyspace;

/*
 * Create column families for Motr indices, see idx_cass.c for details.
 * Currently, only one column family is used in tests.
 * 'cass_v150915' is the prefix used for column family names and
 * is secretly used in idx_cass.c(will change it later to allow applications
 * to specify it.).
 */
CREATE TABLE cass_v150915_0 (
  index_fid TEXT,
  key       BLOB,
  value     BLOB,
  PRIMARY KEY (index_fid, key)
);
/*
 * Currently, only one column family is used in tests.
 * 'cass_v150915' is the prefix used for column family names and
 * is secretly used in idx_cass.c(will change it later to allow applications
 * to specify it.).
 */
CREATE TABLE cass_idx_v150915_0 (
  index_fid TEXT,
  PRIMARY KEY (index_fid)
);
