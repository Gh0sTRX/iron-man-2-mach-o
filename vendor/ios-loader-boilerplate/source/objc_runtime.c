/* SPDX-License-Identifier: MIT. Independently authored, no Apple implementation copied. */
#include "objc_runtime.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#define CLASS_LIMIT 128
#define METHOD_LIMIT 64
#define SELECTOR_LIMIT 2048
#define RW_REALIZED (1u<<31)
typedef struct { const char *name,*types; Im2Imp imp; } Method;
typedef struct { uint32_t stride,count; Method methods[METHOD_LIMIT]; } MethodList;
typedef struct {
    Im2ObjcClass cls,meta;
    const void *identity,*meta_identity;
    int guest;
    Im2ClassRO ro,meta_ro;
    Im2ClassRW rw,meta_rw;
    MethodList instance_methods,class_methods;
    const void *instance_lists[2],*class_lists[2];
    size_t instance_stret[METHOD_LIMIT],class_stret[METHOD_LIMIT];
} ClassEntry;
typedef struct Object {
    void *pointer; size_t refs; void (*destroy)(void *); struct Object *next;
    Im2ObjcClass *isa;int immutable;
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
const Im2ObjcClass *im2_objc_class_metadata(const void *identity) {
    for(size_t i=0;i<class_count;i++) {
        if(identity==classes[i].identity) return &classes[i].cls;
        if(identity==classes[i].meta_identity) return &classes[i].meta;
    }
    return NULL;
}
Im2ObjcClass *im2_objc_class_named(const char *s) {
    if(!s) return NULL;
    for(size_t i=0;i<class_count;i++) if(!strcmp(classes[i].ro.name,s)) return (Im2ObjcClass *)classes[i].identity;
    return NULL;
}
int im2_objc_is_class(const void *p) {return im2_objc_class_metadata(p)!=NULL;}
static void free_entry(ClassEntry *e) {
    for(size_t j=0;j<e->instance_methods.count;j++) free((void *)e->instance_methods.methods[j].types);
    for(size_t j=0;j<e->class_methods.count;j++) free((void *)e->class_methods.methods[j].types);
    free((void *)e->ro.name);memset(e,0,sizeof(*e));
}
static int fill_methods(MethodList *dst,size_t *sizes,const Im2MethodSpec *spec,size_t n) {
    dst->stride=sizeof(Method); dst->count=(uint32_t)n;
    for(size_t i=0;i<n;i++) {
        if(!spec[i].imp || !spec[i].selector || !spec[i].types) return -1;
        const char *sel=im2_objc_sel_register(spec[i].selector);
        if(!sel) return -1;
        for(size_t j=0;j<i;j++) if(dst->methods[j].name==sel) return -1;
        char *types=copy_string(spec[i].types);if(!types) return -1;
        dst->methods[i]=(Method){sel,types,spec[i].imp}; sizes[i]=spec[i].stret_size;
    }
    return 0;
}
Im2ObjcClass *im2_objc_register_class(const char *name,Im2ObjcClass *super,size_t size,
    const Im2MethodSpec *ims,size_t ni,const Im2MethodSpec *cms,size_t nc) {
    if(!name || !*name || size<sizeof(Im2ObjcObject) || size>UINT32_MAX ||
       class_count==CLASS_LIMIT || ni>METHOD_LIMIT || nc>METHOD_LIMIT ||
       (ni && !ims) || (nc && !cms) || im2_objc_class_named(name) ||
       (super && (!im2_objc_is_class(super) || (im2_objc_class_metadata(super)->data->ro->flags&1u) || size<im2_objc_class_metadata(super)->data->ro->instance_size))) return NULL;
    /* A second root would need an explicit independent metaclass hierarchy. */
    if(!super && class_count) return NULL;
    ClassEntry *e=&classes[class_count]; memset(e,0,sizeof(*e));
    char *owned_name=copy_string(name); if(!owned_name) return NULL;
    e->ro.name=owned_name;
    if(fill_methods(&e->instance_methods,e->instance_stret,ims,ni) ||
       fill_methods(&e->class_methods,e->class_stret,cms,nc)) { free_entry(e); return NULL; }
    e->ro.name=e->meta_ro.name=owned_name;
    e->ro.flags=super?0:2; e->meta_ro.flags=1|(super?0:2);
    e->ro.instance_start=super?im2_objc_class_metadata(super)->data->ro->instance_size:sizeof(void *);
    e->ro.instance_size=(uint32_t)size;
    e->meta_ro.instance_start=e->meta_ro.instance_size=sizeof(Im2ObjcClass);
    e->ro.methods=&e->instance_methods; e->meta_ro.methods=&e->class_methods;
    e->rw.flags=e->meta_rw.flags=RW_REALIZED;
    e->rw.ro=&e->ro; e->meta_rw.ro=&e->meta_ro;
    e->instance_lists[0]=&e->instance_methods; e->class_lists[0]=&e->class_methods;
    e->rw.methods=e->instance_lists; e->meta_rw.methods=e->class_lists;
    e->cls=(Im2ObjcClass){&e->meta,super,NULL,NULL,&e->rw};
    Im2ObjcClass *rootmeta=super?classes[0].cls.isa:&e->meta;
    e->meta=(Im2ObjcClass){rootmeta,super?im2_objc_class_metadata(super)->isa:&e->cls,NULL,NULL,&e->meta_rw};
    e->identity=&e->cls;e->meta_identity=&e->meta;
    if(super) {Im2ClassRW *rw=im2_objc_class_metadata(super)->data;e->rw.next_sibling=rw->first_subclass;rw->first_subclass=&e->cls;}
    class_count++; return &e->cls;
}
static Object *object_record(const void *p) {
    for(Object *o=objects;o;o=o->next) if(o->pointer==p) return o;
    return NULL;
}
int im2_objc_lookup_target(const void *receiver,const char *selector,
    const void *current_class,Im2ObjcTarget *target) {
    if(target) memset(target,0,sizeof(*target));
    if(!receiver || !selector || !target) return -1;
    const Im2ObjcClass *metadata=im2_objc_class_metadata(receiver);
    Im2ObjcClass *cls=metadata?metadata->isa:NULL;
    if(!metadata) {
        pthread_mutex_lock(&lock);Object *o=object_record(receiver);
        if(o) cls=o->isa;
        pthread_mutex_unlock(&lock);
    }
    if(current_class) {
        /* objc_msgSendSuper2 starts at the parent of the current implementation class,
         * not the parent of the receiver's dynamic class. Reject unrelated classes. */
        Im2ObjcClass *cursor=cls;int found=0;
        for(size_t depth=0;cursor && depth<CLASS_LIMIT*2;depth++) {
            const Im2ObjcClass *info=im2_objc_class_metadata(cursor);if(!info) return -1;
            if(cursor==current_class) {cls=info->superclass;found=1;break;}
            cursor=info->superclass;
        }
        if(!found) return -1;
    }
    for(size_t depth=0;cls && depth<CLASS_LIMIT*2;depth++) {
        ClassEntry *entry=NULL;MethodList *methods=NULL;size_t *sizes=NULL;
        for(size_t i=0;i<class_count;i++) {
            if(cls==classes[i].identity) {entry=&classes[i];methods=&entry->instance_methods;sizes=entry->instance_stret;break;}
            if(cls==classes[i].meta_identity) {entry=&classes[i];methods=&entry->class_methods;sizes=entry->class_stret;break;}
        }
        if(!entry) return -1;
        for(size_t i=0;i<methods->count;i++) if(!strcmp(selector,methods->methods[i].name)) {
            *target=(Im2ObjcTarget){methods->methods[i].imp,sizes[i],entry->guest};return 0;
        }
        cls=im2_objc_class_metadata(cls)->superclass;
    }
    return -1;
}
Im2Imp im2_objc_lookup(const void *receiver,const char *selector,size_t *size) {
    Im2ObjcTarget target;int result=im2_objc_lookup_target(receiver,selector,NULL,&target);
    if(size) *size=result?0:target.stret_size;
    return result?NULL:target.imp;
}
static void rebuild_subclasses(void) {
    for(size_t i=0;i<class_count;i++) {classes[i].rw.first_subclass=NULL;classes[i].rw.next_sibling=NULL;}
    for(size_t i=0;i<class_count;i++) if(classes[i].cls.superclass) {
        Im2ClassRW *parent=im2_objc_class_metadata(classes[i].cls.superclass)->data;
        classes[i].rw.next_sibling=parent->first_subclass;parent->first_subclass=(Im2ObjcClass *)classes[i].identity;
    }
}
Im2ObjcClass *im2_objc_register_foreign_class(const char *name,void *identity,void *meta,
    Im2ObjcClass *super,size_t size,const Im2MethodSpec *im,size_t ni,const Im2MethodSpec *cm,size_t nc) {
    if(!identity || !meta || identity==meta || im2_objc_is_class(identity) || im2_objc_is_class(meta) || !super) return NULL;
    Im2ObjcClass *local=im2_objc_register_class(name,super,size,im,ni,cm,nc);if(!local) return NULL;
    ClassEntry *e=&classes[class_count-1];e->identity=identity;e->meta_identity=meta;e->guest=1;
    e->cls.isa=(Im2ObjcClass *)meta;rebuild_subclasses();return (Im2ObjcClass *)identity;
}
size_t im2_objc_registration_checkpoint(void) {return class_count;}
int im2_objc_registration_rollback(size_t checkpoint) {
    if(checkpoint>class_count) return -1;
    pthread_mutex_lock(&lock);
    for(Object *o=objects;o;o=o->next) for(size_t i=checkpoint;i<class_count;i++)
        if(o->isa==classes[i].identity) {pthread_mutex_unlock(&lock);return -1;}
    while(class_count>checkpoint) free_entry(&classes[--class_count]);
    rebuild_subclasses();pthread_mutex_unlock(&lock);return 0;
}
void *im2_objc_alloc(Im2ObjcClass *cls,void (*destroy)(void *)) {
    const Im2ObjcClass *info=im2_objc_class_metadata(cls);
    if(!info || info->data->ro->flags&1u) return NULL;
    void *p=calloc(1,info->data->ro->instance_size); Object *o=malloc(sizeof(*o));
    if(!p || !o) {free(p);free(o);return NULL;}
    ((Im2ObjcObject *)p)->isa=cls; *o=(Object){p,1,destroy,NULL,cls,0};
    pthread_mutex_lock(&lock); o->next=objects; objects=o; pthread_mutex_unlock(&lock); return p;
}
void *im2_objc_retain(void *p) {
    if(!p || im2_objc_is_class(p)) return p;
    pthread_mutex_lock(&lock); Object *o=object_record(p);
    if(o && o->immutable) {}
    else if(o && o->refs && o->refs<SIZE_MAX) o->refs++; else p=NULL;
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
    if(o && o->immutable) {pthread_mutex_unlock(&lock);return 0;}
    if(!o || !o->refs) {pthread_mutex_unlock(&lock);return -1;}
    if(--o->refs) {pthread_mutex_unlock(&lock);return 0;}
    /* Keep the dying object discoverable during its -dealloc chain. Zero refs
     * rejects resurrection/double release while guest superclass lookup still works. */
    pthread_mutex_unlock(&lock);
    if(o->destroy) o->destroy(p);
    pthread_mutex_lock(&lock);prev=&objects;
    while(*prev && *prev!=o) prev=&(*prev)->next;
    if(*prev==o) *prev=o->next;
    pthread_mutex_unlock(&lock);free(p);free(o);return 0;
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
    pthread_mutex_lock(&lock);Object *known=object_record(object);int immutable=known && known->immutable;
    pthread_mutex_unlock(&lock);if(immutable)return object;
    pthread_once(&pool_once,make_pool_key);
    Pool *p=pool_key_ok?pthread_getspecific(pool_key):NULL;
    if(!p || !im2_objc_retain_count(object)) return NULL;
    Autorelease *a=malloc(sizeof(*a));if(!a) return NULL;
    *a=(Autorelease){object,p->queued};p->queued=a;return object;
}

Im2ObjcClass *im2_objc_object_class(const void *receiver) {
    const Im2ObjcClass *cls=im2_objc_class_metadata(receiver);if(cls)return cls->isa;
    pthread_mutex_lock(&lock);Object *o=object_record(receiver);
    Im2ObjcClass *isa=o?o->isa:NULL;
    pthread_mutex_unlock(&lock);return isa;
}

int im2_objc_register_static_objects(void *const *ids,size_t count,Im2ObjcClass *cls) {
    const Im2ObjcClass *meta=im2_objc_class_metadata(cls);
    if(!meta || meta->data->ro->flags&1 || count>4096 || (count && !ids))return -1;
    Object *pending=NULL;int result=-1;
    pthread_mutex_lock(&lock);
    for(size_t i=0;i<count;i++) {
        if(!ids[i] || im2_objc_is_class(ids[i]) || object_record(ids[i]))goto done;
        for(Object *o=pending;o;o=o->next)if(o->pointer==ids[i])goto done;
        Object *o=malloc(sizeof(*o));if(!o)goto done;
        *o=(Object){ids[i],SIZE_MAX,NULL,pending,cls,1};pending=o;
    }
    while(pending){Object *o=pending;pending=o->next;o->next=objects;objects=o;}result=0;
done:
    pthread_mutex_unlock(&lock);
    while(pending){Object *o=pending;pending=o->next;free(o);}return result;
}
int im2_objc_unregister_static_objects(void *const *ids,size_t count) {
    if(count>4096 || (count && !ids))return -1;
    pthread_mutex_lock(&lock);
    for(size_t i=0;i<count;i++) {
        Object *o=object_record(ids[i]);
        if(!o || !o->immutable){pthread_mutex_unlock(&lock);return -1;}
        for(size_t j=0;j<i;j++)if(ids[j]==ids[i]){pthread_mutex_unlock(&lock);return -1;}
    }
    for(size_t i=0;i<count;i++) {
        Object **at=&objects;while((*at)->pointer!=ids[i])at=&(*at)->next;
        Object *o=*at;*at=o->next;free(o);
    }
    pthread_mutex_unlock(&lock);return 0;
}
