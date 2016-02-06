#include "logdb_lease.h"
#include "logdb_log.h"

#include <pthread.h>
#include <string.h>

int logdb_lease_acquire (logdb_lease_t* lease, logdb_connection_t* conn)
{
	DBGIF(!lease || !conn) {
		LOG("logdb_lease_acquire: failed-- lease or conn was NULL");
		return -1;
	}

	lease->connection = NULL;

	/* Obtain shared lock on connection to prevent other threads from closing it on us */
	int err = pthread_rwlock_rdlock (&conn->lock);
	if (err) {
		LOG("logdb_lease_acquire: pthread_rwlock_rdlock: %s", strerror(err));
		return -1;
	}
	
	/* Next we need to find an applicable section of the db file to lease */
	

	/* Next we need to obtain our in-proc lock on the applicable section */
	

	lease->connection = conn;
	
	return 0;
}

int logdb_lease_write (logdb_lease_t* lease, const void* buf, size_t len)
{
	DBGIF(!lease || !buf) {
		LOG("logdb_lease_write: failed-- lease or buf was NULL");
		return -1;
	}
	if (len > LOGDB_SECTION_SIZE) {
		LOG("logdb_lease_write: failed-- len is greater than section size");
		return -1;
	}

	return 0;
}

void logdb_lease_release (logdb_lease_t* lease)
{
	if (!lease || !(lease->connection))
		return;
	pthread_rwlock_unlock (&lease->connection->lock);
	lease->connection = NULL;
}