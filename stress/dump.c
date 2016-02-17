
#include "logdb.h"

#include <stdio.h>
#include <libgen.h>

int main (int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s [file]\n\nWhere:\n\n", basename(argv[0]));
		printf("\t[file]\t\tThe name of the db file to dump\n");
		return 1;
	}

	logdb_connection* conn = logdb_open (argv[1], LOGDB_OPEN_EXISTING);
	if (!conn) {
		printf("logdb_open failed\n");
		return 2;
	}

	logdb_iter* iter = logdb_iter_all (conn);
	if (!iter) {
		printf("logdb_iter_all failed\n");
		return 8;
	}

	while (logdb_iter_next (iter)) {
		logdb_buffer* key = logdb_iter_current_key (iter);
		logdb_buffer* val = logdb_iter_current_value (iter);
		if (!key || !val) {
			printf("logdb_iter_current_key or logdb_iter_current_value failed\n");
			return 9;
		}

		const char* k = (const char*)logdb_buffer_data (key);
		const int* v = (const int*)logdb_buffer_data (val);
		if (!k || !v) {
			printf("logdb_buffer_data failed\n");
			return 10;
		}

		printf("%s: %04d\n", k, *v);
	}

	logdb_iter_free (iter);
	return logdb_close (conn);
}