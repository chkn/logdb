#ifndef LOGDB_IO_H
#define LOGDB_IO_H

#include <stddef.h>
#include <unistd.h>

/**
 * Reads from the given fd at the current file pointer into the given buffer.
  * \returns Zero (0) on success. On end of file, -1. Otherwise, the amount of data remaining to be read.
 */
ssize_t logdb_io_read (int fd, void* buf, size_t sz);

/**
 * Writes the given data to the given fd at the current file pointer.
 * \returns Zero (0) on success. On failure, the amount of data remaining to be written.
 */
size_t logdb_io_write (int fd, const void* ptr, size_t sz);

/**
 * Reads from the given fd at the given position into the given buffer.
 * \returns Zero (0) on success. On end of file, -1. Otherwise, the amount of data remaining to be read.
 */
ssize_t logdb_io_pread (int fd, void* buf, size_t sz, off_t offs);

/**
 * Writes the given data to the given fd at the given position.
 * \returns Zero (0) on success. On failure, the amount of data remaining to be written.
 */
size_t logdb_io_pwrite (int fd, const void* ptr, size_t sz, off_t offs);

#endif /* LOGBD_IO_H */