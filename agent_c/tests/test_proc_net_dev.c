#include "../include/proc_net_dev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void tassert(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "ASSERT FAILED: %s\n", msg);
        exit(1);
    }
}

int main(void) {
    struct iface_stats arr[16];
    size_t count = 0;

    int rc = parse_proc_net_dev("tests/fixtures/proc_net_dev_sample.txt", arr, 16, &count);
    tassert(rc == 0, "parse_proc_net_dev should return 0");
    tassert(count == 2, "expected 2 interfaces parsed");

    tassert(strcmp(arr[0].name, "lo") == 0, "expected first iface lo");
    tassert(arr[0].rx_bytes == 12345ULL, "lo rx_bytes should be 12345");

    tassert(strcmp(arr[1].name, "eth0") == 0, "expected second iface eth0");
    tassert(arr[1].rx_errs == 2ULL, "eth0 rx_errs");
    tassert(arr[1].tx_errs == 3ULL, "eth0 tx_errs");

    printf("C tests passed: %zu interfaces parsed\n", count);
    return 0;
}
