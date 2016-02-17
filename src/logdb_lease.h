#ifndef LOGDB_LEASE_H
#define LOGDB_LEASE_H

#include "logdb_internal.h"
#include "logdb_connection.h"
#include "logdb_log.h"


/**
 * The maximum number of log entries to walk backwards to find
 *  a lease.
 */
#define LOGDB_LEASE_MAX_WALK 4

/**
 * Internal struct that represents a lease on a database section.
 */
typedef struct {
	logdb_connection_t* connection;
	logdb_size_t index; /**< index of the entry in the log that this lease starts on */
	off_t offset; /**< offset inside of the section */
	logdb_size_t len; /**< the number of bytes remaining on the lease */
	logdb_log_lock_type type; /**< type of lock taken, if any */
} logdb_lease_t;

/**
 * Acquires a read lease on the given section of the database.
 * \param lease The destination for the lease object.
 * \param conn Connection on which to acquire the lease.
 * \param index Index of the section on which to acquire the lease.
 * \param offset Offset inside the section at which to acquire the lease.
 * \returns Zero (0) on success.
 */
int logdb_lease_acqire_read (logdb_lease_t* lease, logdb_connection_t* conn, logdb_size_t index, off_t offset);

/**
 * Acquires a write lease on a section of the database that is large enough to write
 *  the given amount of data.
 * \param lease The destination for the lease object.
 * \param conn Connection on which to acquire the lease.
 * \param size Number of bytes to lease.
 * \returns Zero (0) on success.
 */
int logdb_lease_acquire_write (logdb_lease_t* lease, logdb_connection_t* conn, logdb_size_t size);

/**
 * Reads data from the leased region of the database file.
 * \param lease The lease from which to read.
 * \param buf A buffer to hold the data that is read.
 * \param len The number of bytes to read into `buf`.
 * \returns Zero (0) on success. On failure, the amount of data remaining to be read.
 */
size_t logdb_lease_read (logdb_lease_t* lease, void* buf, logdb_size_t len);

/**
 * Writes the given data to the leased region of the database file.
 * \param lease The lease to which to write.
 * \param buf The data to write.
 * \param len The number of bytes in `buf` to write.
 * \returns Zero (0) on success. On failure, the amount of data remaining to be written.
 */
size_t logdb_lease_write (logdb_lease_t* lease, const void* buf, logdb_size_t len);

/**
 * Seeks within the given lease.
 * \param lease The lease within which to seek.
 * \param offset The offset to seek to, relative to the current offset.
 * \returns On success, the resulting offset from the start of the lease. On failure, -1.
 */
off_t logdb_lease_seek (logdb_lease_t* lease, off_t offset);

/**
 * Releases a lease previously acquired with `logdb_lease_acquire`.
 */
void logdb_lease_release (logdb_lease_t* lease);


#endif /* LOGDB_LEASE_H */