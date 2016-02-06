#ifndef LOGDB_LEASE_H
#define LOGDB_LEASE_H

#include "logdb_internal.h"
#include "logdb_connection.h"


/**
 * The maximum number of log entries to walk backwards to find
 *  a lease.
 */
#define LOGDB_LEASE_MAX_WALK 5

/**
 * Internal struct that represents a lease on a database section.
 */
typedef struct {
	logdb_connection_t* connection;
	logdb_size_t index; /**< index of the entry in the log that this lease starts on */
} logdb_lease_t;

/**
 * Acquires a lease on a section of the database.
 * \param lease The destination for the lease object.
 * \param conn Connection on which to acquire the lease.
 * \returns Zero (0) on success.
 */
int logdb_lease_acquire (logdb_lease_t* lease, logdb_connection_t* conn);

/**
 * Writes the given data to the leased region of the database file.
 * \param lease The lease to which to write.
 * \param buf The data to write.
 * \param len The number of bytes in `buf` to write.
 * \returns Zero (0) on success.
 */
int logdb_lease_write (logdb_lease_t* lease, const void* buf, size_t len);

/**
 * Releases a lease previously acquired with `logdb_lease_acquire`.
 */
void logdb_lease_release (logdb_lease_t* lease);


#endif /* LOGDB_LEASE_H */