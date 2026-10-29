/* SPDX-License-Identifier: MIT. Actual NSFileManager IMPs, no fake stat results. */
#define _POSIX_C_SOURCE 200809L
#include "file_manager.h"
#include "foundation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char real_file[128];
static const char *map_path(const char *in,char *buffer,size_t capacity) {
    if(!strcmp(in,"/ios/Documents/settings.bin")) {
        if(snprintf(buffer,capacity,"%s",real_file)>=(int)capacity)return NULL;
        return buffer;
    }
    return in;
}
static Im2Imp method(void *receiver,const char *selector) {
    Im2ObjcTarget t;
    assert(!im2_objc_lookup_target(receiver,selector,NULL,&t));
    assert(!t.guest && !t.stret_size);
    return t.imp;
}
static void *send0(void *receiver,const char *selector) {
    return (void *)((uintptr_t(*)(void *,const char *))method(receiver,selector))(receiver,selector);
}
static uint32_t exists(void *receiver,void *path) {
    const char *s="fileExistsAtPath:";
    return ((uint32_t(*)(void *,const char *,void *))method(receiver,s))(receiver,s,path);
}
int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    Im2FoundationConfig config={"/tmp",NULL,NULL,map_path};
    assert(!im2_foundation_init(root,&config));
    assert(!im2_file_manager_init(root));
    assert(im2_file_manager_init(root));

    Im2ObjcClass *cls=im2_objc_class_named("NSFileManager");
    assert(cls);
    void *manager=send0(cls,"defaultManager");
    assert(manager && manager==send0(cls,"defaultManager"));
    assert(im2_objc_object_class(manager)==cls);

    snprintf(real_file,sizeof(real_file),"/tmp/im2-file-manager-XXXXXX");
    int fd=mkstemp(real_file);
    assert(fd>=0);
    assert(write(fd,"OK",2)==2);
    assert(!close(fd));

    void *alias=im2_string_create("/ios/Documents/settings.bin",0);
    void *actual=im2_string_create(real_file,0);
    void *missing=im2_string_create("/ios/Documents/not-there.bin",0);
    void *directory=im2_string_create("/tmp",0);
    const char embedded[]="/ios/Documents/settings.bin\0other";
    void *bad=im2_string_create_bytes(embedded,sizeof(embedded)-1,0);
    assert(alias && actual && missing && directory && bad);
    assert(exists(manager,alias)==1);
    assert(exists(manager,actual)==1);
    assert(exists(manager,directory)==1); /* Cocoa returns YES for directories too. */
    assert(exists(manager,missing)==0);
    assert(exists(manager,bad)==0);
    assert(exists(manager,NULL)==0);

    assert(!unlink(real_file));
    assert(!exists(manager,actual));
    assert(!exists(manager,alias));
    assert(!im2_objc_release(alias));
    assert(!im2_objc_release(actual));
    assert(!im2_objc_release(directory));
    assert(!im2_objc_release(missing));
    assert(!im2_objc_release(bad));
    puts("PASS: NSFileManager singleton, real filesystem stat, mapped iOS path, directories, absent paths and embedded-NUL rejection");
    return 0;
}
