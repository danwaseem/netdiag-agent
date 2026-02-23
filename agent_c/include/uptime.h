#ifndef UPTIME_H
#define UPTIME_H

#include <stddef.h>

long read_uptime_seconds(const char* path, int* err);
int read_operstate(const char* iface, char* out, size_t outlen);

#endif
