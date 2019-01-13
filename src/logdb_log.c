
#include "logdb_log.h"
#include "logdb_connection.h"
#include "logdb_io.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdatomic.h>

static off_t logdb_log_offset (logdb_size_t index)
{
	return sizeof (logdb_log_header_t) + (index * sizeof (logdb_log_entry_t));
}

logdb_size_t logdb_log_index_from_offset (off_t offset)
{
	/* FIXME */
	if (offset < sizeof (logdb_log_header_t))
		return 0;

	return (logdb_size_t)((offset - sizeof (logdb_log_header_t)) / sizeof (logdb_log_entry_t));
}

static logdb_log_t* logdb_log_new (int fd, const char* path)
{
	logdb_log_t* result = malloc (sizeof (logdb_log_t));
	if (!result) {
		ELOG("logdb_log_new: malloc");
		return NULL;
	}

	result->fd = fd;
	result->path = realpath (path, NULL);
	result->lock = NULL;
	return result;
}

typedef enum {
	LOGDB_VALIDATE_ONLY,
	LOGDB_READ_NO_TRAILER,
	LOGDB_READ_HAS_TRAILER
} logdb_log_read_mode;
/**
 * Reads the log at the current position of the given fd.
 *  Unless `onlyvalidate` is true, the returned pointer must be freed with `free`
 */
static logdb_log_header_t* logdb_log_read (int fd, logdb_log_read_mode mode)
{
	logdb_log_header_t header;
	ssize_t readresult = logdb_io_read (fd, &header, sizeof (logdb_log_header_t));
	if (readresult > 0) {
		ELOG("logdb_log_read: read 1");
		return NULL;
	}

	/* Validate the header */
	if ((readresult == -1) || (memcmp (&header.magic, LOGDB_LOG_MAGIC, sizeof (LOGDB_LOG_MAGIC) - 1) != 0)
	 || (header.version != LOGDB_VERSION)) {
		LOG("logdb_log_read: failed to validate log header");
		return NULL;
	}

	if (mode == LOGDB_VALIDATE_ONLY)
		return ((void*)1);

	off_t start = lseek (fd, 0, SEEK_CUR);
	if (start == -1) {
		ELOG("logdb_log_read: lseek 1");
		return NULL;
	}
	off_t end = lseek (fd, 0, SEEK_END);
	if (end == -1) {
		ELOG("logdb_log_read: lseek 2");
		return NULL;
	}
	if (mode == LOGDB_READ_HAS_TRAILER)
		end -= sizeof (logdb_trailer_t);

	size_t entrysize = end - start;
	logdb_log_header_t* result = malloc (sizeof (logdb_log_header_t) + entrysize);
	if (!result) {
		ELOG("logdb_log_read: malloc");
		return NULL;
	}

	memcpy (result, &header, sizeof (logdb_log_header_t));

    if (entrysize) {
		if (lseek (fd, start, SEEK_SET) == -1) {
			ELOG("logdb_log_read: lseek 3");
			return NULL;
		}
		if (logdb_io_read (fd, result + 1, entrysize) != 0) {
			ELOG("logdb_log_read: read 2");
			free (result);
			return NULL;
		}
	}
	return result;
}

logdb_log_t* logdb_log_open (const char* path)
{
	int fd = open (path, O_RDWR | O_APPEND);
	if (fd == -1) {
		LOG("logdb_log_open: open(\"%s\", O_RDWR) failed: %s", path, strerror(errno));
		return NULL;
	}

	if (!logdb_log_read (fd, LOGDB_VALIDATE_ONLY)) {
		close (fd);
		return NULL;
	}

	return logdb_log_new (fd, path);
}

logdb_log_t* logdb_log_create (const char* path, int dbfd)
{
	logdb_log_header_t* header = NULL;
	size_t logsz = sizeof (logdb_log_header_t);

	off_t dbsz = lseek (dbfd, 0, SEEK_END);
	if (dbsz == -1) {
		ELOG("logdb_log_create: lseek 1");
		return NULL;
	}

	/* First, see if there is an existing log at the end of the db */
	if (dbsz >= LOGDB_MIN_SIZE) {
		VLOG("logdb_log_create: reading log from end of db");

		if (lseek (dbfd, -sizeof (logdb_trailer_t), SEEK_END) == -1) {
			ELOG("logdb_log_create: lseek 2");
			return NULL;
		}

		logdb_trailer_t trailer;
		if (logdb_io_read (dbfd, &trailer, sizeof (logdb_trailer_t)) > 0) {
			ELOG("logdb_log_create: read");
			return NULL;
		}

		/* FIXME: This could overflow, causing us not to find the log in the db, causing us to lose all data :( */
		off_t offs = ((off_t)trailer.log_offset) * -1;
		if (lseek (dbfd, offs, SEEK_END) == -1) {
			ELOG("logdb_log_create: lseek 3");
			return NULL;
		}

		header = logdb_log_read (dbfd, LOGDB_READ_HAS_TRAILER);
		logsz = trailer.log_offset - sizeof (logdb_trailer_t);
	}

	/* Otherwise, we can only guess that no data in the db is valid */
	if (!header) {
		VLOG("logdb_log_create: db does not contain a log");

		header = malloc (sizeof (logdb_log_header_t));
		if (!header) {
			ELOG("logdb_log_create: malloc");
			return NULL;
		}

		(void)strncpy (header->magic, LOGDB_LOG_MAGIC, sizeof (header->magic));
		header->version = LOGDB_VERSION;
	}

	int logfd = open (path, O_RDWR | O_CREAT | O_EXCL | O_APPEND, S_IRUSR | S_IWUSR);
	if (logfd == -1) {
		ELOG("logdb_log_create: open");
		free (header);
		return NULL;
	}

	/* Write the log content before the header. This way, if we crash while doing this,
	    we can gracefully recover next time.
	*/
	if ((logsz > sizeof (logdb_log_header_t)) && (logdb_io_pwrite (logfd, header + 1, logsz - sizeof (logdb_log_header_t), sizeof (logdb_log_header_t)) != 0))
		goto logwritefail;

#if DEBUG
	if (getenv ("LOGDB_TEST_LOG_CREATE_RETURN_EARLY")) {
		free (header);
		close (logfd);
		return NULL;
	}
#endif

	/* Now write the header to indicate this log file is valid. Note that this doesn't
	    strictly need to be done atomically because we verify all parts of the header.
	*/
	if (logdb_io_pwrite (logfd, header, sizeof (logdb_log_header_t), 0) != 0)
		goto logwritefail;

	free (header);
	return logdb_log_new (logfd, path);

logwritefail:
	ELOG("logdb_log_create: write");
	free (header);
	close (logfd);
	/* don't leave a partially written log laying around */
	unlink (path);
	return NULL;
}

off_t logdb_log_read_entry (const logdb_log_t* log, logdb_log_entry_t* buf, logdb_size_t index)
{
	DBGIF(!log || !buf) {
		LOG("logdb_log_read_entry: failed-- passed log or buf was null");
		return -1;
	}

	off_t offset = logdb_log_offset (index);
	ssize_t result = logdb_io_pread (log->fd, buf, sizeof (logdb_log_entry_t), offset);
	if (result > 0) {
		ELOG("logdb_log_read_entry: pread");
		return -1;
	}
	return (result == -1)? -1 : offset;
}

int logdb_log_write_entry (logdb_log_t* log, logdb_log_entry_t* buf, logdb_size_t index)
{
	DBGIF(!log || !buf) {
		LOG("logdb_log_write_entry: failed-- passed log or buf was null");
		return -1;
	}

	off_t offset = logdb_log_offset (index);
	if (logdb_io_pwrite (log->fd, buf, sizeof (logdb_log_entry_t), offset) > 0) {
		ELOG("logdb_log_write_entry: pwrite");
		return -1;
	}
	return 0;
}

static void logdb_log_inproc_unlock (logdb_log_t* log, logdb_size_t index, logdb_log_lock_type type)
{
	/* Since we know we have the lock, this algorithm is a little less tortured than the one to take the lock */
	volatile logdb_log_lock_t* lock = log->lock;
	while ((lock->startindex + 127) < index)
		lock = lock->next;

	logdb_size_t lockindex = index - (lock->startindex);
	if (type == LOGDB_LOG_LOCK_READ)
		atomic_fetch_sub (&lock->locks[lockindex], 1);
	else if (type == LOGDB_LOG_LOCK_WRITE)
		atomic_fetch_add (&lock->locks[lockindex], 1);
	else
		abort();
}

int logdb_log_lock (logdb_log_t* log, logdb_size_t index, logdb_log_lock_type type)
{
	/* We MUST obtain our in-proc lock on the applicable section BEFORE
		attempting to take the `fcntl` lock. Otherwise, a different thread
		might inadvertently unlock us.
	 */
	volatile _Atomic(logdb_log_lock_t*)* dest = &log->lock;
	logdb_log_lock_t* lock = atomic_load (dest);

tryagain1:
	if (!lock || (lock->startindex > index)) {
		logdb_log_lock_t* newlock = (logdb_log_lock_t*)calloc (1, sizeof (logdb_log_lock_t));
		if (!newlock) {
			ELOG("logdb_log_lock: calloc");
			return -1;
		}
		newlock->startindex = index;
		atomic_init (&newlock->next, lock);

		/* FIXME: Benchmark if atomic_compare_exchange_weak is more performant
		     (I assumend it wasn't, due to the `newlock` reallocation on loop) */
		if (!atomic_compare_exchange_strong (dest, &lock, newlock)) {
			free (newlock);
			goto tryagain1;
		}
        lock = newlock;
	}

	logdb_size_t lockindex = index - (lock->startindex);
	if (lockindex > 127) {
		dest = &lock->next;
		lock = atomic_load (dest);
		goto tryagain1;
	}

	/* Now that we have the correct structure, attempt to lock it */
	int value;
	struct flock flk;
	value = lock->locks[lockindex];
tryagain2:
	if ((type == LOGDB_LOG_LOCK_READ) && (value >= 0)) {
		/* Take a read lock 
			FIXME: Prevent overflow!
		*/
		if (!atomic_compare_exchange_weak (&lock->locks[lockindex], &value, value + 1))
			goto tryagain2;
		flk.l_type = F_RDLCK;
	} else if ((type == LOGDB_LOG_LOCK_WRITE) && (value == 0)) {
		/* Take a write lock */
		if (!atomic_compare_exchange_weak (&lock->locks[lockindex], &value, -1))
			goto tryagain2;
		flk.l_type = F_WRLCK;
	} else {
		/* We can't take any lock :( */
		return -2;
	}

	/* Now we've locked this entry at the process level, we need to take
	    an `fcntl` lock on the section of the file to synchronize across processes */
	flk.l_whence = SEEK_SET;
	flk.l_start = logdb_log_offset (index);
	flk.l_len = sizeof (logdb_log_entry_t);
	flk.l_pid = getpid();
	if (fcntl (log->fd, F_SETLK, &flk) == -1) {
		if (errno != EAGAIN)
			ELOG("logdb_log_lock: fcntl");
		logdb_log_inproc_unlock (log, index, type);
		return -3;
	}

	/* We have the lock! */
	return 0;
}

void logdb_log_unlock (logdb_log_t* log, logdb_size_t index, logdb_log_lock_type type)
{
	struct flock flk;
	flk.l_type = F_UNLCK;
	flk.l_whence = SEEK_SET;
	flk.l_start = logdb_log_offset (index);
	flk.l_len = sizeof (logdb_log_entry_t);
	flk.l_pid = getpid();
	if (fcntl (log->fd, F_SETLK, &flk) == -1)
		ELOG("logdb_log_unlock: fcntl");

	logdb_log_inproc_unlock (log, index, type);
}

int logdb_log_close (logdb_log_t* log)
{
	if (!log) {
		LOG("logdb_log_close: failed-- passed log was null");
		return -1;
	}
	close (log->fd);
	if (log->path)
		free (log->path);
	free (log);
	return 0;
}

int logdb_log_close_merge (logdb_log_t* log, int dbfd)
{
	if (!log || !(log->path)) {
		LOG("logdb_log_close_merge: failed-- passed log was null, or no path data exists for it");
		return -1;
	}

	/* Lock the whole log; it will be unlocked when the fd is closed */
	struct flock flk;
	flk.l_type = F_WRLCK;
	flk.l_whence = SEEK_SET;
	flk.l_start = 0;
	flk.l_len = 0;
	flk.l_pid = getpid();
	if (fcntl (log->fd, F_SETLKW, &flk) == -1) {
		ELOG("logdb_log_close_merge: fcntl");
		return -1;
	}

	/* Seek to beginning of log */
	if (lseek (log->fd, 0, SEEK_SET) == -1) {
		ELOG("logdb_log_close_merge: lseek 1");
		goto failunlock;
	}
	logdb_log_header_t* header = logdb_log_read (log->fd, LOGDB_READ_NO_TRAILER);
	if (!header)
		goto failunlock;

	/* Now seek to the end to get the length */
	off_t logsz = lseek (log->fd, 0, SEEK_END);
	if (logsz == -1) {
		ELOG("logdb_log_close_merge: lseek 2");
		goto failunlock;
	}

	/* Let's figure out the min size of the db and truncate to there */
	logdb_size_t sections = logdb_log_index_from_offset (logsz);
	off_t minsz = logdb_connection_offset (sections);

	/* See if we can trim any more from the end */
	logdb_log_entry_t* entries = (logdb_log_entry_t*)(header + 1);
	for (unsigned int i = 1; i <= sections; i++) {
		logdb_log_entry_t entry = entries [sections - i];
		minsz -= LOGDB_SECTION_SIZE - entry.len;
		if (entry.len == 0)
			logsz -= sizeof (logdb_log_entry_t);
		else
			break;
	}

	if (ftruncate (dbfd, minsz) != 0) {
		ELOG("logdb_log_close_merge: ftruncate");
		goto failunlock;
	}

	if (lseek (dbfd, 0, SEEK_END) == -1) {
		ELOG("logdb_log_close_merge: lseek");
		goto failunlock;
	}

	logdb_trailer_t trailer;
	trailer.log_offset = logsz + sizeof (trailer);
	if (logdb_io_write (dbfd, header, logsz) || logdb_io_write (dbfd, &trailer, sizeof (trailer))) {
		/* NOTE: The is no possibility of corrupting the db here, because the
		    log file still shows these bytes as free.
		*/
		ELOG("logdb_log_close_merge: write(s)");
		goto failunlock;
	}

	/* It doesn't really matter if this fails; next open will detect the log and deal with it */
	unlink (log->path);

	return logdb_log_close (log);
failunlock:
	flk.l_type = F_UNLCK;
	fcntl (log->fd, F_SETLK, &flk);
	return -1;
}