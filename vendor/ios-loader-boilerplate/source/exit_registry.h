/* SPDX-License-Identifier: MIT. Guest process-lifetime callbacks, no dlclose. */
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef void (*Im2ExitCall)(void *,uintptr_t,uintptr_t,int);
int im2_exit_init(Im2ExitCall call,void *context);
int im2_exit_register(uintptr_t function,uintptr_t argument,uintptr_t dso,int has_argument);
int im2_exit_run(void);
size_t im2_exit_pending(void);
