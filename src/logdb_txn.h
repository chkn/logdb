#ifndef LOGDB_TXN_H
#define LOGDB_TXN_H

#include "logdb_internal.h"
#include "logdb_connection.h"
#include "logdb_lease.h"

/**
 * Internal structure that represents a transaction
 */
typedef struct logdb_txn_t {
	struct logdb_txn_t* outer; /**< outer transaction for this thread, or null */
	logdb_lease_t* lease; /**< lease obtained by this transaction, or null */

	void* buf; /**< the data added by this transaction */
	size_t len; /**< length of data in `buf` */
} logdb_txn_t;

/**
 * Begins a new implicit transaction.
 *  The returned transaction is not set in tls, so it must
 *  be committed with `logdb_txn_commit` before control
 *  returns to user code.
 * \returns the transaction, or NULL on failure.
 */
logdb_txn_t* logdb_txn_begin_implicit (logdb_connection_t* conn);

/** Returns the current transaction in tls or null */
logdb_txn_t* logdb_txn_current (logdb_connection_t* conn);

/**
 * Commits the given transaction.
 * \returns Zero (0) on success.
 */
int logdb_txn_commit (logdb_txn_t* txn);

/**
 * If the passed pointer is not NULL, causes the transaction to which it refers
 *  to roll back.
 */
void logdb_txn_destruct (void* current_txn);

#endif /* LOGDB_TXN_H */