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

/**
 * Internal structure representing a reader/writer lock on
 *  entries in the log file.
 *
 * Although we use `fcntl` locks to lock the relevant portions of
 *  the file, these apply across the whole process. Thus, we need
 *  to roll our own to protect multithreaded access.
 */
typedef struct logdb_log_lock_t {
	/**
	 * A set of locks. Index 0 corresponds to the last index entry,
	 *  counting up corresponding to earlier index entries.
	 *
	 * For each entry, 0 indicates no lock is taken, a positive value
	 *  indicates the number of read locks taken, and -1 indicates a
	 *  write lock is taken.
	 */
	int locks[256];

	/**
	 * If we run out of locks above, we simply append another structure here.
	 */
	struct logdb_log_lock_t* next;
} logdb_log_lock_t;

typedef struct {
	int fd;
	char* path; /* needed to unlink log */
	volatile logdb_log_lock_t* lock;
} logdb_log_t;

typedef struct {
	char magic[sizeof(LOGDB_LOG_MAGIC) - 1]; /* LOGDB_LOG_MAGIC */
	unsigned short version; /* LOGDB_VERSION */
	logdb_size_t len; /**< number of entries in the log */

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
