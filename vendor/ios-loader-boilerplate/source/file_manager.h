/* SPDX-License-Identifier: MIT. Read-only subset of NSFileManager.
 * No filesystem mutations or fabricated iOS file contents.
 */
#pragma once
#include "objc_runtime.h"
int im2_file_manager_init(Im2ObjcClass *root);
