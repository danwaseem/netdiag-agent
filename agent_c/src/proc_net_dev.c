#define _GNU_SOURCE
#include "proc_net_dev.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_line(const char* line, struct iface_stats* st) {
    const char* p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    const char* colon = strchr(p, ':');
    if (!colon) {
        return -1;
    }

    size_t name_len = (size_t)(colon - p);
    if (name_len >= IFACE_NAME_LEN) {
        name_len = IFACE_NAME_LEN - 1;
    }
    memcpy(st->name, p, name_len);
    st->name[name_len] = '\0';

    unsigned long long rx_bytes = 0, rx_packets = 0, rx_errs = 0;
    unsigned long long tx_bytes = 0, tx_packets = 0, tx_errs = 0;

    int n = sscanf(colon + 1, " %llu %llu %llu %*u %*u %*u %*u %*u %llu %llu %llu", &rx_bytes,
                   &rx_packets, &rx_errs, &tx_bytes, &tx_packets, &tx_errs);

    if (n < 6) {
        char* dup = strdup(colon + 1);
        if (!dup) {
            return -1;
        }

        unsigned long long vals[32];
        size_t i = 0;

        char* saveptr = NULL;
        char* tok = strtok_r(dup, " \t\n", &saveptr);
        while (tok && i < 32) {
            char* end = NULL;
            errno = 0;
            unsigned long long v = strtoull(tok, &end, 10);
            if (errno == 0 && end && end != tok) {
                vals[i++] = v;
            } else {
                vals[i++] = 0;
            }
            tok = strtok_r(NULL, " \t\n", &saveptr);
        }
        free(dup);

        if (i >= 1)
            rx_bytes = vals[0];
        if (i >= 3)
            rx_errs = vals[2];
        if (i >= 9)
            tx_bytes = vals[8];
        if (i >= 11)
            tx_errs = vals[10];
    }

    st->rx_bytes = (uint64_t)rx_bytes;
    st->rx_errs = (uint64_t)rx_errs;
    st->tx_bytes = (uint64_t)tx_bytes;
    st->tx_errs = (uint64_t)tx_errs;
    return 0;
}

int parse_proc_net_dev(const char* path, struct iface_stats* out, size_t max, size_t* count) {
    if (!path || !out || !count || max == 0) {
        return EINVAL;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        return errno;
    }

    char* line = NULL;
    size_t len = 0;

    // Skip headers (2 lines). If file is malformed, still proceed safely.
    ssize_t r1 = getline(&line, &len, f);
    ssize_t r2 = getline(&line, &len, f);
    if (r1 < 0 || r2 < 0) {
        // Too short or read error; treat as empty.
        free(line);
        fclose(f);
        *count = 0;
        return 0;
    }

    size_t idx = 0;
    while (getline(&line, &len, f) != -1) {
        if (idx >= max)
            break;
        if (parse_line(line, &out[idx]) == 0)
            idx++;
    }

    free(line);
    fclose(f);
    *count = idx;
    return 0;
}
