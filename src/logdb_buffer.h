#ifndef LOGDB_BUFFER_H
#define LOGDB_BUFFER_H

#include "logdb_internal.h"

/**
 * Internal structure that is a buffer for data.
 */
typedef struct logdb_buffer_t {
	void* data;
	size_t len;
	dispose_func disposer; /**< disposer for data ptr, or null */
	struct logdb_buffer_t* next; /**< next buffer in the linked list, or null */
	volatile int refcnt; /**< reference count */
} logdb_buffer_t;

#endif /* LOGDB_BUFFER_H */