#ifndef LOGDB_H
#define LOGDB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOGDB_API
#define LOGDB_API
#endif

/* CONNECTIONS */

/** An opaque data structure representing a LogDB connection. */
typedef void logdb_connection;

/** Flags that can be passed to `logdb_open`. Can be combined with bitwise OR. */
typedef enum {
	/** Open an existing file */
	LOGDB_OPEN_EXISTING = 0,

	/** Create the file if it doesn't exist */
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

/* BUFFERS */

/** An opaque data structure representing a LogDB data buffer. */
typedef void logdb_buffer;

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
LOGDB_API logdb_buffer* logdb_buffer_new_direct (void* data, size_t length, dispose_func disposer);

/**
 * Allocates a new buffer with a copy of the given data.
 *  The newly allocated buffer will have a reference count of one (1).
 * \param data The data to be copied into the new buffer. This pointer only has to remain valid for the duration of this function call.
 * \param length The length of the data pointed to by the `data` pointer.
 * \returns A pointer to the new buffer object, or NULL on failure.
 */
LOGDB_API logdb_buffer* logdb_buffer_new_copy (void* data, size_t length);

/**
 * Returns the total length of all data pointed to by this buffer.
 * \returns Total length, or 0 if `buffer` is NULL.
 */
LOGDB_API size_t logdb_buffer_length (const logdb_buffer* buffer);

/**
 * Appends the second buffer to the first buffer.
 *
 *  Buffers form a linked list, so any buffer further appended to either
 *  `buffer1` or `buffer2` will come at the end of the entire chain.
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
