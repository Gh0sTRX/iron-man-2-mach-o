/**
 * @file darwin.h
 * @brief Darwin POSIX/libc/pthreads subsystem bridge for PS Vita.
 */
#pragma once
#include <stddef.h>

void darwin_init(void);
const char *darwin_path(const char *in, char *out, size_t outlen);
// Aperturas de archivo del juego (fopen/open) y la última ruta: el watchdog de uikit.c no corta una carga larga.
extern volatile unsigned darwin_io_ops;
extern char darwin_last_open[128];
