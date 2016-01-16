
/** An opaque pointer to a LogDB connection. */
typedef void logdb_connection;

/** Flags that can be passed to logdb_open */
enum logdb_open_flags {
    
};

/**
 * Opens a new connection to a LogDB database file.
 */
logdb_connection* logdb_open ()