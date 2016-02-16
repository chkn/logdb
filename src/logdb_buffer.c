
#include "logdb_buffer.h"

#include <stdlib.h>
#include <string.h>
#include <libkern/OSAtomic.h>

/**
 * Creates an unowned copy of the given buffer
 *  (e.g. `disposer` will be null in the copy regardless of its value in the given buffer).
 * The reference count of the returned copy will be 1.
 * \returns The copy, or NULL on failure.
 */
static logdb_buffer_t* logdb_buffer_copy (logdb_buffer_t* buf)
{
	logdb_buffer_t* result = (logdb_buffer_t*)logdb_buffer_new_direct (buf->data, buf->len, NULL);
	if (result) {
		logdb_buffer_retain (buf);
		result->orig = buf;
	}
	return result;
}

logdb_buffer* logdb_buffer_new_direct (void* data, logdb_size_t length, dispose_func disposer)
{
	DBGIF(!data) {
		LOG("logdb_buffer_new_direct: data is NULL");
		return NULL;
	}

	logdb_buffer_t* buf = malloc (sizeof (logdb_buffer_t));
	if (!buf) {
		ELOG("logdb_buffer_new_direct: malloc");
		return NULL;
	}

	buf->data = data;
	buf->len = length;
	buf->disposer = disposer;
	buf->orig = NULL;
	buf->next = NULL;
	buf->refcnt = 1;
	return buf;
}

logdb_buffer* logdb_buffer_new_copy (void* data, logdb_size_t length)
{
	DBGIF(!data) {
		LOG("logdb_buffer_new_copy: data is NULL");
		return NULL;
	}

	void* copy = malloc (length);
	if (!copy) {
		ELOG("logdb_buffer_new_copy: malloc");
		return NULL;
	}

	logdb_buffer* result = logdb_buffer_new_direct (memcpy (copy, data, length), length, &free);
	if (!result)
		free (copy);
	return result;
}

logdb_size_t logdb_buffer_length (const logdb_buffer* buffer)
{
	logdb_size_t len = 0;
	logdb_buffer_t* buf = (logdb_buffer_t*)buffer;
	while (buf) {
		len += (buf->orig)? logdb_buffer_length (buf->orig) : buf->len;
		buf = buf->next;
	}
	return len;
}

const void* logdb_buffer_data (const logdb_buffer* buffer)
{
	logdb_buffer_t* buf = (logdb_buffer_t*)buffer;
	DBGIF(!buf) {
		LOG("logdb_buffer_data: passed buffer was null");
		return NULL;
	}
	/* If we're just a shallow copy, return the orignal's data */
	if (buf->orig)
		return logdb_buffer_data (buf->orig);

	/* If we have a linked list of data, we'll need to make a copy */
	if (buf->next) {
		logdb_size_t newlen = logdb_buffer_length (buffer);
		void* newdata = malloc (newlen);
		if (!newdata) {
			ELOG("logdb_buffer_data: malloc");
			return NULL;
		}

		char* dataptr = (char*)newdata;
		logdb_buffer_t* cur = buf;
		do {
			if (cur->orig) {
				logdb_size_t len = logdb_buffer_length (cur->orig);
				const void* data = logdb_buffer_data (cur->orig);
				if (!data)
					return NULL;
				(void)memcpy (dataptr, data, len);
				dataptr += len;
			} else {
				(void)memcpy (dataptr, cur->data, cur->len);
				dataptr += cur->len;
			}
			cur = cur->next;
		} while (cur);

		/* Dispose the old data and use our new data instead */
		if (buf->disposer) {
			buf->disposer (buf->data);
			buf->disposer = NULL;
		}
		buf->data = newdata;
		buf->len = newlen;
		buf->next = NULL;
	}
	return buf->data;
}

logdb_buffer* logdb_buffer_append (logdb_buffer* buffer1, logdb_buffer* buffer2)
{
	DBGIF(!buffer2 || (buffer1 == buffer2)) {
		LOG("logdb_buffer_append: buffer2 is NULL or equal to buffer1");
		return NULL;
	}
	if (!buffer1)
		return buffer2;

	/* Check if the resulting buffer overflows logdb_size_t */
	if ((((logdb_size_t)~0) - logdb_buffer_length (buffer1)) < logdb_buffer_length (buffer2)) {
		LOG("logdb_buffer_append: overflow");
		return NULL;
	}

	logdb_buffer_t* buf = (logdb_buffer_t*)buffer1;
	while (buf->next) {
		buf = buf->next;
		if (buf == buffer2)
			return NULL;
	}

	buf->next = logdb_buffer_copy ((logdb_buffer_t*)buffer2);
	return buffer1;
}

void logdb_buffer_retain (logdb_buffer* buffer)
{
	logdb_buffer_t* buf = (logdb_buffer_t*)buffer;
	OSAtomicIncrement32Barrier (&buf->refcnt);
}

void logdb_buffer_free (logdb_buffer* buffer)
{
	logdb_buffer_t* buf = (logdb_buffer_t*)buffer;
	if (!buf || OSAtomicDecrement32Barrier (&buf->refcnt))
		return;
	if (buf->disposer)
		buf->disposer (buf->data);
	if (buf->orig)
		logdb_buffer_free (buf->orig);
	if (buf->next)
		logdb_buffer_free (buf->next);
	free (buf);
}