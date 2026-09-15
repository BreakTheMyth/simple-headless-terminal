#include "signals.h"
#include "log/log.h"

static volatile sig_atomic_t running = 1;

void signals_init() {

    signal(SIGINT,  signals_stop);
    signal(SIGTERM, signals_stop);
}

void signals_stop(int sig) {

    LOG_INFO("%s", "Stopping...");

    running = 0;
}

int signals_stat() {

    return running;
}