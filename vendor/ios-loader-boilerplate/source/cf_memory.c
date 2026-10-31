/* SPDX-License-Identifier: MIT. Shared CF lifetime boundary over managed objects. */
#include "objc_runtime.h"
#include "imports.h"
#include "log.h"
static void cf_release(void *value) {
    if(!value || im2_objc_release(value))fatal_error("CFRelease on unsupported object");
}
static void *cf_retain(void *value) {
    if(!value || !im2_objc_retain(value))fatal_error("CFRetain on unsupported object");
    return value;
}
const import_entry cf_memory_imports[]={F(CFRelease,cf_release),F(CFRetain,cf_retain)};
const int cf_memory_imports_count=sizeof(cf_memory_imports)/sizeof(cf_memory_imports[0]);
