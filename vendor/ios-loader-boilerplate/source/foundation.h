/* SPDX-License-Identifier: MIT. Explicit, partial Foundation behavior for the loader. */
#pragma once
#include "objc_runtime.h"
#define IM2_NS_ASCII 1u
#define IM2_NS_UTF8 4u
#define IM2_FOUNDATION_MAX_DATA (64u*1024u*1024u)
typedef struct { Im2ObjcClass *isa;uint32_t flags;char *str;uint32_t bytes; } Im2String;
typedef struct {
    const char *bundle_path;
    Im2Imp string_format; /* optional Darwin variadic implementation; not synthesized */
    void (*fail)(const char *);
    const char *(*map_path)(const char *,char *,size_t);
} Im2FoundationConfig;
int im2_foundation_init(Im2ObjcClass *root,const Im2FoundationConfig *);
Im2String *im2_string_create(const char *,int autorelease);
const char *im2_string_utf8(const void *);
int im2_utf8_utf16_length(const char *,size_t bytes,uint32_t *length);
int im2_string_get_cstring(const void *,char *out,size_t capacity,uint32_t encoding);
void *im2_data_create(const void *bytes,size_t count,int autorelease);
void *im2_data_from_file(const char *path,int autorelease);
const void *im2_data_bytes(const void *);
size_t im2_data_length(const void *);
int im2_data_get_bytes(const void *,void *out,size_t length);
/* Returned path is malloc-owned; missing resources return NULL. */
char *im2_bundle_resource_path(const char *name,const char *extension,const char *directory);
/* Imported ARM32 CFString constants are borrowed and immutable. */
#include "objc_guest.h"
int im2_foundation_register_cfstrings(uint32_t address,size_t count,Im2GuestRead read,
    void *context,char *error,size_t error_size);
