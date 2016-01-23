
#include "logdb_log.h"
#include "logdb_connection.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static size_t logdb_log_size (logdb_log_header_t* header)
{
	return sizeof (logdb_log_header_t) + (header->len * sizeof (logdb_log_entry_t));
}

static logdb_log_t* logdb_log_new (int fd, const char* path, logdb_log_header_t* header)
{
	logdb_log_t* result = malloc (sizeof (logdb_log_t));
	if (!result) {
		ELOG("logdb_log_new: malloc (sizeof (logdb_log_t)) failed");
		return NULL;
	}

	result->fd = fd;
	result->path = realpath (path, NULL);
	result->len = header->len;
	return result;
}

/**
 * Reads the log at the current position of the given fd.
 *  The returned pointer must be freed with `free`
 */
static logdb_log_header_t* logdb_log_read (int fd, bool entire)
{
	logdb_log_header_t* header = malloc (sizeof (logdb_log_header_t));
	if (read (fd, header, sizeof (logdb_log_header_t)) < sizeof (logdb_log_header_t)) {
		ELOG("logdb_log_read: read (fd, header, sizeof (logdb_log_header_t)) failed");
		free (header);
		return NULL;
	}

	/* Validate the header */
	if ((memcmp (header->magic, LOGDB_LOG_MAGIC, sizeof (LOGDB_LOG_MAGIC) - 1) != 0)
	 || (header->version != LOGDB_VERSION)) {
		LOG("logdb_log_read: failed to validate log header");
		free (header);
		return NULL;
	}

	if (!entire)
		return header;

	/* Build the log */
	size_t entrysize = header->len * sizeof (logdb_log_entry_t);
	logdb_log_header_t* result = malloc (sizeof (logdb_log_header_t) + entrysize);
	if (!result) {
		ELOG("logdb_log_read: malloc (sizeof (logdb_log_header_t) + entrysize) failed");
		free (header);
		return NULL;
	}
	memcpy (result, header, sizeof (logdb_log_header_t));
	free (header);
	if (read (fd, result + 1, entrysize) < entrysize) {
		ELOG("logdb_log_read: read (fd, result + 1, entrysize) failed");
		free (result);
		return NULL;
	}
	
	return result;
}

logdb_log_t* logdb_log_open (const char* path)
{
	int fd = open (path, O_RDWR);
	if (fd == -1) {
		LOG("logdb_log_open: open(\"%s\", O_RDWR) failed: %s", path, strerror(errno));
		return NULL;
	}
	logdb_log_header_t* header = logdb_log_read (fd, false);
	if (!header)
		return NULL;

	logdb_log_t* result = logdb_log_new (fd, path, header);
	free (header);
	return result;
}

logdb_log_t* logdb_log_create (const char* path, int dbfd)
{
	struct stat dbstat;
	logdb_log_header_t* header = NULL;

	if (fstat (dbfd, &dbstat) != 0) {
		ELOG("logdb_log_create: failed to stat db file");
		return NULL;
	}

	/* First, see if there is an existing log at the end of the db */
	if (dbstat.st_size > LOGDB_MIN_SIZE) {
		VLOG("logdb_log_create: reading log from end of db");

		if (lseek (dbfd, -sizeof(logdb_trailer_t), SEEK_END) == -1) {
			ELOG("logdb_log_create: lseek (dbfd, -sizeof(logdb_trailer_t), SEEK_END) failed");
			return NULL;
		}

		logdb_trailer_t trailer;
		if (read (dbfd, &trailer, sizeof(logdb_trailer_t)) == -1) {
			ELOG("logdb_log_create: read (dbfd, &trailer, sizeof(logdb_trailer_t)) failed");
			return NULL;
		}

		/* FIXME: This could overflow, causing us not to find the log in the db, causing us to lose all data :( */
		signed int offs = trailer.log_offset * -1;
		if (lseek (dbfd, offs, SEEK_END) == -1) {
			ELOG("logdb_log_create: lseek (dbfd, offs, SEEK_END) failed");
			return NULL;
		}

		header = logdb_log_read (dbfd, true);
	}

	/* Otherwise, we can only guess that no data in the db is valid */
	if (!header) {
		VLOG("logdb_log_create: db does not contain a log");

		/* Determine number of sections in the db; preallocate more if necessary */
		off_t sections = (off_t)ceil (dbstat.st_size / LOGDB_SECTION_SIZE);
		if (sections < LOGDB_PREALLOCATE) {
			sections = LOGDB_PREALLOCATE;
			if (ftruncate (dbfd, LOGDB_PREALLOCATE * LOGDB_SECTION_SIZE) != 0) {
				ELOG("logdb_log_create: ftruncate (dbfd, LOGDB_PREALLOCATE * LOGDB_SECTION_SIZE) failed");
				return NULL;
			}
		}

		size_t entrysize = sections * LOGDB_SECTION_SIZE;
		header = malloc (sizeof (logdb_log_header_t) + entrysize);
		if (!header) {
			ELOG("logdb_log_create: malloc (sizeof (logdb_log_header_t) + entrysize) failed");
			return NULL;
		}

		strcpy (header->magic, LOGDB_LOG_MAGIC);
		header->version = LOGDB_VERSION;
		header->len = sections;
		memset (header + 1, 0, entrysize);
	}

	int logfd = open (path, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (logfd == -1) {
		LOG("logdb_log_create: open(\"%s\", O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR) failed: %s", path, strerror(errno));
		free (header);
		return NULL;
	}

	size_t logsz = sizeof (logdb_log_header_t) + (header->len * sizeof (logdb_log_entry_t));
	if (write (logfd, header, logsz) < logsz) {
		ELOG("logdb_log_create: write (logfd, result, logsz) failed");
		free (header);
		close (logfd);
		return NULL;
	}

	logdb_log_t* result = logdb_log_new (logfd, path, header);
	free (header);
	return result;
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
		ELOG("logdb_log_close_merge: lseek");
		goto failunlock;
	}
	logdb_log_header_t* header = logdb_log_read (log->fd, true);
	if (!header)
		goto failunlock;

	/* See if there is already enough free space at the end of the file */
	size_t foundspace = 0;
	size_t headersz = logdb_log_size (header);
	size_t reqspace = headersz + sizeof (logdb_trailer_t);
	int nsections = (int)ceil ((float)reqspace / LOGDB_SECTION_SIZE);
	logdb_log_entry_t* entries = (logdb_log_entry_t*)(header + 1);
	if (header->len >= nsections) {
		for (unsigned int i = 1; i <= nsections; i++) {
			logdb_log_entry_t entry = entries [header->len - i];
			foundspace += LOGDB_SECTION_SIZE - entry.len;
			if (entry.len != 0)
				break;
		}
	}

	off_t seek = (foundspace >= reqspace)? -reqspace : 0;
	if (lseek (dbfd, seek, SEEK_END) == -1) {
		ELOG("logdb_log_close_merge: lseek");
		goto failunlock;
	}

	logdb_trailer_t trailer;
	trailer.log_offset = reqspace;
	if ((write (dbfd, header, headersz) < headersz) || (write (dbfd, &trailer, sizeof (trailer)) < sizeof (trailer))) {
		/* NOTE: The is no possibility of corrupting the db here, because the
		    log file still shows these bytes as free.
		*/
		ELOG("logdb_log_close_merge: write(s) failed");
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