/* SPDX-License-Identifier: MIT. Process-wide virtual cwd; host OS cwd is unchanged. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define IM2_FS_PATH_MAX 4096u
int im2_fs_init(const char *data_root);
const char *im2_fs_path(const char *path,char *out,size_t capacity);
int im2_fs_chdir(const char *path);
char *im2_fs_getcwd(char *out,size_t capacity);
/* Native ABI metadata for the owner diagnostic's real-file stat adapter. */
extern const uint32_t im2_fs_stat_layout[3];
