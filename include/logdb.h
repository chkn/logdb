#ifndef LOGDB_H
#define LOGDB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOGDB_API
#define LOGDB_API
#endif

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
 * Opens a new connection to a LogDB database file. Multiple processes can
 *  work with a single database file at once, and multiple threads can share
 *  a single `logdb_connection` without external locking. However, using multiple
 * `logdb_connection`s on the same database file from different threads is not supported.
 * \returns A pointer to the connection data structure, or NULL on failure.
 */
LOGDB_API logdb_connection* logdb_open (const char* path, logdb_open_flags flags);

/**
 * Closes a connection previously opened with `logdb_open` and frees the memory associated with it.
 * \param connection The connection to close. If this call completes successfully, `connection` will no longer be valid.
 * \returns Zero (0) on success.
 */
LOGDB_API int logdb_close (logdb_connection* connection);

#ifdef __cplusplus
}
#endif
#endif /* LOGDB_H */
