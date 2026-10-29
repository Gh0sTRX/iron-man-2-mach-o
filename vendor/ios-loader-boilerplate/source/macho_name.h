/* SPDX-License-Identifier: MIT. Mach-O fixed-width names need not contain NUL. */
#pragma once
#include <string.h>
static inline int im2_macho_name_equal(const char name[16],const char *expected) {
    return expected && strlen(expected)<=16 && strncmp(name,expected,16)==0;
}
