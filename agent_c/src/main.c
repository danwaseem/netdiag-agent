#define _GNU_SOURCE
#include "json_writer.h"
#include "ping.h"
#include "proc_net_dev.h"
#include "uptime.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_sig(int sig) {
    (void)sig;
    keep_running = 0;
}

static int getenv_int_default(const char* name, int def) {
    const char* v = getenv(name);
    if (!v || *v == '\0')
        return def;
    char* end = NULL;
    errno = 0;
    long val = strtol(v, &end, 10);
    if (errno != 0 || end == v || val <= 0 || val > 3600)
        return def;
    return (int)val;
}

int main(void) {
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    const char* target = getenv("NETDIAG_TARGET");
    if (!target || *target == '\0')
        target = "8.8.8.8";
    int interval = getenv_int_default("NETDIAG_INTERVAL", 10);

    const char* iface = getenv("NETDIAG_IFACE");
    if (iface && *iface == '\0')
        iface = NULL;

    const char* outpath = "/tmp/netdiag.json";

    while (keep_running) {
        time_t now = time(NULL);
        struct tm tmv;
        gmtime_r(&now, &tmv);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmv);

        int uerr = 0;
        long uptime = read_uptime_seconds("/proc/uptime", &uerr);
        if (uptime < 0)
            fprintf(stderr, "WARN uptime read failed (err=%d)\n", uerr);

        struct iface_stats stats[64];
        size_t count = 0;
        int prc = parse_proc_net_dev("/proc/net/dev", stats, 64, &count);
        if (prc != 0) {
            fprintf(stderr, "WARN parse /proc/net/dev failed (rc=%d)\n", prc);
            count = 0;
        }

        struct iface_stats sel;
        memset(&sel, 0, sizeof(sel));
        int have_sel = 0;
        if (iface) {
            for (size_t i = 0; i < count; i++) {
                if (strcmp(stats[i].name, iface) == 0) {
                    sel = stats[i];
                    have_sel = 1;
                    break;
                }
            }
        }

        char oper[32] = "unknown";
        if (iface) {
            int orc = read_operstate(iface, oper, sizeof(oper));
            if (orc != 0) {
                fprintf(stderr, "WARN operstate read failed for %s (rc=%d)\n", iface, orc);
                strncpy(oper, "unknown", sizeof(oper));
                oper[sizeof(oper) - 1] = '\0';
            }
        }

        struct ping_result pingr;
        int mrc = measure_ping(target, &pingr);
        if (mrc != 0) {
            fprintf(stderr, "WARN ping measurement failed (rc=%d)\n", mrc);
            pingr.avg_ms = -1.0;
            pingr.loss_pct = 100.0;
            pingr.sent = 0;
            pingr.received = 0;
        }

        char json[8192];
        int off = 0;

        off +=
            snprintf(json + off, sizeof(json) - (size_t)off,
                     "{"
                     "\"timestamp\":\"%s\","
                     "\"target\":\"%s\","
                     "\"uptime\":%ld,"
                     "\"ping\":{"
                     "\"avg_ms\":%.3f,"
                     "\"loss_pct\":%.2f,"
                     "\"sent\":%d,"
                     "\"received\":%d"
                     "},"
                     "\"interfaces\":[",
                     ts, target, uptime, pingr.avg_ms, pingr.loss_pct, pingr.sent, pingr.received);

        for (size_t i = 0; i < count; i++) {
            off += snprintf(json + off, sizeof(json) - (size_t)off,
                            "{\"name\":\"%s\",\"rx_bytes\":%llu,\"rx_errs\":%llu,"
                            "\"tx_bytes\":%llu,\"tx_errs\":%llu}%s",
                            stats[i].name, (unsigned long long)stats[i].rx_bytes,
                            (unsigned long long)stats[i].rx_errs,
                            (unsigned long long)stats[i].tx_bytes,
                            (unsigned long long)stats[i].tx_errs, (i + 1 < count) ? "," : "");
        }

        off += snprintf(json + off, sizeof(json) - (size_t)off,
                        "],"
                        "\"iface_sel\":{"
                        "\"provided\":%s,"
                        "\"name\":\"%s\","
                        "\"operstate\":\"%s\","
                        "\"rx_bytes\":%llu,"
                        "\"tx_bytes\":%llu,"
                        "\"rx_errs\":%llu,"
                        "\"tx_errs\":%llu"
                        "}"
                        "}\n",
                        iface ? "true" : "false", have_sel ? sel.name : "", oper,
                        (unsigned long long)sel.rx_bytes, (unsigned long long)sel.tx_bytes,
                        (unsigned long long)sel.rx_errs, (unsigned long long)sel.tx_errs);

        fputs(json, stdout);
        fflush(stdout);
        (void)atomic_write_json(outpath, json);

        for (int i = 0; i < interval && keep_running; i++)
            sleep(1);
    }

    fprintf(stderr, "netdiag_agent exiting\n");
    return 0;
}
