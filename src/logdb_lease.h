#ifndef LOGDB_LEASE_H
#define LOGDB_LEASE_H

#include "logdb_internal.h"

/**
 * Internal structure representing a lease that a thread has to write
 *  in a portion of a database file.
 */
typedef struct {
    unsigned int index; /**< index of the segement in the index on which this lease is held */
    
} logdb_lease_t;

#endif /* LOGDB_LEASE_H */