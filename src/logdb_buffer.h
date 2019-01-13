#ifndef LOGDB_BUFFER_H
#define LOGDB_BUFFER_H

#include "logdb_internal.h"

#include <stdatomic.h>

/**
 * Internal structure that is a buffer for data.
 */
typedef struct logdb_buffer_t {
	void* data;
	logdb_size_t len;
	dispose_func disposer; /**< disposer for data ptr, or null */
	struct logdb_buffer_t* orig; /**< if this is a copy, the original buffer, or null */
	struct logdb_buffer_t* next; /**< next buffer in the linked list, or null */
	volatile atomic_int refcnt; /**< reference count */
} logdb_buffer_t;

#endif /* LOGDB_BUFFER_H */