#ifndef LOGDB_H
#define LOGDB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOGDB_API
#define LOGDB_API
#endif

/**
 * The type used for offsets and sizes in the logdb format.
 */
typedef unsigned int logdb_size_t;

/* CONNECTIONS */

/** An opaque data structure representing a LogDB connection. */
typedef void logdb_connection;

/** Flags that can be passed to `logdb_open`. Can be combined with bitwise OR. */
typedef enum {
	/**
	 * Open an existing file.
	 *  If `LOGDB_OPEN_CREATE` is not specified, `logdb_open` will fail if the existing
	 *  file is not a valid LogDB database.
	 */
	LOGDB_OPEN_EXISTING = 0,

	/**
	 * Create the file if it doesn't exist.
	 *  If a file does exist but it is not a valid LogDB database, it is overwritten.
	 */
	LOGDB_OPEN_CREATE = 1
} logdb_open_flags;

/**
 * Opens a connection to a LogDB database file.
 *  
 * Multiple processes can open a single database file at once, and multiple threads can share
 *  a single `logdb_connection` without external locking. However, opening multiple
 * `logdb_connection`s on the same database file within a single process is not supported.
 * \returns A pointer to the connection data structure, or NULL on failure.
 */
LOGDB_API logdb_connection* logdb_open (const char* path, logdb_open_flags flags);

/**
 * Closes a connection previously opened with `logdb_open` and frees the memory associated with it.
 *
 *  You must ensure that other threads do not attempt to use the connection after it is closed. Any open transactions
 *   *on the thread calling `logdb_close`* will be rolled back. If there are open transactions on other threads, they
 *   will be leaked-- any attempt to commit or roll them back after calling `logdb_close` will result in undefined behavior.
 *
 * \param connection The connection to close. If this call completes successfully, `connection` will no longer be valid.
 * \returns Zero (0) on success.
 */
LOGDB_API int logdb_close (logdb_connection* connection);

/* ITERATORS */

/** An opaque data structure representing a data buffer (see BUFFERS section below). */
typedef void logdb_buffer;

/** An opaque data structure representing an iterator */
typedef void logdb_iter;

/**
 * Creates a new iterator to iterate over all records in the given connection.
 *  The iterator starts before the first record; a call to `logdb_iter_next`
 *  is required to advance to the first record.
 * \returns The new iterator, or NULL on failure.
 */
LOGDB_API logdb_iter* logdb_iter_all (logdb_connection* connection);

/**
 * Advances the iterator to the next record.
 * \returns One (1) on success, or zero (0) on failure (e.g. there are no more records)
 */
LOGDB_API int logdb_iter_next (logdb_iter* iter);

/**
 * Returns the current key pointed to by this iterator.
 *
 * \returns A buffer with the key, or NULL on failure.
 * The buffer returned by this function may be freed by the next call
 *  to `logdb_iter_next` or `logdb_iter_free`. If you wish to retain the
 *  buffer past that point, you must call `logdb_buffer_retain` (with a
 *  matching call to `logdb_buffer_free`).
 */
LOGDB_API logdb_buffer* logdb_iter_current_key (logdb_iter* iter);

/**
 * Returns the current value pointed to by this iterator.
 *
 * \returns A buffer with the value, or NULL on failure.
 * The buffer returned by this function may be freed by the next call
 *  to `logdb_iter_next` or `logdb_iter_free`. If you wish to retain the
 *  buffer past that point, you must call `logdb_buffer_retain` (with a
 *  matching call to `logdb_buffer_free`).
 */
LOGDB_API logdb_buffer* logdb_iter_current_value (logdb_iter* iter);

/**
 * Frees the resources used by the given iterator and deallocates it.
 */
LOGDB_API void logdb_iter_free (logdb_iter* iter);

/* BUFFERS */

/** A function pointer type representing a function to dispose a pointer. */
typedef void (*dispose_func)(void*);

/**
 * Allocates a new buffer that points to the given data.
 *  The newly allocated buffer will have a reference count of one (1).
 * \param data The data to which this buffer should point. This pointer must remain valid until the disposer is called.
 * \param length The length of the data pointed to by the `data` pointer.
 * \param disposer A function that will be called to free the data pointed to by the `data` pointer, or NULL.
 * \returns A pointer to the new buffer object, or NULL on failure.
 */
LOGDB_API logdb_buffer* logdb_buffer_new_direct (void* data, logdb_size_t length, dispose_func disposer);

/**
 * Allocates a new buffer with a copy of the given data.
 *  The newly allocated buffer will have a reference count of one (1).
 * \param data The data to be copied into the new buffer. This pointer only has to remain valid for the duration of this function call.
 * \param length The length of the data pointed to by the `data` pointer.
 * \returns A pointer to the new buffer object, or NULL on failure.
 */
LOGDB_API logdb_buffer* logdb_buffer_new_copy (void* data, logdb_size_t length);

/**
 * Returns the total length of all data pointed to by this buffer.
 * \returns Total length, or 0 if `buffer` is NULL.
 */
LOGDB_API logdb_size_t logdb_buffer_length (const logdb_buffer* buffer);

/**
 * Returns a pointer to the data contained in this buffer.
 *  The data must not be modified through this pointer!
 * \returns A pointer to the data, or NULL on failure.
 */
LOGDB_API const void* logdb_buffer_data (const logdb_buffer* buffer);

/**
 * Appends the second buffer to the first buffer.
 * \param buffer1 The first buffer.
 * \param buffer2 The second buffer.
 * \returns NULL on failure. On success, `buffer1`, unless it was NULL, then `buffer2`
 */
LOGDB_API logdb_buffer* logdb_buffer_append (logdb_buffer* buffer1, logdb_buffer* buffer2);

/**
 * Atomically increments the reference count on the given buffer.
 */
LOGDB_API void logdb_buffer_retain (logdb_buffer* buffer);

/**
 * Atomically decrements the reference count of the given buffer.
 *  If the reference count is zero (0), disposes of all its resources and frees its memory.
 */
LOGDB_API void logdb_buffer_free (logdb_buffer* buffer);

/* TRANSACTIONS */

/**
 * Begins a transaction on the given connection for the current thread.
 * 
 *  Transactions can be nested by calling `logdb_begin` while there is already
 *  an active transaction on the given connection for the current thread. No data
 *  will be written to the database until the outermost transaction is committed.
 *  If a nested transaction is rolled back, its changes will not be included when
 *  the outer transaction(s) are committed.
 * \returns Zero (0) on success.
 */
LOGDB_API int logdb_begin (logdb_connection* connection);

/**
 * Writes the given key-value record to the database.
 *  The same key may have multiple values.
 * \returns Zero (0) on success.
 */
LOGDB_API int logdb_put (logdb_connection* connection, logdb_buffer* key, logdb_buffer* value);

/**
 * Commits the current transaction on the given connection for the current thread.
 *
 *  If there is no active transaction on the given connection for the current thread,
 *   this call fails.
 *  If there are no active nested transactions on the given connection for the current thread,
 *   this call causes data to be written to the database.
 *
 *  If this function fails, the current transaction is left uncommitted. Either retry the commit
 *   by calling this function again, or call `logdb_rollback`.
 * \returns Zero (0) on success.
 */
LOGDB_API int logdb_commit (logdb_connection* connection);

/**
 * Rolls back the current transaction on the given connection for the current thread.
 *
 *  If there is no active transaction on the given connection for the current thread,
 *   this call fails. This is the only failure mode for this function.
 * \returns Zero (0) on success.
 */
LOGDB_API int logdb_rollback (logdb_connection* connection);

#ifdef __cplusplus
}
#endif
#endif /* LOGDB_H */
