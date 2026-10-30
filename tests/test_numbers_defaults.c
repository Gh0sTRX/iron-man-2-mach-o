/* SPDX-License-Identifier: MIT. Invoke production NSNumber/defaults methods with actual files. */
#define _POSIX_C_SOURCE 200809L
#include "numbers.h"
#include "defaults.h"
#include "foundation.h"
#include "collections.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static Im2Imp method(void *o,const char *sel){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,sel,NULL,&t));assert(t.types && !t.guest && !t.stret_size);return t.imp;}
static uintptr_t send(void *o,const char *sel){return ((uintptr_t(*)(void *,const char *))method(o,sel))(o,sel);}
static uintptr_t send1(void *o,const char *sel,void *arg){return ((uintptr_t(*)(void *,const char *,void *))method(o,sel))(o,sel,arg);}
static uint32_t equal(void *a,const char *sel,void *b){return ((uint32_t(*)(void *,const char *,void *))method(a,sel))(a,sel,b);}
static uint32_t word(void *o,const char *sel){return ((uint32_t(*)(void *,const char *))method(o,sel))(o,sel);}
static uint64_t wide(void *o,const char *sel){return ((uint64_t(*)(void *,const char *))method(o,sel))(o,sel);}
static void missing(void *o,const char *sel){Im2ObjcTarget t;assert(im2_objc_lookup_target(o,sel,NULL,&t));}
static void *factory32(const char *sel,uint32_t value){void *cls=im2_objc_class_named("NSNumber");if(!strcmp(sel,"numberWithInt:") || !strcmp(sel,"numberWithInteger:"))return (void *)((uintptr_t(*)(void *,const char *,int32_t))method(cls,sel))(cls,sel,(int32_t)value);return (void *)((uintptr_t(*)(void *,const char *,uint32_t))method(cls,sel))(cls,sel,value);}
static void *factory64(const char *sel,uint64_t value){void *cls=im2_objc_class_named("NSNumber");return (void *)((uintptr_t(*)(void *,const char *,uint32_t,uint32_t))method(cls,sel))(cls,sel,(uint32_t)value,(uint32_t)(value>>32));}
int main(void){
 Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root);
 Im2FoundationConfig cfg={"/tmp",NULL,NULL,NULL};assert(!im2_foundation_init(root,&cfg));assert(!im2_collections_init(root,NULL));
 assert(!im2_numbers_init(root) && im2_numbers_init(root));assert(!im2_number_bool(1,1)); /* Missing pool never leaks a factory. */
 void *owned=im2_number_bool(0,0);int boolean=-1;assert(owned && !im2_number_bool_value(owned,&boolean) && !boolean);
 assert(im2_number_bool_value(NULL,&boolean));
 void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
 void *no=factory32("numberWithBool:",0xffffff00u),*yes=factory32("numberWithBool:",0xffu);assert(no && yes && !word(no,"boolValue") && word(yes,"boolValue")==1);
 void *minus=factory32("numberWithInt:",(uint32_t)-123);assert(minus && word(minus,"intValue")== (uint32_t)-123 && wide(minus,"longLongValue")== (uint64_t)(int64_t)-123);
 assert(!strcmp((const char *)send(minus,"objCType"),"i"));
 void *large=factory64("numberWithUnsignedLongLong:",UINT64_MAX),*negative=factory64("numberWithLongLong:",UINT64_MAX);assert(large && negative);
 assert(wide(large,"unsignedLongLongValue")==UINT64_MAX && word(large,"unsignedIntValue")==UINT32_MAX);
 assert(!equal(large,"isEqual:",negative));
 void *one=factory32("numberWithInt:",1);assert(equal(one,"isEqualToNumber:",yes));
 void *onesigned=im2_number_signed(1,1),*oneunsigned=im2_number_unsigned(1,1);assert(onesigned && oneunsigned && equal(onesigned,"isEqual:",oneunsigned));
 uint32_t bits32=word(minus,"floatValue");float value32;memcpy(&value32,&bits32,4);assert(value32==-123.f);
 uint64_t bits64=wide(large,"doubleValue");double value64;memcpy(&value64,&bits64,8);assert(value64==(double)UINT64_MAX);
 assert(!strcmp(im2_string_utf8((void *)send(large,"stringValue")),"18446744073709551615"));
 assert(!strcmp(im2_string_utf8((void *)send(negative,"description")),"-1"));
 void *copy=(void *)send(minus,"copy");assert(copy==minus && im2_objc_retain_count(minus)==2 && !im2_objc_release(copy));
 void *text=im2_string_create("0",1);assert(text && im2_number_bool_value(text,&boolean) && !equal(minus,"isEqual:",text));
 missing(im2_objc_class_named("NSNumber"),"numberWithDouble:");
 char dir_template[]="/tmp/im2-defaults-XXXXXX";char *dir=mkdtemp(dir_template);assert(dir);char path[512];snprintf(path,sizeof(path),"%s/preferences.plist",dir);
 FILE *f=fopen(path,"wb");assert(f && fwrite("private preference input",1,24,f)==24 && !fclose(f));
 assert(im2_defaults_init(root,"tr",path)); /* Existing preference bytes must not be silently discarded. */
 assert(!im2_objc_class_named("NSUserDefaults"));
 assert(!unlink(path));assert(im2_defaults_init(root,"en--GB",path));assert(im2_defaults_init(root,"../en",path));
 assert(!im2_defaults_init(root,"tr",path));assert(im2_defaults_init(root,"en",path));
 assert(!strcmp(im2_vita_language_code(19),"tr") && !strcmp(im2_vita_language_code(18),"en-GB") && !strcmp(im2_vita_language_code(10),"zh-Hant"));
 assert(!im2_vita_language_code(-1) && !im2_vita_language_code(20));
 void *cls=im2_objc_class_named("NSUserDefaults"),*defaults=(void *)send(cls,"standardUserDefaults");assert(defaults && defaults==(void *)send(cls,"standardUserDefaults"));
 void *key=im2_string_create("AppleLanguages",1),*unknown=im2_string_create("Unknown",1);assert(key && unknown);
 void *languages=(void *)send1(defaults,"objectForKey:",key);assert(languages && send(languages,"count")==1);
 void *language=(void *)((uintptr_t(*)(void *,const char *,size_t))method(languages,"objectAtIndex:"))(languages,"objectAtIndex:",0);
 assert(!strcmp(im2_string_utf8(language),"tr"));
 assert(send1(defaults,"arrayForKey:",key)== (uintptr_t)languages && !send1(defaults,"stringForKey:",key));
 assert(!send1(defaults,"objectForKey:",unknown));
 void *nulkey=im2_string_create_bytes("AppleLanguages\0x",16,1);assert(nulkey && !send1(defaults,"objectForKey:",nulkey));
 void *snapshot=(void *)send(defaults,"dictionaryRepresentation");assert(snapshot && send(snapshot,"count")==1);assert(send1(snapshot,"objectForKey:",key)== (uintptr_t)languages);
 missing(defaults,"setObject:forKey:");missing(defaults,"synchronize");missing(languages,"addObject:");
 assert(im2_objc_retain(defaults)==defaults && im2_defaults_shutdown());assert(!im2_objc_release(defaults));
 assert(!im2_defaults_shutdown());assert(!send(cls,"standardUserDefaults"));
 assert(!strcmp(im2_string_utf8(language),"tr")); /* Snapshot still retains the array. */
 assert(!im2_objc_pool_drain(pool));assert(!im2_objc_retain_count(language));assert(!im2_objc_release(owned));
 assert(!access(dir,F_OK) && !rmdir(dir));
 puts("PASS: integral NSNumber signed/unsigned/BOOL semantics, SoftFP getter bits, Cocoa lifetimes, actual fresh language defaults and existing-preference rejection");
}
