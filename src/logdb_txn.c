
#include "logdb_txn.h"

#include <stdlib.h>
#include <pthread.h>

logdb_txn_t* logdb_txn_current (logdb_connection_t* conn)
{
	return (logdb_txn_t*)pthread_getspecific (conn->current_txn_key);
}

static int logdb_txn_set_current (logdb_connection_t* conn, logdb_txn_t* txn)
{
	return pthread_setspecific (conn->current_txn_key, txn);
}

logdb_txn_t* logdb_txn_begin_implicit (logdb_connection_t* conn)
{
	logdb_txn_t* txn = calloc (1, sizeof (logdb_txn_t));
	if (!txn) {
		ELOG ("logdb_txn_begin_implicit: calloc");
		return NULL;
	}

	txn->outer = logdb_txn_current (conn);
	return txn;
}

int logdb_txn_commit (logdb_txn_t* txn)
{
	/* If there is no data associated with this transaction,
	    then there is nothing to do! */
	if (!(txn->buf) || txn->len == 0)
		return 0;

	return -1;
}

static void logdb_txn_close (logdb_txn_t* txn, logdb_connection_t* conn)
{
	/* FIXME: We should close the txn lease if there is one */
	if (txn->buf)
		free (txn->buf);
	if (conn)
		logdb_txn_set_current (conn, txn->outer);
	else if (txn->outer)
		logdb_txn_close (txn->outer, NULL);
	free (txn);
}

int logdb_begin LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn)
{
	logdb_txn_t* txn = logdb_txn_begin_implicit (conn);
	return logdb_txn_set_current (conn, txn);
}}

int logdb_commit LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn)
{
	logdb_txn_t* txn = logdb_txn_current (conn);
	if (!txn)
		return -1;

	int err = logdb_txn_commit (txn);
	if (err)
		return err;

	logdb_txn_close (txn, conn);
	return 0;
}}

int logdb_rollback LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn)
{
	logdb_txn_t* txn = logdb_txn_current (conn);
	if (!txn)
		return -1;

	logdb_txn_close (txn, conn);
	return 0;
}}

void logdb_txn_destruct (void* current_txn)
{
	/* If there is a current transaction active when a thread
	    exits, rollback the transaction. */
	logdb_txn_t* txn = (logdb_txn_t*)current_txn;
	if (txn)
		logdb_txn_close (txn, NULL);
}
