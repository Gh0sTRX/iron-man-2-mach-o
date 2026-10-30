/* SPDX-License-Identifier: MIT. Independently implemented read-only language defaults.
 * No saving/synchronize or fake success for persisted preference loading. */
#include "defaults.h"
#include "collections.h"
#include "foundation.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
typedef struct {Im2ObjcClass *isa;void *values;} Defaults;
static Im2ObjcClass *defaults_class;
static Defaults *shared;
const char *im2_vita_language_code(int language){
 /* VitaSDK SceSystemParamLang enumeration, 0..19. Native code asserts key IDs. */
 static const char *codes[]={"ja","en","fr","es","de","it","nl","pt-PT","ru","ko","zh-Hant","zh-Hans","fi","sv","da","nb","pl","pt-BR","en-GB","tr"};
 return language>=0 && language<(int)(sizeof(codes)/sizeof(*codes))?codes[language]:NULL;
}
static int language_valid(const char *s){
 if(!s)return 0;
 size_t n=strlen(s);if(n<2 || n>16)return 0;
 int group=0;for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)s[i];if(c=='-'){if(!group)return 0;group=0;}else if((c>='a' && c<='z') || (c>='A' && c<='Z'))group++;else return 0;}return group!=0;
}
static void cleanup(void *o){Defaults *d=o;if(d->values)im2_objc_release(d->values);d->values=NULL;}
static uintptr_t standard(void *cls,const char *sel){(void)sel;if(cls!=defaults_class)abort();return (uintptr_t)shared;}
static uintptr_t initialize(Defaults *d,const char *sel){(void)sel;if(!shared || !shared->values)abort();void *values=im2_objc_retain(shared->values);if(d->values)im2_objc_release(d->values);d->values=values;return (uintptr_t)d;}
static uintptr_t get(Defaults *d,const char *sel,void *key){
 (void)sel;Im2ObjcTarget method;
 if(!d->values || im2_objc_lookup_target(d->values,"objectForKey:",NULL,&method) || method.guest || method.stret_size)abort();
 return ((uintptr_t(*)(void *,const char *,void *))method.imp)(d->values,"objectForKey:",key);
}
static uintptr_t array_get(Defaults *d,const char *sel,void *key){void *o=(void *)get(d,sel,key);return im2_objc_object_class(o)==im2_objc_class_named("NSArray")?(uintptr_t)o:0;}
static uintptr_t string_get(Defaults *d,const char *sel,void *key){void *o=(void *)get(d,sel,key);return im2_string_utf8(o)?(uintptr_t)o:0;}
static uintptr_t representation(Defaults *d,const char *sel){(void)sel;void *o=im2_objc_retain(d->values);if(o && !im2_objc_autorelease(o)){im2_objc_release(o);return 0;}return (uintptr_t)o;}
static void dealloc(void *o,const char *sel){(void)sel;cleanup(o);}
#define M(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_defaults_init(Im2ObjcClass *root,const char *language,const char *preferences_path){
 const Im2ObjcClass *r=im2_objc_class_metadata(root);
 if(defaults_class || !r || r->data->ro->flags&1 || !language_valid(language) || !im2_objc_class_named("NSArray") || !im2_objc_class_named("NSDictionary"))return -1;
 if(preferences_path){struct stat st;if(!*preferences_path || strlen(preferences_path)>4095)return -1;if(!stat(preferences_path,&st) || errno!=ENOENT)return -1;}
 size_t checkpoint=im2_objc_registration_checkpoint();void *key=im2_string_create("AppleLanguages",0),*lang=im2_string_create(language,0),*array=NULL,*values=NULL;
 if(key && lang)array=im2_array_create(&lang,1,0,0);
 if(array)values=im2_dictionary_create(&array,&key,1,0,0);
 if(key)im2_objc_release(key);
 if(lang)im2_objc_release(lang);
 if(array)im2_objc_release(array);
 if(!values)return -1;
 const Im2MethodSpec im[]={M("init","@@:",initialize),M("objectForKey:","@@:@",get),M("arrayForKey:","@@:@",array_get),M("stringForKey:","@@:@",string_get),M("dictionaryRepresentation","@@:",representation),M("dealloc","v@:",dealloc)};
 const Im2MethodSpec cm[]={M("standardUserDefaults","@@:",standard)};
 defaults_class=im2_objc_register_class("NSUserDefaults",root,sizeof(Defaults),im,sizeof(im)/sizeof(*im),cm,1);
 if(defaults_class)shared=im2_objc_alloc(defaults_class,cleanup);
 if(!shared){im2_objc_release(values);(void)im2_objc_registration_rollback(checkpoint);defaults_class=NULL;return -1;}
 shared->values=values;return 0;
}
int im2_defaults_shutdown(void){if(!shared || im2_objc_retain_count(shared)!=1)return -1;Defaults *o=shared;shared=NULL;return im2_objc_release(o);}
