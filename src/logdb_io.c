#include "logdb_io.h"

#include <unistd.h>

ssize_t logdb_io_read (int fd, void* buf, size_t sz)
{
    ssize_t bytes;
    char* ix = (char*)buf;
    while ((sz > 0) && ((bytes = read (fd, ix, sz)) > 0)) {
        ix += bytes;
        sz -= bytes;
    }
    return (bytes == 0)? -1 : sz;
}

size_t logdb_io_write (int fd, const void* ptr, size_t sz)
{
    ssize_t bytes;
    const char* ix = (const char*)ptr;
    while ((sz > 0) && ((bytes = write (fd, ix, sz)) > 0)) {
		ix += bytes;
		sz -= bytes;
	}
    return sz;
}

ssize_t logdb_io_pread (int fd, void* buf, size_t sz, off_t offs)
{
    ssize_t bytes;
    char* ix = (char*)buf;
    while ((sz > 0) && ((bytes = pread (fd, ix, sz, offs)) > 0)) {
        offs += bytes;
        ix += bytes;
        sz -= bytes;
    }
    return (bytes == 0)? -1 : sz;
}

size_t logdb_io_pwrite (int fd, const void* ptr, size_t sz, off_t offs)
{
    ssize_t bytes;
    const char* ix = (const char*)ptr;
    while ((sz > 0) && ((bytes = pwrite (fd, ix, sz, offs)) > 0)) {
        offs += bytes;
		ix += bytes;
		sz -= bytes;
	}
    return sz;
}