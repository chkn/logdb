
#include "logdb_internal.h"

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
	int oflags = O_RDWR;
	if ((flags & LOGDB_OPEN_CREATE) == LOGDB_OPEN_CREATE)
		oflags |= O_CREAT;

	/* Compute the path for the index file */
	size_t pathlen = strlen (path) + sizeof (LOGDB_INDEX_FILE_SUFFIX);
	char* indexpath = malloc (pathlen);
	if (!indexpath) {
		ELOG("logdb_open: malloc");
		return NULL;
	}
	(void)strncpy (indexpath, path, pathlen);
	(void)strcat (indexpath, LOGDB_INDEX_FILE_SUFFIX);

	/* Open the database first */
	int fd = open (path, oflags, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		ELOG("logdb_open: open");
		free (indexpath);
		return NULL;
	}

	/* If we are the first ones to open this file, we need to create an index.
	 *  The `flock` prevents races with other processes.
	 */
	bool retry = true;
	logdb_index_t* index = NULL;
retry_create:
	if (flock (fd, LOCK_EX | LOCK_NB) == 0) {
		index = logdb_index_create (indexpath, fd);
		if (!index) {
			VLOG("logdb_open: failed to create index-- there may be an existing one that needs recovery.");
			/* We can end up here if:
			 *  1) Another process crashed while creating the index previously.
			 *  2) The DB was previouly open by other process(es) who all failed to close it.
			 */
			 /* FIXME: Determine which of the above cases it is and recover */
			 close (fd);
			 free (indexpath);
			 return NULL;
		}
	} else {
		VLOG("logdb_open: failed to acquire exclusive db lock to setup index-- another process must've already done it.");
	}

	/* Otherwise, we acquire a shared lock to wait for the other process to finish setting up the index.
	 *  This will go away when we close the fd.
	 */
	if (flock (fd, LOCK_SH) != 0) {
		ELOG("logdb_open: flock");
		close (fd);
		free (indexpath);
		return NULL;
	}

	/* Open the index if we did not create one earlier */
	if (!index) {
		index = logdb_index_open (indexpath);
		if (!index) {
			/* We could end up here if another process was doing a `logdb_close`
			    and merging the index back into the db when we were previously trying
				 to get the exclusive lock. So we'll retry once if we haven't already.
			*/
			if (retry) {
				VLOG("logdb_open: failed to open index after acquiring shared db lock-- trying to create again.");
				retry = false;
				goto retry_create;
			}
			LOG("logdb_open: logdb_index_open failed");
			close (fd);
			free (indexpath);
			return NULL;
		}
	}
	free (indexpath);

	logdb_connection_t* result = malloc (sizeof (logdb_connection_t));
	if (!result) {
		ELOG("logdb_open: malloc");
		close (fd);
		return NULL;   
	}

	/* Zero our memory first, just incase..
	   N.B. Darwin's `pthread_rwlock_init` seems to check for an existing rwlock structure.
	    The odds of this causing a problem are very slim, but so is the cost of this memset :p */
	memset (result, 0, sizeof (logdb_connection_t));

	int err = pthread_rwlock_init (&result->lock, NULL);
	if (err) {
		ELOG("logdb_open: pthread_rwlock_init");
		close (fd);
		free (result);
		return NULL;
	}

	result->version = LOGDB_VERSION;
	result->fd = fd;
	result->index = index;
	return result;
}

int logdb_close (logdb_connection* connection)
{
	logdb_connection_t* conn = (logdb_connection_t*)connection;
	if (!conn || conn->version != LOGDB_VERSION) {
		LOG("logdb_close: failed. Passed connection was either null, already closed, or incorrect version.");
		return -1;
	}
	/* Wait for any other threads to finish */
	int err = pthread_rwlock_wrlock (&conn->lock);
	if (err) {
		ELOG("logdb_close: pthread_rwlock_wrlock");
		return -1;
	}

	/* If we are the last process using the db, merge the index back into it */
	bool index_closed = false;
	if (flock (conn->fd, LOCK_EX | LOCK_NB) == 0) {
		VLOG("logdb_close: acquired exclusive lock on db file, so merging index back");
		if (logdb_index_close_merge (conn->index, conn->fd) == 0)
			index_closed = true;
	}
	if (!index_closed) {
		/* Other processes are still using it, so just close it, leaving the file there */
		if (logdb_index_close (conn->index) != 0) {
			LOG("logdb_close: logdb_index_close failed");
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
	free (connection);
	return 0;
}