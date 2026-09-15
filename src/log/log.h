#pragma once

#include <stdio.h>

#define LOG_ERROR(fmt, ...) fprintf(stderr, "\033[31mERROR: " fmt " [%s:%d] [%s]\033[0m\n", ##__VA_ARGS__, __FILE__, __LINE__, __func__)
#define LOG_INFO(fmt, ...)  fprintf(stdout, "\033[32mINFO:  " fmt "\033[0m\n", ##__VA_ARGS__)