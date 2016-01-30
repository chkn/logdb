#ifndef LOGDB_LOG_H
#define LOGDB_LOG_H

#include "logdb_internal.h"

/**
 * The suffix applied to the database file name to derive the
 * log file name.
 */
#define LOGDB_LOG_FILE_SUFFIX "-log"

/**
 * The magic cookie appearing at byte 0 of the log file.
 */
#define LOGDB_LOG_MAGIC "LDBL"

typedef struct {
	int fd;
	char* path; /* needed to unlink log */
	unsigned int len; /**< number of entries in the log */

	/**
	 * A bitfield lock, with each bit representing an entry of the
	 * log. If the bit is set to 1, that section is locked by some
	 * thread in this process.
	 */
	volatile void* lock;
} logdb_log_t;

typedef struct {
	char magic[sizeof(LOGDB_LOG_MAGIC) - 1]; /* LOGDB_LOG_MAGIC */
	unsigned short version; /* LOGDB_VERSION */
	unsigned int len; /**< number of entries in the log */

	/* len logdb_log_entry_t structs follow */
} logdb_log_header_t;

/**
 * Internal struct that holds an entry in the log file.
 */
typedef struct {
	unsigned short len; /**< number of bytes that are valid in this section */
} logdb_log_entry_t;

/**
 * Opens the log file at the given path.
 * \returns The log, or NULL if it could not be opened.
 */
logdb_log_t* logdb_log_open (const char* path);

/**
 * Creates an log file for the database file open on the given fd.
 * \param path The path at which to create the log.
 * \param dbfd The file descriptor for the database for which to create the log.
 * \returns The log, or NULL if it could not be created.
 */
logdb_log_t* logdb_log_create (const char* path, int dbfd);

/**
 * Closes the given log.
 * \returns Zero (0) on success.
 */
int logdb_log_close (logdb_log_t* log);

/**
 * Closes the given log, merging it back into the database file
 *  open on the given fd.
 * \returns Zero (0) on success.
 */
int logdb_log_close_merge (logdb_log_t* log, int dbfd);

#endif /* LOGDB_LOG_H */
