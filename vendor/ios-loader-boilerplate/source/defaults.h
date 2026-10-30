/* SPDX-License-Identifier: MIT. Read-only fresh-install NSUserDefaults language domain. */
#pragma once
#include "objc_runtime.h"
const char *im2_vita_language_code(int system_language);
/* A preexisting preferences file is rejected until a real plist reader exists.
 * NULL preferences_path is allowed only for callers supplying no persistent domain. */
int im2_defaults_init(Im2ObjcClass *root,const char *language,const char *preferences_path);
int im2_defaults_shutdown(void);
