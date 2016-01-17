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

/** Flags that can be passed to logdb_open */
typedef enum {
	/** Open an existing file */
	LOGDB_OPEN_EXISTING = 0,

	/** Create the file if it doesn't exist */
	LOGDB_OPEN_CREATE = 1,

	/** Truncate the file to 0 */
	LOGDB_OPEN_TRUNCATE = 2,
} logdb_open_flags;

/**
 * Opens a new connection to a LogDB database file.
 * \param vmmax Specifies the maxmimum amount of memory to map. Specify a value <= 0 for no limit.
 * \returns A pointer to the connection data structure, or NULL on failure.
 */
LOGDB_API logdb_connection* logdb_open (const char* path, logdb_open_flags flags, int vmmax);

/**
 * Closes a connection previously opened with `logdb_open`.
 * \returns Zero (0) on success.
 */
LOGDB_API int logdb_close (logdb_connection* connection);

#ifdef __cplusplus
}
#endif
#endif /* LOGDB_H */
