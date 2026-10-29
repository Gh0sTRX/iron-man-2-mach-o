/* SPDX-License-Identifier: MIT. Independently authored, no Apple implementation copied. */
#include "objc_runtime.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#define CLASS_LIMIT 64
#define METHOD_LIMIT 64
#define SELECTOR_LIMIT 2048
#define RW_REALIZED (1u<<31)
typedef struct { const char *name,*types; Im2Imp imp; } Method;
typedef struct { uint32_t stride,count; Method methods[METHOD_LIMIT]; } MethodList;
typedef struct {
    Im2ObjcClass cls,meta;
    Im2ClassRO ro,meta_ro;
    Im2ClassRW rw,meta_rw;
    MethodList instance_methods,class_methods;
    const void *instance_lists[2],*class_lists[2];
    size_t instance_stret[METHOD_LIMIT],class_stret[METHOD_LIMIT];
} ClassEntry;
typedef struct Object {
    void *pointer; size_t refs; void (*destroy)(void *); struct Object *next;
} Object;
typedef struct Autorelease { void *object; struct Autorelease *next; } Autorelease;
typedef struct Pool { void *object; Autorelease *queued; struct Pool *parent; } Pool;
static ClassEntry classes[CLASS_LIMIT]; static size_t class_count;
static char *selectors[SELECTOR_LIMIT]; static size_t selector_count;
static Object *objects;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t pool_key; static pthread_once_t pool_once=PTHREAD_ONCE_INIT;
static int pool_key_ok;
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(Im2ObjcClass)==20,"ObjC2 ARM32 class layout");
_Static_assert(offsetof(Im2ClassRO,name)==16,"ObjC2 ARM32 class_ro name");
_Static_assert(sizeof(Method)==12,"ObjC2 ARM32 method entry");
#endif
static char *copy_string(const char *s) {
    size_t n=strlen(s)+1; char *p=malloc(n); if(p) memcpy(p,s,n); return p;
}
const char *im2_objc_sel_register(const char *s) {
    if(!s || !*s) return NULL;
    pthread_mutex_lock(&lock);
    for(size_t i=0;i<selector_count;i++) if(!strcmp(selectors[i],s)) {
        const char *r=selectors[i]; pthread_mutex_unlock(&lock); return r;
    }
    char *p=selector_count<SELECTOR_LIMIT?copy_string(s):NULL;
    if(p) selectors[selector_count++]=p;
    pthread_mutex_unlock(&lock); return p;
}
Im2ObjcClass *im2_objc_class_named(const char *s) {
    if(!s) return NULL;
    for(size_t i=0;i<class_count;i++) if(!strcmp(classes[i].ro.name,s)) return &classes[i].cls;
    return NULL;
}
int im2_objc_is_class(const void *p) {
    for(size_t i=0;i<class_count;i++) if(p==&classes[i].cls || p==&classes[i].meta) return 1;
    return 0;
}
static int fill_methods(MethodList *dst,size_t *sizes,const Im2MethodSpec *spec,size_t n) {
    dst->stride=sizeof(Method); dst->count=(uint32_t)n;
    for(size_t i=0;i<n;i++) {
        if(!spec[i].imp || !spec[i].selector || !spec[i].types) return -1;
        const char *sel=im2_objc_sel_register(spec[i].selector);
        if(!sel) return -1;
        for(size_t j=0;j<i;j++) if(dst->methods[j].name==sel) return -1;
        dst->methods[i]=(Method){sel,spec[i].types,spec[i].imp}; sizes[i]=spec[i].stret_size;
    }
    return 0;
}
Im2ObjcClass *im2_objc_register_class(const char *name,Im2ObjcClass *super,size_t size,
    const Im2MethodSpec *ims,size_t ni,const Im2MethodSpec *cms,size_t nc) {
    if(!name || !*name || size<sizeof(Im2ObjcObject) || size>UINT32_MAX ||
       class_count==CLASS_LIMIT || ni>METHOD_LIMIT || nc>METHOD_LIMIT ||
       (ni && !ims) || (nc && !cms) || im2_objc_class_named(name) ||
       (super && (!im2_objc_is_class(super) || (super->data->ro->flags&1u) || size<super->data->ro->instance_size))) return NULL;
    /* A second root would need an explicit independent metaclass hierarchy. */
    if(!super && class_count) return NULL;
    ClassEntry *e=&classes[class_count]; memset(e,0,sizeof(*e));
    char *owned_name=copy_string(name); if(!owned_name) return NULL;
    if(fill_methods(&e->instance_methods,e->instance_stret,ims,ni) ||
       fill_methods(&e->class_methods,e->class_stret,cms,nc)) { free(owned_name); return NULL; }
    e->ro.name=e->meta_ro.name=owned_name;
    e->ro.flags=super?0:2; e->meta_ro.flags=1|(super?0:2);
    e->ro.instance_start=super?super->data->ro->instance_size:sizeof(void *);
    e->ro.instance_size=(uint32_t)size;
    e->meta_ro.instance_start=e->meta_ro.instance_size=sizeof(Im2ObjcClass);
    e->ro.methods=&e->instance_methods; e->meta_ro.methods=&e->class_methods;
    e->rw.flags=e->meta_rw.flags=RW_REALIZED;
    e->rw.ro=&e->ro; e->meta_rw.ro=&e->meta_ro;
    e->instance_lists[0]=&e->instance_methods; e->class_lists[0]=&e->class_methods;
    e->rw.methods=e->instance_lists; e->meta_rw.methods=e->class_lists;
    e->cls=(Im2ObjcClass){&e->meta,super,NULL,NULL,&e->rw};
    Im2ObjcClass *rootmeta=super?classes[0].cls.isa:&e->meta;
    e->meta=(Im2ObjcClass){rootmeta,super?super->isa:&e->cls,NULL,NULL,&e->meta_rw};
    if(super) {e->rw.next_sibling=super->data->first_subclass;super->data->first_subclass=&e->cls;}
    class_count++; return &e->cls;
}
static Object *object_record(const void *p) {
    for(Object *o=objects;o;o=o->next) if(o->pointer==p) return o;
    return NULL;
}
Im2Imp im2_objc_lookup(const void *receiver,const char *selector,size_t *stret_size) {
    if(stret_size) *stret_size=0;
    if(!receiver || !selector) return NULL;
    Im2ObjcClass *cls=NULL;
    if(im2_objc_is_class(receiver)) cls=((const Im2ObjcClass *)receiver)->isa;
    else {
        pthread_mutex_lock(&lock); Object *o=object_record(receiver);
        if(o) cls=((const Im2ObjcObject *)receiver)->isa;
        pthread_mutex_unlock(&lock);
    }
    /* Only registered host classes, never blindly dereference an arbitrary guest pointer. */
    for(size_t depth=0;cls && depth<CLASS_LIMIT*2;depth++,cls=cls->superclass) {
        ClassEntry *entry=NULL; MethodList *methods=NULL; size_t *sizes=NULL;
        for(size_t i=0;i<class_count;i++) {
            if(cls==&classes[i].cls) {entry=&classes[i];methods=&entry->instance_methods;sizes=entry->instance_stret;break;}
            if(cls==&classes[i].meta) {entry=&classes[i];methods=&entry->class_methods;sizes=entry->class_stret;break;}
        }
        if(!entry) return NULL;
        for(size_t i=0;i<methods->count;i++) if(!strcmp(selector,methods->methods[i].name)) {
            if(stret_size) *stret_size=sizes[i];
            return methods->methods[i].imp;
        }
    }
    return NULL;
}
void *im2_objc_alloc(Im2ObjcClass *cls,void (*destroy)(void *)) {
    if(!im2_objc_is_class(cls) || cls->data->ro->flags&1u) return NULL;
    void *p=calloc(1,cls->data->ro->instance_size); Object *o=malloc(sizeof(*o));
    if(!p || !o) {free(p);free(o);return NULL;}
    ((Im2ObjcObject *)p)->isa=cls; *o=(Object){p,1,destroy,NULL};
    pthread_mutex_lock(&lock); o->next=objects; objects=o; pthread_mutex_unlock(&lock); return p;
}
void *im2_objc_retain(void *p) {
    if(!p || im2_objc_is_class(p)) return p;
    pthread_mutex_lock(&lock); Object *o=object_record(p);
    if(o && o->refs<SIZE_MAX) o->refs++; else p=NULL;
    pthread_mutex_unlock(&lock); return p;
}
size_t im2_objc_retain_count(const void *p) {
    if(im2_objc_is_class(p)) return SIZE_MAX;
    pthread_mutex_lock(&lock); Object *o=object_record(p);size_t n=o?o->refs:0;
    pthread_mutex_unlock(&lock);return n;
}
int im2_objc_release(void *p) {
    if(!p || im2_objc_is_class(p)) return 0;
    pthread_mutex_lock(&lock); Object **prev=&objects;
    while(*prev && (*prev)->pointer!=p) prev=&(*prev)->next;
    Object *o=*prev;
    if(!o) {pthread_mutex_unlock(&lock);return -1;}
    if(--o->refs) {pthread_mutex_unlock(&lock);return 0;}
    *prev=o->next; pthread_mutex_unlock(&lock);
    if(o->destroy) o->destroy(p);
    free(p);free(o);return 0;
}
static void drain_items(Pool *p) {
    while(p->queued) { Autorelease *a=p->queued;p->queued=a->next;im2_objc_release(a->object);free(a); }
}
static void thread_exit_pools(void *value) {
    Pool *p=value;
    while(p) {Pool *parent=p->parent;drain_items(p);im2_objc_release(p->object);free(p);p=parent;}
}
static void make_pool_key(void) {pool_key_ok=pthread_key_create(&pool_key,thread_exit_pools)==0;}
int im2_objc_pool_push(void *object) {
    pthread_once(&pool_once,make_pool_key);
    if(!pool_key_ok || !im2_objc_retain_count(object)) return -1;
    for(Pool *p=pthread_getspecific(pool_key);p;p=p->parent) if(p->object==object) return -1;
    Pool *p=calloc(1,sizeof(*p)); if(!p) return -1;
    p->object=object;p->parent=pthread_getspecific(pool_key);
    if(pthread_setspecific(pool_key,p)) {free(p);return -1;}
    return 0;
}
int im2_objc_pool_drain(void *object) {
    pthread_once(&pool_once,make_pool_key);
    Pool *p=pool_key_ok?pthread_getspecific(pool_key):NULL;
    if(!p || p->object!=object) return -1;
    if(pthread_setspecific(pool_key,p->parent)) return -1;
    drain_items(p);free(p);return im2_objc_release(object);
}
void *im2_objc_autorelease(void *object) {
    if(!object || im2_objc_is_class(object)) return object;
    pthread_once(&pool_once,make_pool_key);
    Pool *p=pool_key_ok?pthread_getspecific(pool_key):NULL;
    if(!p || !im2_objc_retain_count(object)) return NULL;
    Autorelease *a=malloc(sizeof(*a));if(!a) return NULL;
    *a=(Autorelease){object,p->queued};p->queued=a;return object;
}
