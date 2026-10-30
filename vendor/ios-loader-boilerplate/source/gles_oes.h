/* SPDX-License-Identifier: MIT. Bounded legacy OES framebuffer resource mapping. */
#pragma once
#include <stdint.h>
typedef struct Im2Oes Im2Oes;
typedef struct {
    void *ctx;
    uint32_t (*new_framebuffer)(void *);
    void (*delete_framebuffer)(void *,uint32_t);
    void (*bind_framebuffer)(void *,uint32_t);
    uint32_t (*new_storage)(void *,uint32_t format,int width,int height);
    void (*delete_storage)(void *,uint32_t format,uint32_t handle);
    int (*attach)(void *,uint32_t framebuffer,uint32_t attachment,uint32_t format,uint32_t storage);
    uint32_t (*check)(void *,uint32_t framebuffer);
} Im2OesBackend;
enum {
    IM2_OES_FB=0x8D40, IM2_OES_RB=0x8D41,
    IM2_OES_COLOR=0x8CE0, IM2_OES_DEPTH=0x8D00,
    IM2_OES_RGBA8=0x8058, IM2_OES_RGB565=0x8D62, IM2_OES_DEPTH16=0x81A5,
    IM2_OES_COMPLETE=0x8CD5, IM2_OES_INCOMPLETE=0x8CD6,
    IM2_OES_MISSING=0x8CD7, IM2_OES_DIMENSIONS=0x8CD9,
    IM2_OES_ENUM=0x500, IM2_OES_VALUE=0x501,
    IM2_OES_OPERATION=0x502, IM2_OES_MEMORY=0x505
};
Im2Oes *im2_oes_create(Im2OesBackend backend);
void im2_oes_destroy(Im2Oes *);
/* Zero = success; otherwise a GL error. Output names remain untouched on failure. */
uint32_t im2_oes_gen(Im2Oes *,uint32_t target,int count,uint32_t *names);
uint32_t im2_oes_delete(Im2Oes *,uint32_t target,int count,const uint32_t *names);
uint32_t im2_oes_bind(Im2Oes *,uint32_t target,uint32_t name);
uint32_t im2_oes_binding(Im2Oes *,uint32_t target);
uint32_t im2_oes_storage(Im2Oes *,uint32_t format,int width,int height);
uint32_t im2_oes_attach(Im2Oes *,uint32_t attachment,uint32_t renderbuffer);
uint32_t im2_oes_status(Im2Oes *);
uint32_t im2_oes_parameter(Im2Oes *,uint32_t pname,int *out);
/* Resolve the bound color renderbuffer to a complete actual GPU FBO and texture. */
int im2_oes_drawable(Im2Oes *,uint32_t *framebuffer,uint32_t *texture);
