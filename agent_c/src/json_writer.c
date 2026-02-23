#define _GNU_SOURCE
#include "json_writer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int atomic_write_json(const char* path, const char* content) {
    if (!path || !content)
        return EINVAL;

    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmpXXXXXX", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp))
        return EINVAL;

    int fd = mkstemp(tmp);
    if (fd < 0)
        return errno;

    ssize_t want = (ssize_t)strlen(content);
    ssize_t wrote = write(fd, content, (size_t)want);
    if (wrote != want) {
        close(fd);
        (void)unlink(tmp);
        return EIO;
    }

    (void)fsync(fd);
    close(fd);

    if (rename(tmp, path) != 0) {
        (void)unlink(tmp);
        return errno;
    }
    return 0;
}
