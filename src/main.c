#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "terminal/terminal.h"
#include "signals/signals.h"
#include "log/log.h"
#include "help.h"

typedef enum event_type {
    TERMINAL_READ,
    TERMINAL_WRITE,
} event_type;

typedef int (*handle_event)(terminal *self);

static const handle_event handlers[] = {
    terminal_read,
    terminal_write,
};

int main(int argc, char **argv) {

    int   row_count   = 24;
    int   col_count   = 80;
    int   enable_json = 0;
    char *filename    = NULL;
    char *args[0x100] = { getenv("SHELL"), NULL };
    int   args_index  = 0;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-h")     == 0 ||
            strcmp(argv[i], "--help") == 0) {

            puts(HELP_CONTENT);

            return 0;
        } else
        if (strcmp(argv[i], "-o")       == 0 ||
            strcmp(argv[i], "--output") == 0) {

            if (++i >= argc) {
                LOG_ERROR("Invalid arguments");

                return 1;
            }

            filename = argv[i];
        } else 
        if (strcmp(argv[i], "-s")     == 0 ||
            strcmp(argv[i], "--size") == 0) {

            if (++i >= argc) {
                LOG_ERROR("Invalid arguments");

                return 1;
            }

            sscanf(argv[i], "%dx%d", &col_count, &row_count);
        } else 
        if (strcmp(argv[i], "--json") == 0) {

            enable_json = 1;
        } else while (i < argc) {

            args[args_index++] = argv[i++];
            args[args_index]   = NULL;
        }
    }

    signals_init();

    terminal t;

    terminal_open(&t, row_count, col_count, enable_json, filename, args);

    int epfd = epoll_create1(0);

    if (epfd < 0) goto cleanup;

    struct epoll_event r_ev = {
        .events   = EPOLLIN,
        .data.u32 = TERMINAL_READ,
    };

    struct epoll_event w_ev = {
        .events   = EPOLLIN,
        .data.u32 = TERMINAL_WRITE,
    };

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, t.master_fd,  &r_ev) == -1) goto cleanup;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &w_ev) == -1) goto cleanup;

    struct epoll_event events[2];

    while (signals_stat()) {

        int n = epoll_wait(epfd, events, 2, -1);

        if (n < 0) {
            if (errno == EINTR) continue;

            goto cleanup;
        }

        for (int i = 0; i < n; i++) {

            if (handlers[events[i].data.u32](&t)) signals_stop(0);
        }

        terminal_print(&t);
    }

    close(epfd);

    terminal_close(&t);

    return 0;

cleanup:
    LOG_ERROR("%s", strerror(errno));

    if (epfd >= 0) close(epfd);

    terminal_close(&t);

    return 1;
}
