#pragma once

#include <signal.h>

void signals_init();
void signals_stop(int sig);
int  signals_stat();