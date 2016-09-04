
#include "logdb.h"

#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

static logdb_connection* conn;
static pthread_t* threads;
static int count;

static void* thread_func (void* arg)
{
	const char* key = (const char*)arg;
	logdb_buffer* keybuf = logdb_buffer_new_direct (arg, strlen(key) + 1, &free);

	for (int i = 0; i < count; i++) {
		logdb_buffer* valuebuf = logdb_buffer_new_copy (&i, sizeof (int));

		if (logdb_put (conn, keybuf, valuebuf) != 0)
			printf("logdb_put failed on iter %d of thread %s\n", i, key);

		logdb_buffer_free (valuebuf);
	}
	
	logdb_buffer_free (keybuf);
	return NULL;
}

int main (int argc, char **argv) {
	if (argc != 5) {
		printf("Usage: %s [file] [key] [threads] [count]\n\nWhere:\n\n", basename(argv[0]));
		printf("\t[file]\t\tThe name of the db file to open/create\n");
		printf("\t[key]\t\tThe prefix to use for the keys added by this process\n");
		printf("\t\t\t  - Env var LOGDB_STRESS_KEY_PREFIX will override this. Suffix will have thread # unless LOGDB_STRESS_KEY_SUFFIX is set.\n");
		printf("\t[threads]\tThe number of threads to create in this process\n");
		printf("\t[count]\t\tThe number of iterations per thread\n");
		return 1;
	}
	
	const char* file = argv[1];
	const char* keyprefix = getenv("LOGDB_STRESS_KEY_PREFIX");
	if (!keyprefix)
		keyprefix = argv[2];

	int threadcnt = atoi(argv[3]);
	if (!threadcnt) {
		printf("Invalid number of threads!\n");
		return 2;
	}

	count = atoi(argv[4]);
	if (!count) {
		printf("Invalid iterations per thread!\n");
		return 2;
	}

	threads = (pthread_t*)malloc (threadcnt * sizeof (pthread_t));
	if (!threads) {
		perror("malloc 1");
		return 3;
	}

	conn = logdb_open (file, LOGDB_OPEN_CREATE);
	if (!conn) {
		printf("logdb_open failed\n");
		return 4;
	}

	for (int i = 0; i < threadcnt; i++) {
		const char* keysuffix = getenv("LOGDB_STRESS_KEY_SUFFIX");
		char* key = (char*)malloc (strlen (keyprefix) + (keysuffix? strlen (keysuffix) : 10));
		if (!key) {
			perror("malloc 2");
			(void)logdb_close (conn);
			return 5;
		}
		if (keysuffix) {
			sprintf (key, "%s%s", keyprefix, keysuffix);
		} else {
			sprintf (key, "%s:t%d", keyprefix, i);
		}
		int err = pthread_create (&threads[i], NULL, &thread_func, key);
		if (err) {
			printf("pthread_create: %s\n", strerror(err));
			(void)logdb_close (conn);
			return 6;
		}
	}

	for (int i = 0; i < threadcnt; i++) {
		int err = pthread_join (threads[i], NULL);
		if (err) {
			printf("pthread_join: %s\n", strerror(err));
			(void)logdb_close (conn);
			return 7;
		}
	}

	return logdb_close (conn);
}
