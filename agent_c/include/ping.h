#ifndef PING_H
#define PING_H

struct ping_result {
    double avg_ms;
    double loss_pct;
    int sent;
    int received;
};

int measure_ping(const char* target, struct ping_result* out);

#endif
