#pragma once
#include <stddef.h>
void kuKernelCpuUnrestrictedMemcpy(void *, const void *, size_t);
void kuKernelFlushCaches(void *, size_t);
