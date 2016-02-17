#ifndef LOGDB_LOG_H
#define LOGDB_LOG_H

#include "logdb_internal.h"

#include <sys/types.h>

/**
 * The suffix applied to the database file name to derive the
 * log file name.
 */
#define LOGDB_LOG_FILE_SUFFIX "-log"

/**
 * The magic cookie appearing at byte 0 of the log file.
 */
#define LOGDB_LOG_MAGIC "LDBL"

typedef enum {
	LOGDB_LOG_LOCK_NONE,
	LOGDB_LOG_LOCK_READ,
	LOGDB_LOG_LOCK_WRITE
} logdb_log_lock_type;

/**
 * Internal structure representing a reader/writer lock on
 *  entries in the log file.
 *
 * Although we use `fcntl` locks to lock the relevant portions of
 *  the file, these apply across the whole process. Thus, we need
 *  to roll our own to protect multithreaded access.
 */
typedef struct logdb_log_lock_t {
	logdb_size_t startindex;

	/**
	 * A set of locks. Index 0 corresponds to `startindex`, with
	 *  each subsequent index being the next section in the log.
	 *
	 * For each entry, 0 indicates no lock is taken, a positive value
	 *  indicates the number of read locks taken, and -1 indicates a
	 *  write lock is taken.
	 */
	int locks[128];

	/**
	 * If we run out of locks above, we simply append another structure here.
	 */
	volatile struct logdb_log_lock_t* next;
} logdb_log_lock_t;

typedef struct {
	int fd;
	char* path; /* needed to unlink log */
	volatile logdb_log_lock_t* lock;
} logdb_log_t;

typedef struct {
	char magic[sizeof(LOGDB_LOG_MAGIC) - 1]; /* LOGDB_LOG_MAGIC */
	unsigned short version; /* LOGDB_VERSION */

	/* logdb_log_entry_t structs follow until EOF */
} logdb_log_header_t;

/**
 * Internal struct that holds an entry in the log file.
 */
typedef struct {
	unsigned short len; /**< number of bytes that are valid in this section */
} logdb_log_entry_t;

/**
 * Returns the entry index associated with the given offset into the log file.
 */
logdb_size_t logdb_log_index_from_offset (off_t offset);

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
 * Reads the given entry from the log.
 * \param log The log from which to read.
 * \param buf The buffer into which the entry will be read.
 * \param index Zero-based index of the entry to read.
 * \returns -1 on failure, otherwise the offset in the log file of the entry.
 */
off_t logdb_log_read_entry (const logdb_log_t* log, logdb_log_entry_t* buf, logdb_size_t index);

/**
 * Writes the given entry to the log. This entry should be locked.
 * \param log The log from which to read.
 * \param buf The buffer from which the entry will be written.
 * \param index Zero-based index of the entry to write.
 * \returns Zero (0) on success.
 */
int logdb_log_write_entry (logdb_log_t* log, logdb_log_entry_t* buf, logdb_size_t index);

/**
 * Attempts to acquire the given lock on the given entry in the log.
 * \param log The log.
 * \param index The index of the entry to lock.
 * \param type The type of lock to acquire.
 * \returns Zero (0) if the lock was acquired.
 */
int logdb_log_lock (logdb_log_t* log, logdb_size_t index, logdb_log_lock_type type);

/**
 * Unlocks the given lock on the given entry in the log.
 * \param log The log.
 * \param index The index of the entry to unlock.
 * \param type The type of lock to remove.
 */
void logdb_log_unlock (logdb_log_t* log, logdb_size_t index, logdb_log_lock_type type);

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
