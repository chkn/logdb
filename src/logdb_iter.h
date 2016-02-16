#ifndef LOGDB_ITER_H
#define LOGDB_ITER_H

#include "logdb_internal.h"
#include "logdb_connection.h"
#include "logdb_buffer.h"
#include "logdb_lease.h"
#include "logdb_data.h"

typedef struct {
	logdb_connection_t* connection;
	logdb_lease_t lease;

	logdb_data_header_t record; /**< header for current record */
	logdb_buffer_t* key;
	logdb_buffer_t* value;
} logdb_iter_t;

/**
 * Verifies the first arg is a valid `logdb_iter_t`, otherwise returns 0
 * Note this is not meant to be foolproof and should not be passed arbitrary pointers!
 */
#define LOGDB_VERIFY_ITER(var, ...) (logdb_iter* iter_arg_ , ##__VA_ARGS__) { \
	logdb_iter_t* iter_var_; \
	var = iter_var_ = (logdb_iter_t*)(iter_arg_); \
	DBGIF(!iter_var_ || !(iter_var_->connection) || (iter_var_->connection->version != LOGDB_VERSION)) { \
		LOG("%s: failed-- passed iterator was either null, freed, or invalid", __func__); \
		return 0; \
	}

#endif /* LOGDB_ITER_H */