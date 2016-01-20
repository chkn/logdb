#ifndef LOGDB_INTERNAL_H
#define LOGDB_INTERNAL_H

#include "logdb.h"

#include <stddef.h>
#include <pthread.h>
#include "logdb_index.h"

/**
 * The version of the internal data structures (and thus file format).
 * Bump this when any of the structs in this file change.
 */
#define LOGDB_VERSION 1

/**
 * The magic cookie appearing at byte 0 of the database file.
 */
#define LOGDB_MAGIC "LOGD"

/**
 * The size of the database file sections that are reserved
 * in the index file.
 */
#define LOGDB_SECTION_SIZE 65536 /* bytes */

/**
 * The number of sections to preallocate in the database file.
 */
#define LOGDB_PREALLOCATE 4 /* sections */

/**
 * The minimum size for a valid database file
 */
#define LOGDB_MIN_SIZE (sizeof(logdb_header_t) + sizeof(logdb_index_header_t) + sizeof(logdb_trailer_t))

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


/**
 * Internal struct that represents the header of the database file.
 */
typedef struct {
	char magic[sizeof(LOGDB_MAGIC) - 1]; /* LOGDB_MAGIC */
	unsigned int version;
} logdb_header_t;

/**
 * Internal struct that represents the trailer of the database file.
 */
typedef struct {
	unsigned int index_offset; /**< offset from the end of the db where the index starts */
} logdb_trailer_t;

/**
 * Internal struct that holds the state of a LogDB connection.
 */
typedef struct {
	unsigned int version; /**< version of logdb structure. Should equal LOGDB_VERSION */

	int fd; /**< file descriptor of database file */
	logdb_index_t* index; /**< struct containing fd and metadata about the index file */

	pthread_rwlock_t lock; /**< protects threaded access to this `logdb_connection_t` */
} logdb_connection_t;

#endif /* LOGDB_INTERNAL_H */
