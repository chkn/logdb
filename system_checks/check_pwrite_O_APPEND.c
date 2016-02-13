/*
Tests that `pwrite` doesn't suffer from https://bugzilla.kernel.org/show_bug.cgi?id=43178
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


int
main(int argc, char *argv[])
{
    int fd = open("test.dat", O_RDWR | O_CREAT | O_TRUNC | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    /* First write 10 "b"s */
    const char* buf = "bbbbbbbbbb";
    if (write(fd, buf, 10) == -1) {
        perror("write");
        return 1;
    }

    /* Now overwrite first 5 "b"s with "a"s */
    buf = "aaaaa";
    if (pwrite(fd, buf, 5, 0) == -1) {
        perror("pwrite");
        return 1;
    }

    fsync(fd);

    char result[10];
    if (pread(fd, result, 10, 0) == -1) {
        perror("pread");
        return 2;
    }

    int ev = strncmp(result, "aaaaabbbbb", 10);
    if (!ev)
        unlink("test.dat");
    return (ev == 0)? 0 : 3;
}
