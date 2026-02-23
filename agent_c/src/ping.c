#define _GNU_SOURCE
#include "ping.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>

#if defined(__linux__)
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#elif defined(__APPLE__)
#include <netinet/ip.h>
#include <netinet/ip_icmp.h> // struct icmp
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int parse_ping_output(FILE* f, struct ping_result* out) {
    char buf[512];
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "packet loss")) {
            unsigned long sent = 0, recv = 0;
            double loss = 100.0;

            // Linux iputils format:
            // "4 packets transmitted, 4 received, 0% packet loss, time ..."
            if (sscanf(buf, "%lu packets transmitted, %lu received, %lf%% packet loss", &sent,
                       &recv, &loss) >= 2) {
                out->sent = (int)sent;
                out->received = (int)recv;
                out->loss_pct = loss;
                continue;
            }

            // macOS format often includes "packets received"
            // "4 packets transmitted, 4 packets received, 0.0% packet loss"
            sent = 0;
            recv = 0;
            loss = 100.0;
            if (sscanf(buf, "%lu packets transmitted, %lu packets received, %lf%% packet loss",
                       &sent, &recv, &loss) >= 2) {
                out->sent = (int)sent;
                out->received = (int)recv;
                out->loss_pct = loss;
                continue;
            }
        }

        // Linux:
        // "rtt min/avg/max/mdev = 9.8/10.0/10.2/0.1 ms"
        if (strstr(buf, "rtt min/avg/max")) {
            double minv = 0, avgv = 0, maxv = 0, mdev = 0;
            char* eq = strchr(buf, '=');
            if (eq && sscanf(eq + 1, " %lf/%lf/%lf/%lf", &minv, &avgv, &maxv, &mdev) == 4) {
                out->avg_ms = avgv;
            }
            continue;
        }

        // macOS:
        // "round-trip min/avg/max/stddev = 9.8/10.0/10.2/0.1 ms"
        if (strstr(buf, "round-trip min/avg/max")) {
            double minv = 0, avgv = 0, maxv = 0, stdv = 0;
            char* eq = strchr(buf, '=');
            if (eq && sscanf(eq + 1, " %lf/%lf/%lf/%lf", &minv, &avgv, &maxv, &stdv) == 4) {
                out->avg_ms = avgv;
            }
            continue;
        }
    }
    return 0;
}

static int fallback_exec_ping(const char* target, struct ping_result* out) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return errno;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return errno;
    }

    if (pid == 0) {
        (void)dup2(pipefd[1], STDOUT_FILENO);
        (void)dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

#if defined(__APPLE__)
        // macOS ping is typically /sbin/ping
        // -c 4 : count
        // -W 1000 : timeout in ms (per packet)
        char* const argv[] = {(char*)"/sbin/ping", (char*)"-c",   (char*)"4", (char*)"-W",
                              (char*)"1000",       (char*)target, NULL};
        execv("/sbin/ping", argv);
        _exit(127);
#else
        // Linux iputils ping usually /bin/ping
        // -c 4 : count
        // -W 1 : timeout in seconds (per packet)
        char* const argv[] = {(char*)"/bin/ping", (char*)"-c",   (char*)"4", (char*)"-W",
                              (char*)"1",         (char*)target, NULL};
        execv("/bin/ping", argv);
        _exit(127);
#endif
    }

    close(pipefd[1]);
    FILE* fr = fdopen(pipefd[0], "r");
    if (!fr) {
        close(pipefd[0]);
        (void)waitpid(pid, NULL, 0);
        return errno;
    }

    (void)parse_ping_output(fr, out);
    fclose(fr);
    (void)waitpid(pid, NULL, 0);
    return 0;
}

#if defined(__linux__)

static uint16_t icmp_checksum(const void* data, size_t len) {
    const uint16_t* p = (const uint16_t*)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += (uint16_t)(*(const uint8_t*)p);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static double tv_to_ms(const struct timeval* a, const struct timeval* b) {
    double sec = (double)(a->tv_sec - b->tv_sec);
    double usec = (double)(a->tv_usec - b->tv_usec);
    return sec * 1000.0 + usec / 1000.0;
}

static int resolve_ipv4_literal(const char* target, struct in_addr* out) {
    if (!target || !out) {
        return EINVAL;
    }
    if (inet_pton(AF_INET, target, out) == 1) {
        return 0;
    }
    return EINVAL;
}

static int raw_icmp_ping_ipv4(const char* target, struct ping_result* out) {
    struct in_addr dst;
    if (resolve_ipv4_literal(target, &dst) != 0) {
        return EINVAL;
    }

    int s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s < 0) {
        return errno; // EPERM/EACCES likely
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = dst;

    const int id = (int)(getpid() & 0xFFFF);
    int sent = 0;
    int received = 0;
    double sum_rtt = 0.0;

    struct echo_payload {
        uint64_t magic;
        struct timeval t0;
    };

    for (int seq = 0; seq < 4; seq++) {
        struct {
            struct icmphdr hdr;
            struct echo_payload payload;
        } pkt;

        memset(&pkt, 0, sizeof(pkt));
        pkt.hdr.type = ICMP_ECHO;
        pkt.hdr.code = 0;
        pkt.hdr.un.echo.id = (uint16_t)htons((uint16_t)id);
        pkt.hdr.un.echo.sequence = (uint16_t)htons((uint16_t)seq);
        pkt.payload.magic = 0x4E45544449414755ULL;
        gettimeofday(&pkt.payload.t0, NULL);

        pkt.hdr.checksum = 0;
        pkt.hdr.checksum = icmp_checksum(&pkt, sizeof(pkt));

        ssize_t w = sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr));
        if (w < 0) {
            int e = errno;
            close(s);
            return e;
        }
        sent++;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int sel = select(s + 1, &rfds, NULL, NULL, &tv);
        if (sel <= 0) {
            continue; // timeout or error -> treat as lost
        }

        uint8_t buf[1500];
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        ssize_t r = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
        if (r < 0) {
            continue;
        }

        if ((size_t)r < sizeof(struct iphdr) + sizeof(struct icmphdr)) {
            continue;
        }

        struct iphdr* ip = (struct iphdr*)buf;
        size_t ip_hl = (size_t)ip->ihl * 4u;
        if ((size_t)r < ip_hl + sizeof(struct icmphdr)) {
            continue;
        }

        struct icmphdr* ih = (struct icmphdr*)(buf + ip_hl);
        uint8_t* icmp_start = buf + ip_hl;
        size_t icmp_len = (size_t)r - ip_hl;

        if (ih->type != ICMP_ECHOREPLY) {
            continue;
        }
        if (ntohs(ih->un.echo.id) != (uint16_t)id) {
            continue;
        }
        if (ntohs(ih->un.echo.sequence) != (uint16_t)seq) {
            continue;
        }

        if (icmp_len < sizeof(struct icmphdr) + sizeof(struct echo_payload)) {
            continue;
        }

        struct echo_payload payload_copy;
        memcpy(&payload_copy, icmp_start + sizeof(struct icmphdr), sizeof(payload_copy));
        if (payload_copy.magic != 0x4E45544449414755ULL) {
            continue;
        }

        struct timeval t1;
        gettimeofday(&t1, NULL);

        double rtt = tv_to_ms(&t1, &payload_copy.t0);
        if (rtt < 0) {
            rtt = 0;
        }

        received++;
        sum_rtt += rtt;
    }

    close(s);

    out->sent = sent;
    out->received = received;
    out->loss_pct = (sent > 0) ? (100.0 * (double)(sent - received) / (double)sent) : 100.0;
    out->avg_ms = (received > 0) ? (sum_rtt / (double)received) : -1.0;

    return 0;
}

#else
// Non-Linux: no raw ICMP implementation here.
static int raw_icmp_ping_ipv4(const char* target, struct ping_result* out) {
    (void)target;
    (void)out;
    return ENOTSUP;
}
#endif

int measure_ping(const char* target, struct ping_result* out) {
    if (!target || !out) {
        return EINVAL;
    }

    out->avg_ms = -1.0;
    out->loss_pct = 100.0;
    out->sent = 0;
    out->received = 0;

    int rc = raw_icmp_ping_ipv4(target, out);
    if (rc == 0) {
        return 0;
    }

    // If raw isn't supported, isn't permitted, or target isn't IPv4 literal, fall back.
    if (rc == ENOTSUP || rc == EPERM || rc == EACCES || rc == EINVAL) {
        return fallback_exec_ping(target, out);
    }

    return fallback_exec_ping(target, out);
}