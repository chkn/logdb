#ifndef LOGDB_TXN_H
#define LOGDB_TXN_H

#include "logdb_internal.h"
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
 * If the passed pointer is not NULL, cause the transaction to which it refers
 *  to roll back.
 */
void logdb_txn_destruct (void* current_txn);

#endif /* LOGDB_TXN_H */