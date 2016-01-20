#ifndef LOGDB_INDEX_H
#define LOGDB_INDEX_H

/**
 * The suffix applied to the database file name to derive the
 * index file name.
 */
#define LOGDB_INDEX_FILE_SUFFIX "-index"

/**
 * The magic cookie appearing at byte 0 of the index file.
 */
#define LOGDB_INDEX_MAGIC "LDBI"

typedef struct {
	int fd;
	char* path; /* needed to unlink index */
	unsigned int len;
} logdb_index_t;

typedef struct {
	char magic[sizeof(LOGDB_INDEX_MAGIC) - 1]; /* LOGDB_INDEX_MAGIC */
	unsigned int version; /* LOGDB_VERSION */
	unsigned int len; /**< number of entries in the index */

	/* len logdb_index_entry_t structs follow */
} logdb_index_header_t;

/**
 * Internal struct that holds an entry in the index file.
 */
typedef struct {
	unsigned short len; /**< number of bytes that are valid in this section */
} logdb_index_entry_t;

/**
 * Opens the index file at the given path.
 * \returns The index, or NULL if it could not be opened.
 */
logdb_index_t* logdb_index_open (const char* path);

/**
 * Creates an index file for the database file open on the given fd.
 * \param path The path at which to create the index.
 * \param dbfd The file descriptor for the database for which to create the index.
 * \returns The index, or NULL if it could not be created.
 */
logdb_index_t* logdb_index_create (const char* path, int dbfd);

/**
 * Closes the given index.
 * \returns Zero (0) on success.
 */
int logdb_index_close (logdb_index_t* index);

/**
 * Closes the given index, merging it back into the database file
 *  open on the given fd.
 * \returns Zero (0) on success.
 */
int logdb_index_close_merge (logdb_index_t* index, int dbfd);

#endif /* LOGDB_INDEX_H */
