#include "logdb_lease.h"
#include "logdb_io.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h>

static bool logdb_lease_read_entry_space (logdb_log_t* log, logdb_log_entry_t* entry, logdb_size_t index, logdb_size_t size)
{
	off_t offset = logdb_log_read_entry (log, entry, index);
	if (offset == -1)
		return false;

	/* Check if the entry has enough free space for us */
	int freespace = LOGDB_SECTION_SIZE - (entry->len);
	return (freespace >= size);
}

static int logdb_lease_acquire_prelude (logdb_lease_t* lease, logdb_connection_t* conn)
{
	DBGIF(!lease || !conn) {
		LOG("logdb_lease_acquire_prelude: failed-- lease or conn was NULL");
		return -1;
	}
	lease->connection = NULL;

	/* Obtain shared lock on connection to prevent other threads from closing it on us */
	int err = pthread_rwlock_rdlock (&conn->lock);
	if (err) {
		LOG("logdb_lease_acquire_write: pthread_rwlock_rdlock: %s", strerror(err));
		return -1;
	}

	return 0;
}

int logdb_lease_acqire_read (logdb_lease_t* lease, logdb_connection_t* conn, logdb_size_t index, off_t offset)
{
	if (logdb_lease_acquire_prelude (lease, conn) != 0)
		return -1;

	/* Read the log to determine how much valid data is in the section.
	    Note that we don't take a lock here and it is perfectly valid for
		more data to be appended to the section after we read this. That
		data simply won't be a part of this lease.
	*/
	logdb_log_entry_t entry;
	if (logdb_log_read_entry (conn->log, &entry, index) == -1) {
		pthread_rwlock_unlock (&conn->lock);
		return -1;
	}
	if ((offset < 0) || (offset >= entry.len)) {
		LOG("logdb_lease_acqire_read: invalid offset");
		pthread_rwlock_unlock (&conn->lock);
		return -1;
	}

	lease->connection = conn;
	lease->index = index;
	lease->offset = offset;
	lease->len = entry.len - offset;
	lease->type = LOGDB_LOG_LOCK_NONE;
	return 0;
}

int logdb_lease_acquire_write (logdb_lease_t* lease, logdb_connection_t* conn, logdb_size_t size)
{
	if (size > LOGDB_SECTION_SIZE) {
		LOG("logdb_lease_acquire_write: cannot yet save data > LOGDB_SECTION_SIZE");
		return -1;
	}
	if (logdb_lease_acquire_prelude (lease, conn) != 0)
		return -1;

	/* Next we need to find an applicable section of the db file to lease..
	    We first check the last few entries to see if there's space, otherwise just append.

		To get at the last entries, we need to find the current end of the file, so we do an `lseek`.
		Note this won't screw up other threads; worst case one will fail to get the lock and take a walk.

		Also, it's ok if more entries are appended after we read the offset; we will simply traverse
		some older entries, but it's perfectly valid to try to extend them if we can get their locks.
		*/
	off_t offset;
	logdb_size_t index;
	logdb_log_entry_t entry;
	unsigned int visited = 0;

walk:
	offset = lseek (conn->log->fd, 0, SEEK_END);
	if (offset == -1) {
		ELOG("logdb_lease_acquire_write: lseek");
		pthread_rwlock_unlock (&conn->lock);
		return -1;
	}
	index = logdb_log_index_from_offset (offset) - visited;
	offset = -1;
	visited = 0;
	while ((index--) && (visited++ < LOGDB_LEASE_MAX_WALK)) {
		if (logdb_lease_read_entry_space (conn->log, &entry, index, size)) {
			offset = entry.len;
			break;
		}
		offset = -1;
	}

	/* If we didn't find any section with enough free space, just append a new one..
	    Since the file was opened with O_APPEND and we are writing so little data,
		this should be atomic.
	*/
	if (offset < 0) {
		/* We first write zero to the index indicating there is no valid data in this section */
		entry.len = 0;
		if (write (conn->log->fd, &entry, sizeof (logdb_log_entry_t)) < sizeof (logdb_log_entry_t)) {
			ELOG("logdb_lease_acquire_write: write");
			pthread_rwlock_unlock (&conn->lock);
			return -1;
		}

		/* If the write succeeded, the offset is dictated by our current file pointer
		    N.B. Technically there is a race here; another thread could've done a `write` after
			 ours but before we call `lseek` -- this doesn't matter. We'll simply get the index
			 of the section that thread just added. Whoever loses acquiring the lock will go back
			 and walk and find the section our `write` call added.
		 */
		offset = lseek (conn->log->fd, 0, SEEK_CUR);
		if (offset < 0) {
			ELOG("logdb_lease_acquire_write: lseek");
			pthread_rwlock_unlock (&conn->lock);
			return -1;
		}
		index = logdb_log_index_from_offset (offset) - 1;
		offset = 0;
		visited = 1; /* skip the last entry if we walk again */
	}
	

	VLOG("logdb_lease_acquire_write: attempting to acquire lease of section %d", index);

	/* get the lock */
	if (logdb_log_lock (conn->log, index, LOGDB_LOG_LOCK_WRITE) != 0) {
		/* FIXME: Would it ever be possible to loop forever here? */
		goto walk;
	}

	/* now that we have the lock, double check that there is still enough space in the section */
	if (!logdb_lease_read_entry_space (conn->log, &entry, index, size))
		goto walk;

	lease->connection = conn;
	lease->index = index;
	lease->offset = offset;
	lease->len = size;
	lease->type = LOGDB_LOG_LOCK_WRITE;
	return 0;
}

size_t logdb_lease_read (logdb_lease_t* lease, void* buf, logdb_size_t len)
{
	DBGIF(!lease || !buf) {
		LOG("logdb_lease_read: failed-- lease or buf was NULL");
		return len;
	}
	if (len > lease->len) {
		LOG("logdb_lease_read: failed-- len exceeds lease size");
		return len;
	}

	off_t offset = logdb_connection_offset (lease->index) + lease->offset;
	size_t notread = logdb_io_pread (lease->connection->fd, buf, len, offset);
	size_t bytes = len - notread;

	lease->offset += bytes;
	lease->len -= bytes;
	return notread;
}

size_t logdb_lease_write (logdb_lease_t* lease, const void* buf, logdb_size_t len)
{
	DBGIF(!lease || !buf) {
		LOG("logdb_lease_write: failed-- lease or buf was NULL");
		return len;
	}
	if (len > lease->len) {
		LOG("logdb_lease_write: failed-- len exceeds lease size");
		return len;
	}

	off_t offset = logdb_connection_offset (lease->index) + lease->offset;
	size_t notwritten = logdb_io_pwrite (lease->connection->fd, buf, len, offset);
	size_t bytes = len - notwritten;

	lease->offset += bytes;
	lease->len -= bytes;
	return notwritten;
}

off_t logdb_lease_seek (logdb_lease_t* lease, off_t offset)
{
	DBGIF(((lease->offset + offset) < 0) || (offset > (lease->len))) {
		LOG("logdb_lease_seek: invalid offset");
		return -1;
	}
	lease->offset += offset;
	lease->len -= offset;
	return lease->offset;
}

void logdb_lease_release (logdb_lease_t* lease)
{
	DBGIF(!lease || !(lease->connection)) {
		LOG("logdb_lease_release: passed invalid lease");
		return;
	}
	if (lease->type != LOGDB_LOG_LOCK_NONE)
		logdb_log_unlock (lease->connection->log, lease->index, lease->type);
	pthread_rwlock_unlock (&lease->connection->lock);
	lease->connection = NULL;
}