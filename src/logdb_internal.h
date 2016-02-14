#ifndef LOGDB_INTERNAL_H
#define LOGDB_INTERNAL_H

#include "logdb.h"

#include <stdlib.h>

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
 * The minimum size for a valid database file
 */
#define LOGDB_MIN_SIZE (sizeof(logdb_header_t) + sizeof(logdb_log_header_t) + sizeof(logdb_trailer_t))

typedef enum { false, true } bool;

/* Setup logging macros */
#ifdef DEBUG
#  include <stdio.h>
#  define DBGIF(...) if(__VA_ARGS__)
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
#  define DBGIF(...) if(0)
#  define LOG(fmt, ...)
#  define ELOG(str)
#  define VLOG(fmt, ...)
#endif

#endif /* LOGDB_INTERNAL_H */
