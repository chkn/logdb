
#include "logdb_buffer.h"

#include <stdlib.h>
#include <string.h>
#include <libkern/OSAtomic.h>

logdb_buffer* logdb_buffer_new_direct (void* data, size_t length, dispose_func disposer)
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
	buf->next = NULL;
	buf->refcnt = 1;
	return buf;
}

logdb_buffer* logdb_buffer_new_copy (void* data, size_t length)
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

size_t logdb_buffer_length (const logdb_buffer* buffer)
{
	size_t len = 0;
	logdb_buffer_t* buf = (logdb_buffer_t*)buffer;
	while (buf) {
		len += buf->len;
		buf = buf->next;
	}
	return len;
}

logdb_buffer* logdb_buffer_append (logdb_buffer* buffer1, logdb_buffer* buffer2)
{
	if (!buffer2 || (buffer1 == buffer2))
		return NULL;
	if (!buffer1)
		return buffer2;

	logdb_buffer_t* buf = (logdb_buffer_t*)buffer1;
	while (buf->next) {
		buf = buf->next;
		if (buf == buffer2)
			return NULL;
	}

	logdb_buffer_retain (buffer2);
	buf->next = (logdb_buffer_t*)buffer2;

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
	if (buf->next)
		logdb_buffer_free (buf->next);
	free (buf);
}