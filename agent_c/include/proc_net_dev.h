#ifndef PROC_NET_DEV_H
#define PROC_NET_DEV_H

#include <stddef.h>
#include <stdint.h>

#define IFACE_NAME_LEN 64

struct iface_stats {
    char name[IFACE_NAME_LEN];
    uint64_t rx_bytes;
    uint64_t rx_errs;
    uint64_t tx_bytes;
    uint64_t tx_errs;
};

int parse_proc_net_dev(const char* path, struct iface_stats* out, size_t max, size_t* count);

#endif
