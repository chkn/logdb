#ifndef LOGDB_MMAP_H
#define LOGDB_MMAP_H

#include "logdb_internal.h"

/* FIXME: Windows support? */
#include <sys/mman.h>

/**
 * Maps a view of the file at the given path into memory.
 * \returns A pointer to the memory mapping, or NULL if there was an error mapping the file into memory.
 */
void* logdb_mmap (const char* path, off_t offset, size_t length, logdb_open_flags flags);

/**
 * Unmaps a file memory mapping previously created by `logdb_mmap`.
 * \returns Zero (0) on success.
 */
int logdb_munmap (void* addr, size_t length);

#endif /* LOGDB_MMAP_H */
