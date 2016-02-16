#ifndef LOGDB_CONNECTION_H
#define LOGDB_CONNECTION_H

#include "logdb_internal.h"
#include "logdb_log.h"

#include <pthread.h>

/**
 * The magic cookie appearing at byte 0 of the database file.
 */
#define LOGDB_MAGIC "LDBF"

/**
 * Internal struct that represents the header of the database file.
 */
typedef struct {
	char magic[sizeof(LOGDB_MAGIC) - 1]; /* LOGDB_MAGIC */
	unsigned short version;
} logdb_header_t;

/**
 * Internal struct that represents the trailer of the database file.
 */
typedef struct {
	logdb_size_t log_offset; /**< offset from the end of the db where the log starts */
} logdb_trailer_t;

/**
 * Internal struct that holds the state of a LogDB connection.
 */
typedef struct {
	unsigned short version; /**< version of logdb structure. Should equal LOGDB_VERSION */
	pthread_rwlock_t lock; /**< protects threaded access to this `logdb_connection_t` */

	int fd; /**< file descriptor of database file */
	logdb_log_t* log; /**< struct containing fd and metadata about the log file */
	pthread_key_t current_txn_key; /**< tls key for the current transaction for this connection */

} logdb_connection_t;

/**
 * Returns the offset in the file database corresponding to the
 *  given section index.
 */
off_t logdb_connection_offset (logdb_size_t index);

/**
 * Verifies the first arg is a valid `logdb_connection_t`, otherwise returns -1
 * Note this is not meant to be foolproof and should not be passed arbitrary pointers!
 */
#define LOGDB_VERIFY_CONNECTION(var, ...) (logdb_connection* conn_arg_ , ##__VA_ARGS__) { \
	logdb_connection_t* conn_var_; \
	var = conn_var_ = (logdb_connection_t*)(conn_arg_); \
	DBGIF(!conn_var_ || conn_var_->version != LOGDB_VERSION) { \
		LOG("%s: failed-- passed connection was either null, already closed, or incorrect version", __func__); \
		return -1; \
	}

#endif /* LOGDB_CONNECTION_H */