/* SPDX-License-Identifier: MIT. Partial Foundation URL request support.
 * Stores URL, method, body and a bounded set of HTTP header fields.
 * Does not execute network requests or implement CFNetwork.
 */
#pragma once
#include "objc_runtime.h"
int im2_url_request_init(Im2ObjcClass *root, void (*failure)(const char *));
