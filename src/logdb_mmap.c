
#include "logdb_mmap.h"

#include <fcntl.h>
#include <unistd.h>

void* logdb_mmap (const char* path, off_t offset, size_t length, logdb_open_flags flags)
{
    int oflags = O_RDWR;
    if ((flags & LOGDB_OPEN_CREATE) == LOGDB_OPEN_CREATE)
        oflags |= O_CREAT;
    if ((flags & LOGDB_OPEN_TRUNCATE) == LOGDB_OPEN_TRUNCATE)
        oflags |= O_TRUNC;

    int fd = open (path, oflags);
    if (fd < 0)
        return NULL;

    void* ptr = mmap (NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);

    close (fd);
    return (ptr == MAP_FAILED)? NULL : ptr;
}

int logdb_munmap (void* addr, size_t length)
{
    return munmap (addr, length);
}
