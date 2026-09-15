#pragma once

int utf8_to_json(const char *src, int src_len, char *dst, int dst_size, int *consumed);
int json_to_utf8(const char *src, int src_len, char *dst, int dst_size, int *consumed);