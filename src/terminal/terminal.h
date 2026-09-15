#pragma once

#include <sys/types.h>

#include "vterm.h"

typedef struct terminal {
    int          master_fd;
    int          output_fd;
    pid_t        slave_pid;
    int          row_count;
    int          col_count;
    VTerm       *vt;
    VTermScreen *screen;
    char         input_buffer[0x1000];
    int          input_buffer_len;
    char         output_buffer[0x10000];
    int          output_buffer_len;
    int          enable_json;
    char        *output_file;
    size_t       output_file_length;
} terminal;

int  terminal_open(terminal  *self, int row_count, int col_count, int enable_json, char *filename, char *args[]);
void terminal_close(terminal *self);
int  terminal_read(terminal  *self);
int  terminal_write(terminal *self);
void terminal_print(terminal *self);