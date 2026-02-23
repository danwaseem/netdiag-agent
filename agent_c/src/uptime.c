#define _GNU_SOURCE
#include "uptime.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

long read_uptime_seconds(const char* path, int* err) {
    const char* p = path ? path : "/proc/uptime";
    FILE* f = fopen(p, "r");
    if (!f) {
        if (err)
            *err = errno;
        return -1;
    }

    double up = 0.0;
    if (fscanf(f, "%lf", &up) != 1) {
        fclose(f);
        if (err)
            *err = EIO;
        return -1;
    }

    fclose(f);
    if (err)
        *err = 0;
    return (long)up;
}

int read_operstate(const char* iface, char* out, size_t outlen) {
    if (!iface || !out || outlen == 0)
        return EINVAL;

    char path[256];
    int n = snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
    if (n <= 0 || (size_t)n >= sizeof(path))
        return EINVAL;

    FILE* f = fopen(path, "r");
    if (!f)
        return errno;

    if (!fgets(out, (int)outlen, f)) {
        fclose(f);
        return EIO;
    }
    fclose(f);

    size_t l = strlen(out);
    if (l > 0 && out[l - 1] == '\n')
        out[l - 1] = '\0';
    return 0;
}
