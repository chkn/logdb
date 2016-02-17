
#include "logdb_iter.h"

static logdb_buffer_t* logdb_iter_read_buf (logdb_iter_t* iter, logdb_size_t len)
{
	void* buf = malloc (len);
	if (!buf) {
		ELOG("logdb_iter_read_buf: malloc");
		return NULL;
	}

	if (logdb_lease_read (&iter->lease, buf, len) != 0)
		return NULL;

	return logdb_buffer_new_direct (buf, len, &free);
}

logdb_iter* logdb_iter_all (logdb_connection* connection)
{
	logdb_connection_t* conn = (logdb_connection_t*)connection;
	DBGIF(!conn || (conn->version != LOGDB_VERSION)) {
		LOG("logdb_iter_all: failed-- passed connection was either null, already closed, or incorrect version");
		return NULL;
	}

	logdb_iter_t* iter = calloc (1, sizeof (logdb_iter_t));
	if (!iter) {
		ELOG("logdb_iter_all: calloc");
		return NULL;
	}

	iter->connection = conn;
	return iter;
}

int logdb_iter_next LOGDB_VERIFY_ITER(logdb_iter_t* iter)
{
	if (iter->lease.len < sizeof (logdb_data_header_t)) {
		/* No more data left on our current lease-- find the next one */
		logdb_size_t index = 0;
		if (iter->lease.connection) {
			index = iter->lease.index + 1;
			logdb_lease_release (&iter->lease);
		}
		logdb_log_entry_t entry;
		while (1) {
			if (logdb_log_read_entry (iter->connection->log, &entry, index) == -1)
				return 0;
			if (entry.len)
				break;
			else
				index++;
		}
		
		/* Take a lease to read the record header for the next index */
		if (logdb_lease_acqire_read (&iter->lease, iter->connection, index, 0) != 0)
			return 0;
	} else {
		/* Skip past the key and/or value if they weren't read */
		if (!(iter->key) && (logdb_lease_seek (&iter->lease, iter->record.keylen) == -1))
			return 0;
		if (!(iter->value) && (logdb_lease_seek (&iter->lease, iter->record.valuelen) == -1))
			return 0;
	}

	/* Dispose the old key/value if they had been read */
	if (iter->key) {
		logdb_buffer_free (iter->key);
		iter->key = NULL;
	}
	if (iter->value) {
		logdb_buffer_free (iter->value);
		iter->value = NULL;
	}

	/* Read the next record in our lease */
	if (logdb_lease_read (&iter->lease, &iter->record, sizeof (logdb_data_header_t)) != 0)
		return 0;

	return 1;
}}

logdb_buffer* logdb_iter_current_key LOGDB_VERIFY_ITER(logdb_iter_t* iter)
{
	if (!(iter->key)) {
		DBGIF(!(iter->lease.connection)) {
			LOG("logdb_iter_current_key: failed-- you must call `logdb_iter_next` first");
			return NULL;
		}

		iter->key = logdb_iter_read_buf (iter, iter->record.keylen);
	}
	return iter->key;
}}

logdb_buffer* logdb_iter_current_value LOGDB_VERIFY_ITER(logdb_iter_t* iter)
{
	if (!(iter->value)) {
		DBGIF(!(iter->lease.connection)) {
			LOG("logdb_iter_current_value: failed-- you must call `logdb_iter_next` first");
			return NULL;
		}

		/* FIXME: It would be nice to not have to read the key to get the value */
		if (!(iter->key))
			(void)logdb_iter_current_key (iter);

		iter->value = logdb_iter_read_buf (iter, iter->record.valuelen);
	}
	return iter->value;
}}

void logdb_iter_free (logdb_iter* iterator)
{
	logdb_iter_t* iter = (logdb_iter_t*)iterator;
	DBGIF(!iter || !(iter->connection)) {
		LOG("logdb_iter_free: passed invalid iterator");
		return;
	}
	if (iter->key)
		logdb_buffer_free (iter->key);
	if (iter->value)
		logdb_buffer_free (iter->value);
	if (iter->lease.connection)
		logdb_lease_release (&iter->lease);
	iter->connection = NULL;
	free (iterator);
}