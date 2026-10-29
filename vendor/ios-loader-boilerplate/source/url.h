/* SPDX-License-Identifier: MIT
 * Partial NSURL support for the original iOS Mach-O loader.
 * URL parsing is deliberately strict; unsupported inputs do not silently work.
 */
#pragma once
#include "objc_runtime.h"

/* Registers a native NSURL class with immutable, owned URL state.
 * Requires an initialized NSString class. Returns 0 on success. */
int im2_url_init(Im2ObjcClass *root, void (*failure)(const char *));

/* Borrowed UTF-8 path only for registered, absolute local file:// URLs.
 * NULL for http(s), unknown objects or unsupported URL representations. */
const char *im2_url_local_file_path(const void *url);
