
#include "logdb_txn.h"
#include "logdb_connection.h"

#include <stdlib.h>
#include <pthread.h>


static int logdb_txn_rollback (logdb_txn_t* txn)
{
	
}

int logdb_begin LOGDB_VERIFY_CONNECTION(logdb_connection* conn)
{
	return -1;
}}

int logdb_commit LOGDB_VERIFY_CONNECTION(logdb_connection* conn)
{
	return -1;    
}}

int logdb_rollback LOGDB_VERIFY_CONNECTION(logdb_connection* conn)
{
	return -1;
}}

void logdb_txn_destruct (void* current_txn)
{
	/* If there is a current transaction active when a thread
	    exits, rollback the transaction. */
	logdb_txn_t* txn = (logdb_txn_t*)current_txn;
	if (txn)
		logdb_txn_rollback (txn);
}
