#ifndef LOGDB_INTERNAL_H
#define LOGDB_INTERNAL_H

#include "logdb.h"

#define NULL ((void*)0)

#ifndef LOG
#ifdef DEBUG
#include <stdio.h>
#define LOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif
#endif

/**
 * Internal struct that holds the state of a LogDB connection.
 */
typedef struct {
	int version; /**< version of logdb structure (1) */
	void* data;
	int len; /**< length of mmap */
} logdb_connection_t;

#endif /* LOGDB_INTERNAL_H */
