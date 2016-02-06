
#include "logdb_connection.h"
#include "logdb_txn.h"
#include "logdb_io.h"

#include <stdlib.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

logdb_connection* logdb_open (const char* path, logdb_open_flags flags)
{
	if (!path) {
		LOG("logdb_open: path is NULL");
		return NULL;
	}

	int oflags = O_RDWR;
	if ((flags & LOGDB_OPEN_CREATE) == LOGDB_OPEN_CREATE)
		oflags |= O_CREAT;

	/* Compute the path for the log file */
	size_t pathlen = strlen (path) + sizeof (LOGDB_LOG_FILE_SUFFIX);
	char* logpath = malloc (pathlen);
	if (!logpath) {
		ELOG("logdb_open: malloc");
		return NULL;
	}
	(void)strncpy (logpath, path, pathlen);
	(void)strcat (logpath, LOGDB_LOG_FILE_SUFFIX);

	/* Open the database first */
	int fd = open (path, oflags, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		ELOG("logdb_open: open");
		free (logpath);
		return NULL;
	}

	/* If LOGDB_OPEN_CREATE was specified, write a db header, otherwise we need to verify the db header */
	if ((flags & LOGDB_OPEN_CREATE) == LOGDB_OPEN_CREATE) {
		VLOG("logdb_open: writing db header");

		logdb_header_t header;
		strcpy (header.magic, LOGDB_MAGIC);
		header.version = LOGDB_VERSION;

		if (logdb_io_write (fd, &header, sizeof (header)) != 0) {
			ELOG("logdb_open: write");
			close (fd);
			free (logpath);
			return NULL;
		}
	} else {
		VLOG("logdb_open: verifying db header");

		logdb_header_t header;
		if (logdb_io_read (fd, &header, sizeof (header)) != 0) {
			ELOG("logdb_open: read");
			close (fd);
			free (logpath);
			return NULL;
		}

		/* Validate the header */
		if ((memcmp (&header.magic, LOGDB_MAGIC, sizeof (LOGDB_MAGIC) - 1) != 0)
			|| (header.version != LOGDB_VERSION)) {
			LOG("logdb_open: failed to validate db header");
			close (fd);
			free (logpath);
			return NULL;
		}
	}

	/* If we are the first ones to open this file, we need to create a log.
	 *  The `flock` prevents races with other processes.
	 */
	bool retry = true;
	logdb_log_t* log = NULL;
retry_create:
	if (flock (fd, LOCK_EX | LOCK_NB) == 0) {
		log = logdb_log_create (logpath, fd);
		if (!log) {
			VLOG("logdb_open: failed to create log-- there may be an existing one that needs recovery.");
			/* We can end up here if:
			 *  1) Another process crashed while creating the log previously.
			 *  2) The DB was previouly open by other process(es) who all failed to close it.
			 */
			 /* FIXME: Determine which of the above cases it is and recover */
			 close (fd);
			 free (logpath);
			 return NULL;
		}
	} else {
		VLOG("logdb_open: failed to acquire exclusive db lock to setup log-- another process must've already done it.");
	}

	/* Otherwise, we acquire a shared lock to wait for the other process to finish setting up the log.
	 *  This will go away when we close the fd.
	 */
	if (flock (fd, LOCK_SH) != 0) {
		ELOG("logdb_open: flock");
		close (fd);
		free (logpath);
		return NULL;
	}

	/* Open the log if we did not create one earlier */
	if (!log) {
		log = logdb_log_open (logpath);
		if (!log) {
			/* We could end up here if another process was doing a `logdb_close`
			    and merging the log back into the db when we were previously trying
				 to get the exclusive lock. So we'll retry once if we haven't already.
			*/
			if (retry) {
				VLOG("logdb_open: failed to open log after acquiring shared db lock-- trying to create again.");
				retry = false;
				goto retry_create;
			}
			LOG("logdb_open: logdb_log_open failed");
			close (fd);
			free (logpath);
			return NULL;
		}
	}
	free (logpath);

	logdb_connection_t* result = calloc (1, sizeof (logdb_connection_t));
	if (!result) {
		ELOG("logdb_open: calloc");
		goto logclosefail;
	}

	int err = pthread_rwlock_init (&result->lock, NULL);
	if (err) {
		LOG("logdb_open: pthread_rwlock_init: %s", strerror (err));
		free (result);
		goto logclosefail;
	}

	err = pthread_key_create (&result->current_txn_key, &logdb_txn_destruct);
	if (err) {
		LOG("logdb_open: pthread_key_create: %s", strerror (err));
		pthread_rwlock_destroy (&result->lock);
		free (result);
		goto logclosefail;
	}

	result->version = LOGDB_VERSION;
	result->fd = fd;
	result->log = log;
	return result;
logclosefail:
	logdb_log_close (log);
	close (fd);
	return NULL;
}

int logdb_close LOGDB_VERIFY_CONNECTION(logdb_connection_t* conn)
{
	/* Wait for any other threads to finish */
	int err = pthread_rwlock_wrlock (&conn->lock);
	if (err) {
		LOG("logdb_close: pthread_rwlock_wrlock: %s", strerror(err));
		return -1;
	}

	/* If there is an active transactions on this thread, roll them back */
	logdb_txn_rollback_all (conn);
	pthread_key_delete (conn->current_txn_key);

	/* If we are the last process using the db, merge the log back into it */
	bool log_closed = false;
	if (flock (conn->fd, LOCK_EX | LOCK_NB) == 0) {
		VLOG("logdb_close: acquired exclusive lock on db file, so merging log back");
		if (logdb_log_close_merge (conn->log, conn->fd) == 0)
			log_closed = true;
	}
	if (!log_closed) {
		/* Other processes are still using it, so just close it, leaving the file there */
		if (logdb_log_close (conn->log) != 0) {
			LOG("logdb_close: logdb_log_close failed");
			flock (conn->fd, LOCK_UN);
			pthread_rwlock_unlock (&conn->lock);
			return -1;
		}
	}
	flock (conn->fd, LOCK_UN);
	close (conn->fd);
	pthread_rwlock_unlock (&conn->lock);
	pthread_rwlock_destroy (&conn->lock);
	/* Just in case this helps.. */
	conn->version = 0;
	free (conn);
	return 0;
}}