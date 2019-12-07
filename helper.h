#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int makeargv(const char *s, const char *delimiters, char ***argvp);

ssize_t r_read(int fd, void *buf, size_t size);

ssize_t r_write(int fd, void *buf, size_t size);