
#include "logdb_tests.h"
#include "testcases.def"

int main (int argc, char **argv) {
    int result = 0;

#   undef TEST
#   define TEST(name) result |= name(); while(0) {
#   include "testcases.def"

    return result;
}
