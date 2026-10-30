/* SPDX-License-Identifier: MIT.
 * Minimal EAGLContext/CAEAGLLayer state and explicit Vita graphics backend.
 * Class identity alone does not imply an OpenGL ES context is functional.
 */
#pragma once
#include <stdint.h>
#include "objc_runtime.h"
#include "imports.h"

enum { IM2_EAGL_ES1 = 1u, IM2_GL_RENDERBUFFER_OES = 0x8D41u };
typedef struct {
    /* ready checks backend initialization. Storage allocates real GPU resources;
     * present additionally requires a complete framebuffer for the bound drawable.
     * No callback may report success for an unperformed GPU operation. */
    int (*ready)(void);
    int (*storage)(uint32_t target,uint32_t color_format);
    int (*present)(uint32_t target);
} Im2EaglBackend;

int im2_eagl_init(Im2ObjcClass *root,void (*failure)(const char *));
int im2_eagl_set_backend(Im2EaglBackend backend);

/* All three original external NSString constant symbols are DATA addresses,
 * i.e. addresses of the NSString * globals, not NSString object pointers. */
extern const import_entry eagl_imports[];
extern const int eagl_imports_count;
