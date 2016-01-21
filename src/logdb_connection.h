#ifndef LOGDB_CONNECTION_H
#define LOGDB_CONNECTION_H

#include "logdb_internal.h"
#include "logdb_index.h"

#include <pthread.h>

/**
 * The magic cookie appearing at byte 0 of the database file.
 */
#define LOGDB_MAGIC "LOGD"

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
	pthread_rwlock_t lock; /**< protects threaded access to this `logdb_connection_t` */

	int fd; /**< file descriptor of database file */
	logdb_index_t* index; /**< struct containing fd and metadata about the index file */

} logdb_connection_t;

#endif /* LOGDB_CONNECTION_H */