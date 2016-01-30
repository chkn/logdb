#ifndef LOGDB_TXN_H
#define LOGDB_TXN_H

#include "logdb_internal.h"
#include "logdb_connection.h"
#include "logdb_buffer.h"

/**
 * Internal structure that represents a transaction
 */
typedef struct logdb_txn_t {
	struct logdb_txn_t* outer; /**< outer transaction for this thread, or null */
	logdb_buffer_t* buf; /**< the data added by this transaction, or null */
} logdb_txn_t;

/**
 * Begins a new implicit transaction.
 *  The returned transaction is not set in tls, so it must
 *  be committed with `logdb_txn_commit_implicit` before control
 *  returns to user code.
 * \returns the transaction, or NULL on failure.
 */
logdb_txn_t* logdb_txn_begin_implicit (logdb_connection_t* conn);

/**
 * Commits the given implicit transaction.
 *  Unlike `logdb_commit`, this function never leaves the transaction open.
 *  If the commit fails, the transaction is rolled back.
 * \returns Zero (0) on success.
 */
int logdb_txn_commit_implicit (logdb_connection_t* conn, logdb_txn_t* txn);

/**
 * Rolls back all open transactions on the current thread.
 */
void logdb_txn_rollback_all (logdb_connection_t* conn);

/**
 * If the passed pointer is not NULL, causes the transaction to which it refers
 *  to roll back. Any outer transactions are also rolled back.
 */
void logdb_txn_destruct (void* current_txn);

#endif /* LOGDB_TXN_H */