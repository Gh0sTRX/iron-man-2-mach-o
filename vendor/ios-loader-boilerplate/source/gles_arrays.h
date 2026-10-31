/* SPDX-License-Identifier: MIT. Bounded GLES1 client memory arrays; no VBOs. */
#pragma once
#include <stdint.h>
typedef struct Im2Arrays Im2Arrays;
enum { IM2_ARRAY_VERTEX,IM2_ARRAY_COLOR,IM2_ARRAY_NORMAL,IM2_ARRAY_TEX0,IM2_ARRAY_TEX1,IM2_ARRAY_SLOTS };
typedef struct {
 void *context;
 void (*pointer)(void *,unsigned,int,uint32_t,int,const void *);
 void (*enable)(void *,unsigned,int);
 void (*client_unit)(void *,uint32_t);
 void (*draw_arrays)(void *,uint32_t,int,int);
 void (*draw_elements)(void *,uint32_t,int,uint32_t,const void *);
} Im2ArrayBackend;
Im2Arrays *im2_arrays_create(Im2ArrayBackend);
void im2_arrays_destroy(Im2Arrays *);
uint32_t im2_arrays_set(Im2Arrays *,unsigned,int,uint32_t,int,const void *);
uint32_t im2_arrays_enable(Im2Arrays *,uint32_t,int);
uint32_t im2_arrays_client_unit(Im2Arrays *,uint32_t);
uint32_t im2_arrays_unit(const Im2Arrays *);
uint32_t im2_arrays_get_pointer(const Im2Arrays *,uint32_t,void **);
int im2_arrays_parameter(const Im2Arrays *,uint32_t,int *);
uint32_t im2_arrays_draw(Im2Arrays *,uint32_t,int,int);
uint32_t im2_arrays_elements(Im2Arrays *,uint32_t,int,uint32_t,const void *);
void im2_arrays_restore(Im2Arrays *);
