/* SPDX-License-Identifier: MIT. Darwin thread lifecycle values, not host enums. */
#pragma once
#include <pthread.h>
#include <stdint.h>
int im2_thread_error(int native_error);
int im2_thread_join(pthread_t thread,uint32_t *guest_result);
int im2_thread_cancel(pthread_t thread);
int im2_thread_setcanceltype(int darwin_type,int32_t *old_type);
void im2_thread_testcancel(void);
