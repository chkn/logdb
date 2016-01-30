#ifndef LOGDB_DATA_H
#define LOGDB_DATA_H

/**
 * Internal structure that represents the header
 * of a record in the database.
 */
typedef struct {
    unsigned int keylen;
    unsigned int valuelen;
} logdb_data_header_t;

#endif /* LOGDB_DATA_H */