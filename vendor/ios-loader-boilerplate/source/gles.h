/**
 * @file gles.h
 * @brief OpenGL ES 1.1 / 2.0 graphics pipeline over vitaGL with SoftFP calling convention.
 */
#pragma once
#include <stdint.h>

/**
 * @brief Initializes the OpenGL ES pipeline and off-screen virtual FBO.
 */
void gles_init(void);

/**
 * @brief Presents current frame: blits and scales iOS FBO to PS Vita 960x544 screen.
 */
void gles_present(void);

/**
 * @brief Returns the total number of draw calls dispatched this session.
 */
uint32_t gles_get_draw_calls(void);

/* Actual vitaGL FBO-backed iOS EAGLContext bridge.
 * A 0 return must propagate to the original Cocoa caller as BOOL NO. */
int gles_eagl_ready(void);
int gles_eagl_storage(uint32_t renderbuffer_target,uint32_t color_format);
int gles_eagl_present(uint32_t renderbuffer_target);

/* Shared first-error slot for GLES compatibility bridges. */
void gles_record_error(uint32_t error);

/* CPU client-array registry, also used for asset-free ARM integration tests. */
int gles_client_arrays_init(void);
void gles_client_arrays_restore(void);
