#ifndef LOGDB_IO_H
#define LOGDB_IO_H

#include <stddef.h>

/**
 * Reads from the given fd into the given buffer.
 * \returns Zero (0) on success. On failure, the amount of data remaining to be read.
 */
size_t logdb_io_read (int fd, void* buf, size_t sz);

/**
 * Writes the given data to the given fd.
 * \returns Zero (0) on success. On failure, the amount of data remaining to be written.
 */
size_t logdb_io_write (int fd, const void* ptr, size_t sz);

#endif /* LOGBD_IO_H */