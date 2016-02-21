
#include "logdb_txn.h"
#include "logdb_data.h"
#include "logdb_lease.h"

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

static logdb_txn_t* logdb_txn_current (logdb_connection_t* conn)
{
	return (logdb_txn_t*)pthread_getspecific (conn->current_txn_key);
}

static int logdb_txn_set_current (logdb_connection_t* conn, logdb_txn_t* txn)
{
	return pthread_setspecific (conn->current_txn_key, txn);
}

static int logdb_txn_write_buf (logdb_lease_t* lease, logdb_buffer_t* buf)
{
	do {
		if (buf->orig) {
			if (logdb_txn_write_buf (lease, buf->orig) != 0)
				return -1;
		} else {
			if (logdb_lease_write (lease, buf->data, buf->len) != 0)
				return -1;
		}
		buf = buf->next;
	} while (buf);
	return 0;
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

/**
 * Closes the given transaction and frees its resources.
 * \param conn The associated database connection. May be NULL, in which case all outer transactions are also closed.
 */
static void logdb_txn_close (logdb_connection_t* conn, logdb_txn_t* txn)
{
	if (conn) {
		logdb_txn_set_current (conn, txn->outer);
	} else if (txn->outer) {
		logdb_txn_close (NULL, txn->outer);
	}
	if (txn->buf)
		logdb_buffer_free (txn->buf);
	free (txn);
}

static int logdb_txn_commit (logdb_connection_t* conn, logdb_txn_t* txn)
{
	/* Determine how much data we have to write */
	size_t len = logdb_buffer_length (txn->buf);
	if (len == 0)
		goto closereturn;

	/* If this is not the outer transaction, then merge our data
	    into the outer transaction */
	if (txn->outer) {
		if (txn->buf)
			txn->outer->buf = logdb_buffer_append (txn->outer->buf, txn->buf);
		goto closereturn;
	}

	/* Acquire a lease to write this data */
	logdb_lease_t lease;
	if (logdb_lease_acquire_write (&lease, conn, len) != 0)
		return -1;

	/* Write the data */
	if (logdb_txn_write_buf (&lease, txn->buf) != 0) {
		logdb_lease_release (&lease);
		return -1;
	}

	bool durable = (conn->flags & LOGDB_OPEN_NOSYNC) != LOGDB_OPEN_NOSYNC;
	if (durable && (fsync (conn->fd) == -1)) {
		ELOG("logdb_txn_commit: fsync 1");
		logdb_lease_release (&lease);
		return -1;
	}

	/* Update the log */
	logdb_log_entry_t entry;
	entry.len = lease.offset;
	if (logdb_log_write_entry (conn->log, &entry, lease.index) != 0) {
		/* FIXME: There *might* be a slim chance we've corrupted the log here. */
		logdb_lease_release (&lease);
		return -1;
	}

	/* I don't think there's really much we can do if this fails? */
	if (durable)
		(void)fsync (conn->log->fd);

	/* Release the lease */
	logdb_lease_release (&lease);
closereturn:
	logdb_txn_close (conn, txn);
	return 0;
}

int logdb_txn_commit_implicit (logdb_connection_t* conn, logdb_txn_t* txn)
{
	int result = logdb_txn_commit (conn, txn);

	/* Since this is an implicit transaction, we must close it even if
	    the commit fails.
	*/
	if (result != 0)
		logdb_txn_close (conn, txn);

	return result;
}

int logdb_begin LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn)
{
	logdb_txn_t* txn = logdb_txn_begin_implicit (conn);
	return txn? logdb_txn_set_current (conn, txn) : -1;
}}

int logdb_put LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn, logdb_buffer* key, logdb_buffer* value)
{
	DBGIF(!key || !value) {
		LOG("logdb_put: key or value was null");
		return -1;
	}

	/* Create record header */
	logdb_data_header_t header;
	header.keylen = logdb_buffer_length (key);
	header.valuelen = logdb_buffer_length (value);

	/* Create buffer for record header */
	logdb_buffer* headerbuf = logdb_buffer_new_copy (&header, sizeof (header));
	if (!headerbuf) {
		LOG("logdb_put: logdb_buffer_new_copy failed");
		return -1;
	}

	/* Create an implicit transaction for this put */
	logdb_txn_t* txn = logdb_txn_begin_implicit (conn);
	if (!txn) {
		LOG("logdb_put: logdb_txn_begin_implicit failed");
		logdb_buffer_free (headerbuf);
		return -1;
	}

	/* Commit the write */
	txn->buf = logdb_buffer_append (logdb_buffer_append (headerbuf, key), value);
	return logdb_txn_commit_implicit (conn, txn);
}}

int logdb_commit LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn)
{
	logdb_txn_t* txn = logdb_txn_current (conn);
	if (!txn)
		return -1;

	return logdb_txn_commit (conn, txn);
}}

int logdb_rollback LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn)
{
	logdb_txn_t* txn = logdb_txn_current (conn);
	if (!txn)
		return -1;

	logdb_txn_close (conn, txn);
	return 0;
}}

void logdb_txn_rollback_all (logdb_connection_t* conn)
{
	logdb_txn_destruct (logdb_txn_current (conn));
}

void logdb_txn_destruct (void* current_txn)
{
	/* If there is a current transaction active when a thread
	    exits, rollback the transaction. */
	logdb_txn_t* txn = (logdb_txn_t*)current_txn;
	if (txn)
		logdb_txn_close (NULL, txn);
}
