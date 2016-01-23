#ifndef LOGDB_TESTS_H
#define LOGDB_TESTS_H

#include "logdb.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#define TEST(name) static int name () { printf("%s: ", #name);
#define PASS printf(" pass!\n"); return 0; }

#define ASSERTF(cond, fmt, ...) if(!(cond)) { printf(" FAIL!\n\nFailed `ASSERT(%s)` at %s:%d\n" fmt "\n", #cond, __FILE__, __LINE__, ##__VA_ARGS__); return (__COUNTER__ + 1); }
#define ASSERT(cond) ASSERTF(cond, "")

#endif /* LOGDB_TESTS_H */