
#include "logdb_internal.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

/* By default, we use around 2MB (assuming a 4KB page size)
 * FIXME: Too conservative?
 */
#define DEFAULT_VMMAX (getpagesize() * 5)

logdb_connection* logdb_open (const char* path, logdb_open_flags flags, int vmmax)
{
	if (vmmax <= 0)
		vmmax = DEFAULT_VMMAX;

	int oflags = O_RDWR;
	if ((flags & LOGDB_OPEN_CREATE) == LOGDB_OPEN_CREATE)
		oflags |= O_CREAT;
	if ((flags & LOGDB_OPEN_TRUNCATE) == LOGDB_OPEN_TRUNCATE)
		oflags |= O_TRUNC;

	int fd = open (path, oflags, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		LOG("logdb_open: open(\"%s\", %d) failed: %s", path, oflags, strerror(errno));
		return NULL;
	}

	/* FIXME: To keep it simple, we'll just map vmmax right off the bat.
	 *  In the future, we may want to be smarter to minimize memory usage if possible.
	 */
	int length = vmmax;

	void* ptr = mmap (NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (close (fd) == -1) {
		LOG("logdb_open: close(...) failed: %s", strerror(errno));
	}
	if (ptr == MAP_FAILED) {
		LOG("logdb_open: mmap(NULL, %d, PROT_READ | PROT_WRITE, MAP_SHARED, %d, 0) failed: %s", length, fd, strerror(errno));
		return NULL;
	}

	logdb_connection_t* result = malloc (sizeof (logdb_connection_t));
	if (!result) {
		munmap (ptr, length);
		return NULL;   
	}

	result->version = 1;
	result->data = ptr;
	result->len = length;
	return result;
}

int logdb_close (logdb_connection* connection)
{
	logdb_connection_t* conn = (logdb_connection_t*)connection;
	if (!conn || conn->version != 1) {
		LOG("logdb_close: failed. Passed connection was either null or incorrect version.");
		return -1;
	}
	munmap (conn->data, conn->len);
	return 0;
}