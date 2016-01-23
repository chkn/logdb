#ifndef LOGDB_INTERNAL_H
#define LOGDB_INTERNAL_H

#include "logdb.h"

#include <stddef.h>

/**
 * The version of the internal data structures (and thus file format).
 * Bump this when any of the structs in this file change.
 */
#define LOGDB_VERSION 1

/**
 * The size of the database file sections that are reserved
 * in the log file.
 */
#define LOGDB_SECTION_SIZE 65536 /* bytes */

/**
 * The number of sections to preallocate in the database file.
 */
#define LOGDB_PREALLOCATE 4 /* sections */

/**
 * The minimum size for a valid database file
 */
#define LOGDB_MIN_SIZE (sizeof(logdb_header_t) + sizeof(logdb_log_header_t) + sizeof(logdb_trailer_t))

#define NULL ((void*)0)

typedef enum { false, true } bool;

/* Setup logging macros */
#ifdef DEBUG
#  include <stdio.h>
#  ifndef LOG
#    define LOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#  endif
#  ifndef ELOG
#    define ELOG(str) perror(str)
#  endif
#  ifndef VLOG
#    ifdef VERBOSE
#      define VLOG(fmt, ...) LOG(fmt, ##__VA_ARGS__)
#    else
#      define VLOG(fmt, ...)
#    endif
#  endif
#else
#  define LOG(fmt, ...)
#  define ELOG(str)
#  define VLOG(fmt, ...)
#endif

#endif /* LOGDB_INTERNAL_H */
