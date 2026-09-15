#define _XOPEN_SOURCE 600

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log/log.h"
#include "encoding/json.h"
#include "encoding/utf8.h"
#include "terminal.h"

int terminal_open(
    terminal *self, 
    int       row_count, 
    int       col_count, 
    int       enable_json,
    char     *filename,
    char     *args[]) {

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);

    if (flags == -1) return 1;

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) return 1;

    self->master_fd         = -1;
    self->output_fd         = -1;
    self->slave_pid         = -1;
    self->vt                = NULL;
    self->screen            = NULL;
    self->row_count         = row_count;
    self->col_count         = col_count;
    self->input_buffer_len  = 0;
    self->output_buffer_len = 0;
    self->enable_json       = enable_json;
    self->output_file       = NULL;

    int slave_fd = -1;

    self->master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (self->master_fd < 0) goto cleanup;

    if (grantpt(self->master_fd)  == -1) goto cleanup;
    if (unlockpt(self->master_fd) == -1) goto cleanup;

    char *slave_name = ptsname(self->master_fd);

    if (slave_name == NULL) goto cleanup;

    if ((slave_fd = open(slave_name, O_RDWR | O_NOCTTY)) < 0) goto cleanup;

    pid_t pid = fork();

    if (pid < 0) goto cleanup;

    if (pid == 0) {

        pid_t ppid = getppid();

        if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) _exit(1);

        if (getppid() != ppid) _exit(1);

        if (setsid() == -1) _exit(1);

        struct winsize ws = {
            .ws_row    = self->row_count,
            .ws_col    = self->col_count,
            .ws_xpixel = 0,
            .ws_ypixel = 0,
        };

        if (ioctl(slave_fd, TIOCSWINSZ, &ws) == -1) _exit(1);

        if (dup2(slave_fd, STDIN_FILENO)  == -1) _exit(1);
        if (dup2(slave_fd, STDOUT_FILENO) == -1) _exit(1);
        if (dup2(slave_fd, STDERR_FILENO) == -1) _exit(1);

        close(self->master_fd);
        close(slave_fd);

        execvp(args[0], args);

        _exit(1);
    }

    self->slave_pid = pid;

    close(slave_fd);

    self->vt = vterm_new(self->row_count, self->col_count);

    if (self->vt == NULL) goto cleanup;

    vterm_set_utf8(self->vt, 1);

    self->screen = vterm_obtain_screen(self->vt);

    if (self->screen == NULL) goto cleanup;

    vterm_screen_reset(self->screen, 1);

    if (filename == NULL) return 0;

    self->output_file_length = self->row_count * self->col_count * 8;

    self->output_fd = open(filename, O_RDWR | O_CREAT, 0644);

    if (self->output_fd < 0) goto cleanup;

    if (ftruncate(self->output_fd, self->output_file_length) == -1) goto cleanup;

    self->output_file = mmap(NULL, self->output_file_length, PROT_READ | PROT_WRITE, MAP_SHARED, self->output_fd, 0);

    if (self->output_file == MAP_FAILED) goto cleanup;

    return 0;

cleanup:
    LOG_ERROR("%s", strerror(errno));

    if (self->output_file != NULL || self->output_file != MAP_FAILED)
        munmap(self->output_file, self->output_file_length);

    if (self->vt != NULL) vterm_free(self->vt);

    if (self->slave_pid > 0) kill(self->slave_pid, SIGHUP);

    if (slave_fd        >= 0) close(slave_fd);
    if (self->output_fd >= 0) close(self->output_fd);
    if (self->master_fd >= 0) close(self->master_fd);

    self->master_fd   = -1;
    self->slave_pid   = -1;
    self->vt          = NULL;
    self->screen      = NULL;
    self->output_file = NULL;

    return 1;
}

void terminal_close(terminal *self) {

    if (self->output_file != NULL) 
        munmap(self->output_file, self->output_file_length);

    if (self->vt != NULL) vterm_free(self->vt);

    if (self->slave_pid > 0) {

        kill(self->slave_pid, SIGHUP);

        waitpid(self->slave_pid, NULL, 0);
    }

    if (self->master_fd >= 0) close(self->master_fd);

    self->master_fd   = -1;
    self->output_fd   = -1;
    self->slave_pid   = -1;
    self->vt          = NULL;
    self->screen      = NULL;
    self->output_file = NULL;
}

int terminal_read(terminal *self) {

    char buffer[0x1000];

    while (1) {

        ssize_t n = read(self->master_fd, buffer, sizeof(buffer));

        if (n == 0) return 0;

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;

            if (errno == EINTR) continue;

            LOG_ERROR("%s", strerror(errno));

            return 1;
        }

        vterm_input_write(self->vt, buffer, n);
    }

    return 0;
}

static int need_print = 0;

static int remove_ln(char *buffer, ssize_t *len) {

    int ln_count = 0;

    for (int i = 0; i < *len; i++) {

        buffer[i - ln_count] = buffer[i];

        if (buffer[i] != '\n') continue;

        ln_count++;
    }

    *len -= ln_count;

    return ln_count;
}

int terminal_write(terminal *self) {

    int stdin_is_end = 0;

    while (1) {

        if (sizeof(self->input_buffer) - self->input_buffer_len == 0 ||
            stdin_is_end) goto write;

        ssize_t n = read(STDIN_FILENO, 
            self->input_buffer         + self->input_buffer_len, 
            sizeof(self->input_buffer) - self->input_buffer_len);

        if (n == 0) {

            stdin_is_end = 1;

            goto write;
        }

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) goto write;

            if (errno == EINTR) continue;

            LOG_ERROR("%s", strerror(errno));

            stdin_is_end = 1;

            goto write;
        }

        need_print = remove_ln(self->input_buffer + self->input_buffer_len, &n);

        self->input_buffer_len += n;

        int consumed;

        int len = json_to_utf8(
            self->input_buffer, self->input_buffer_len, 
            self->output_buffer + self->output_buffer_len, 
            sizeof(self->output_buffer) - self->output_buffer_len, 
            &consumed);

        if (len < 0) {

            LOG_ERROR("Failed to parse json");

            goto write;
        }

        self->input_buffer_len  -= consumed;
        self->output_buffer_len += len;

        if (self->input_buffer_len > 0) memmove(self->input_buffer, 
            self->input_buffer + consumed, self->input_buffer_len);

write:
        if (self->output_buffer_len == 0) break;

        n = write(self->master_fd, self->output_buffer, 
            self->output_buffer_len);

        if (n == 0) break;

        if (n < 0) {
            if (errno == EINTR) goto write;

            if (errno == EAGAIN || errno == EWOULDBLOCK) break;

            LOG_ERROR("%s", strerror(errno));

            return 1;
        }

        self->output_buffer_len -= n;

        if (self->output_buffer_len > 0) memmove(self->output_buffer,
            self->output_buffer + n, self->output_buffer_len);
    }

    return stdin_is_end && self->output_buffer_len == 0;
}

void terminal_print(terminal *self) {

    if (!need_print) return;

    need_print = 0;

    char  buffer[self->row_count * self->col_count * 8];
    char *p_buffer = buffer;

    for (int row = 0; row < self->row_count; row++) {

        int space_count = 0;

        for (int col = 0; col < self->col_count; col++) {

            VTermPos pos = {
                .row = row,
                .col = col
            };

            VTermScreenCell cell;

            vterm_screen_get_cell(self->screen, pos, &cell);

            char utf8[5];

            cell_to_utf8(cell.chars[0], utf8);

            if (strcmp(utf8, " ") == 0) {

                space_count++;

                continue;
            }

            memset(p_buffer, ' ', space_count);

            p_buffer += space_count;

            space_count = 0;

            int len = strlen(utf8);

            memcpy(p_buffer, utf8, len);

            p_buffer += len;
        }

        *p_buffer = '\n';

        p_buffer++;
    }

    *p_buffer = '\0';

    int buffer_len = p_buffer - buffer;

    p_buffer = buffer;

    if (self->enable_json) {

        int  dst_size = buffer_len * 6 + 1;
        char dst[dst_size];
        int  consumed;

        buffer_len = utf8_to_json(buffer, buffer_len, dst, dst_size, &consumed);

        p_buffer = dst;
    }

    if (self->output_file == NULL) {

        puts(p_buffer);

        return;
    }

    ftruncate(self->output_fd, buffer_len);
    memcpy(self->output_file, p_buffer, buffer_len);
    msync(self->output_file, buffer_len, MS_SYNC);
}