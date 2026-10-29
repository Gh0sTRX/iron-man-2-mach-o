/* SPDX-License-Identifier: MIT
 * Partial NSFileManager: genuine read-only path existence via the same
 * configured path translation already used by NSData.
 */
#include "file_manager.h"
#include "foundation.h"
#include <stdint.h>

static Im2ObjcClass *file_manager_class;
static void *shared_manager;

static uintptr_t default_manager(void *cls,const char *selector) {
    (void)selector;
    return cls==file_manager_class?(uintptr_t)shared_manager:0;
}
static uint32_t exists(void *manager,const char *selector,void *path) {
    (void)manager;(void)selector;
    return im2_foundation_path_exists(path)?1u:0u;
}
#define METHOD(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_file_manager_init(Im2ObjcClass *root) {
    if(file_manager_class || !root || !im2_objc_class_named("NSString"))
        return -1;
    const Im2MethodSpec instance[]={
        METHOD("fileExistsAtPath:","c@:@",exists)
    };
    const Im2MethodSpec class_methods[]={
        METHOD("defaultManager","@@:",default_manager)
    };
    file_manager_class=im2_objc_register_class("NSFileManager",root,
        sizeof(Im2ObjcObject),instance,1,class_methods,1);
    if(!file_manager_class)return -1;
    shared_manager=im2_objc_alloc(file_manager_class,NULL);
    if(!shared_manager) {
        size_t checkpoint=im2_objc_registration_checkpoint();
        (void)im2_objc_registration_rollback(checkpoint-1);
        file_manager_class=NULL;
        return -1;
    }
    return 0;
}
