#pragma once
#include <stddef.h>
typedef int SceUID;
SceUID sceKernelAllocMemBlockForVM(const char *, size_t);
int sceKernelGetMemBlockBase(SceUID, void **);
int sceKernelFreeMemBlock(SceUID);
