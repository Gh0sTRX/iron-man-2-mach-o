/* SPDX-License-Identifier: MIT. Independently implemented retained Foundation collections.
 * Startup class registration only. Collection mutation requires external serialization.
 * Dictionary keys are supported NSStrings; arbitrary NSObject hashing is not implemented.
 */
#include "collections.h"
#include "foundation.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define LIMIT 65536u
/* ml_align_call copies sixteen stack words; leave space for the nil sentinel. */
#define VARIADIC_ITEMS 17u
#define VARIADIC_PAIRS 8u
typedef struct {Im2ObjcClass *isa;void **items;size_t count;uintptr_t mutation;} Array;
typedef struct {void *key,*value;} Pair;
typedef struct {Im2ObjcClass *isa;Pair *pairs;size_t count;uintptr_t mutation;} Dictionary;
static Im2ObjcClass *arrays,*mutable_arrays,*dictionaries,*mutable_dictionaries;
static void (*failure)(const char *);
static void fail(const char *why){if(failure)failure(why);abort();}
static int kind(const void *o,Im2ObjcClass *wanted){
    for(Im2ObjcClass *c=im2_objc_object_class(o);c;){
        if(c==wanted)return 1;
        const Im2ObjcClass *m=im2_objc_class_metadata(c);if(!m)return 0;c=m->superclass;
    }return 0;
}
static int factory_mutable(void *cls,Im2ObjcClass *base,Im2ObjcClass *mutable){
    if(cls!=base && cls!=mutable)fail("Unsupported collection factory subclass");
    return cls==mutable;
}
static void array_clear(Array *a){for(size_t i=0;i<a->count;i++)im2_objc_release(a->items[i]);free(a->items);a->items=NULL;a->count=0;a->mutation++;}
static void array_destroy(void *o){array_clear(o);}
static int array_assign(Array *a,void *const *items,size_t count){
    if(count>LIMIT || (count && !items))return -1;
    void **copy=count?calloc(count,sizeof(*copy)):NULL;if(count && !copy)return -1;
    size_t i=0;for(;i<count;i++){if(!items[i] || !(copy[i]=im2_objc_retain(items[i])))break;}
    if(i<count){while(i)im2_objc_release(copy[--i]);free(copy);return -1;}
    array_clear(a);a->items=copy;a->count=count;return 0;
}
static void *finish(void *o,int autorelease){if(o && autorelease && !im2_objc_autorelease(o)){im2_objc_release(o);return NULL;}return o;}
void *im2_array_create(void *const *items,size_t count,int mutable,int autorelease){
    Im2ObjcClass *cls=mutable?mutable_arrays:arrays;if(!cls)return NULL;
    Array *a=im2_objc_alloc(cls,array_destroy);if(!a)return NULL;
    if(array_assign(a,items,count)){im2_objc_release(a);return NULL;}return finish(a,autorelease);
}
static int key_bytes(void *key,const char **text,size_t *bytes){
    *text=im2_string_utf8(key);if(!*text)return -1;
    *bytes=im2_string_byte_length(key);return 0;
}
static int find(Dictionary *d,void *key,size_t *index){
    const char *s;size_t n;if(key_bytes(key,&s,&n))return -1;
    for(size_t i=0;i<d->count;i++){
        const char *k;size_t kn;if(key_bytes(d->pairs[i].key,&k,&kn))return -1;
        if(n==kn && !memcmp(s,k,n)){*index=i;return 1;}
    }*index=d->count;return 0;
}
static void dict_clear(Dictionary *d){for(size_t i=0;i<d->count;i++){im2_objc_release(d->pairs[i].key);im2_objc_release(d->pairs[i].value);}free(d->pairs);d->pairs=NULL;d->count=0;d->mutation++;}
static void dict_destroy(void *o){dict_clear(o);}
static int dict_set(Dictionary *d,void *value,void *key){
    size_t index;int found=find(d,key,&index);if(found<0 || !value)return -1;
    void *retained=im2_objc_retain(value);if(!retained)return -1;
    if(found){void *previous=d->pairs[index].value;d->pairs[index].value=retained;d->mutation++;im2_objc_release(previous);return 0;}
    if(d->count==LIMIT){im2_objc_release(retained);return -1;}
    const char *text;size_t bytes;if(key_bytes(key,&text,&bytes)){im2_objc_release(retained);return -1;}
    void *copy=im2_string_create_bytes(text,bytes,0);if(!copy){im2_objc_release(retained);return -1;}
    Pair *pairs=realloc(d->pairs,(d->count+1)*sizeof(*pairs));if(!pairs){im2_objc_release(copy);im2_objc_release(retained);return -1;}
    d->pairs=pairs;pairs[d->count++]=(Pair){copy,retained};d->mutation++;return 0;
}
static int dict_assign(Dictionary *d,void *const *values,void *const *keys,size_t count){
    if(count>LIMIT || (count && (!values || !keys)))return -1;
    Dictionary staged={0};
    for(size_t i=0;i<count;i++)if(dict_set(&staged,values[i],keys[i])){dict_clear(&staged);return -1;}
    dict_clear(d);d->pairs=staged.pairs;d->count=staged.count;return 0;
}
void *im2_dictionary_create(void *const *values,void *const *keys,size_t count,int mutable,int autorelease){
    Im2ObjcClass *cls=mutable?mutable_dictionaries:dictionaries;if(!cls)return NULL;
    Dictionary *d=im2_objc_alloc(cls,dict_destroy);if(!d)return NULL;
    if(dict_assign(d,values,keys,count)){im2_objc_release(d);return NULL;}return finish(d,autorelease);
}
static uintptr_t array_init(Array *a,const char *sel){(void)sel;array_clear(a);return (uintptr_t)a;}
static uintptr_t array_init_items(Array *a,const char *sel,void *const *items,size_t count){(void)sel;if(array_assign(a,items,count)){im2_objc_release(a);return 0;}return (uintptr_t)a;}
static uintptr_t array_factory(void *cls,const char *sel){(void)sel;return (uintptr_t)im2_array_create(NULL,0,factory_mutable(cls,arrays,mutable_arrays),1);}
static uintptr_t array_factory_items(void *cls,const char *sel,void *const *items,size_t n){(void)sel;return (uintptr_t)im2_array_create(items,n,factory_mutable(cls,arrays,mutable_arrays),1);}
/* All variadic arguments here are pointers: Darwin ARM32 and AAPCS use the same
 * four-byte slots. Unlike doubles/int64, they require no alignment adaptation. */
static uintptr_t array_factory_variadic(void *cls,const char *sel,void *first,...) {
    (void)sel;void **items=NULL;size_t count=0,capacity=0;va_list ap;va_start(ap,first);
    for(void *value=first;value;value=va_arg(ap,void *)) {
        if(count==VARIADIC_ITEMS){free(items);va_end(ap);fail("NSArray variadic bridge limit exceeded");}
        if(count==capacity){capacity=capacity?capacity*2:8;void **grown=realloc(items,capacity*sizeof(*items));if(!grown){free(items);va_end(ap);fail("NSArray variadic staging failed");}items=grown;}
        items[count++]=value;
    }
    va_end(ap);void *a=im2_array_create(items,count,factory_mutable(cls,arrays,mutable_arrays),1);free(items);return (uintptr_t)a;
}
static uintptr_t array_count(Array *a,const char *sel){(void)sel;return a->count;}
static uintptr_t array_at(Array *a,const char *sel,size_t index){(void)sel;if(index>=a->count)fail("NSArray index outside bounds");return (uintptr_t)a->items[index];}
static uintptr_t array_last(Array *a,const char *sel){(void)sel;return a->count?(uintptr_t)a->items[a->count-1]:0;}
static uintptr_t array_copy(Array *a,const char *sel){(void)sel;return kind(a,mutable_arrays)?(uintptr_t)im2_array_create(a->items,a->count,0,0):(uintptr_t)im2_objc_retain(a);}
static uintptr_t array_mutable_copy(Array *a,const char *sel){(void)sel;return (uintptr_t)im2_array_create(a->items,a->count,1,0);}
static void array_dealloc(Array *a,const char *sel){(void)sel;array_clear(a);}
static void array_add(Array *a,const char *sel,void *value){
    (void)sel;if(a->count==LIMIT || !value)fail("Invalid NSMutableArray addition");
    void *retained=im2_objc_retain(value);if(!retained)fail("Unmanaged NSMutableArray value");
    void **items=realloc(a->items,(a->count+1)*sizeof(*items));if(!items){im2_objc_release(retained);fail("NSMutableArray allocation failed");}
    a->items=items;items[a->count++]=retained;a->mutation++;
}
static void array_replace(Array *a,const char *sel,size_t index,void *value){
    (void)sel;if(index>=a->count || !value)fail("Invalid NSMutableArray replacement");
    void *retained=im2_objc_retain(value);if(!retained)fail("Unmanaged NSMutableArray value");
    void *old=a->items[index];a->items[index]=retained;a->mutation++;im2_objc_release(old);
}
static void array_remove(Array *a,const char *sel,size_t index){
    (void)sel;if(index>=a->count)fail("NSMutableArray removal outside bounds");
    void *old=a->items[index];memmove(a->items+index,a->items+index+1,(a->count-index-1)*sizeof(*a->items));a->count--;a->mutation++;im2_objc_release(old);
}
static void array_remove_last(Array *a,const char *sel){if(a->count)array_remove(a,sel,a->count-1);}
static void array_remove_all(Array *a,const char *sel){(void)sel;array_clear(a);}
static uintptr_t dict_init(Dictionary *d,const char *sel){(void)sel;dict_clear(d);return (uintptr_t)d;}
static uintptr_t dict_init_items(Dictionary *d,const char *sel,void *const *values,void *const *keys,size_t count){(void)sel;if(dict_assign(d,values,keys,count)){im2_objc_release(d);return 0;}return (uintptr_t)d;}
static uintptr_t dict_factory(void *cls,const char *sel){(void)sel;return (uintptr_t)im2_dictionary_create(NULL,NULL,0,factory_mutable(cls,dictionaries,mutable_dictionaries),1);}
static uintptr_t dict_factory_one(void *cls,const char *sel,void *value,void *key){(void)sel;return (uintptr_t)im2_dictionary_create(&value,&key,1,factory_mutable(cls,dictionaries,mutable_dictionaries),1);}
static uintptr_t dict_factory_items(void *cls,const char *sel,void *const *values,void *const *keys,size_t count){(void)sel;return (uintptr_t)im2_dictionary_create(values,keys,count,factory_mutable(cls,dictionaries,mutable_dictionaries),1);}
static uintptr_t dict_factory_variadic(void *cls,const char *sel,void *first,...) {
    (void)sel;Dictionary *d=im2_dictionary_create(NULL,NULL,0,factory_mutable(cls,dictionaries,mutable_dictionaries),0);if(!d)return 0;
    va_list ap;va_start(ap,first);size_t count=0;
    for(void *value=first;value;value=va_arg(ap,void *)) {
        if(count++==VARIADIC_PAIRS){va_end(ap);im2_objc_release(d);fail("NSDictionary variadic bridge limit exceeded");}
        void *key=va_arg(ap,void *);
        if(!key || dict_set(d,value,key)){va_end(ap);im2_objc_release(d);fail("NSDictionary variadic key/value rejected");}
    }
    va_end(ap);return (uintptr_t)finish(d,1);
}
static uintptr_t dict_count(Dictionary *d,const char *sel){(void)sel;return d->count;}
static uintptr_t dict_get(Dictionary *d,const char *sel,void *key){(void)sel;size_t i;int found=find(d,key,&i);if(found<0)fail("NSDictionary requires a supported NSString key");return found?(uintptr_t)d->pairs[i].value:0;}
static void dict_put(Dictionary *d,const char *sel,void *value,void *key){(void)sel;if(dict_set(d,value,key))fail("NSMutableDictionary key/value or allocation rejected");}
static void dict_remove(Dictionary *d,const char *sel,void *key){
    (void)sel;size_t i;int found=find(d,key,&i);if(found<0)fail("NSMutableDictionary key rejected");if(!found)return;
    Pair old=d->pairs[i];memmove(d->pairs+i,d->pairs+i+1,(d->count-i-1)*sizeof(*d->pairs));d->count--;d->mutation++;im2_objc_release(old.key);im2_objc_release(old.value);
}
static void dict_remove_all(Dictionary *d,const char *sel){(void)sel;dict_clear(d);}
static uintptr_t dict_clone(Dictionary *d,int mutable){
    Dictionary *copy=im2_dictionary_create(NULL,NULL,0,mutable,0);if(!copy)return 0;
    for(size_t i=0;i<d->count;i++)if(dict_set(copy,d->pairs[i].value,d->pairs[i].key)){im2_objc_release(copy);return 0;}
    return (uintptr_t)copy;
}
static uintptr_t dict_copy(Dictionary *d,const char *sel){(void)sel;return kind(d,mutable_dictionaries)?dict_clone(d,0):(uintptr_t)im2_objc_retain(d);}
static uintptr_t dict_mutable_copy(Dictionary *d,const char *sel){(void)sel;return dict_clone(d,1);}
static uintptr_t dict_keys(Dictionary *d,const char *sel){
    (void)sel;void **keys=d->count?calloc(d->count,sizeof(*keys)):NULL;if(d->count && !keys)return 0;
    for(size_t i=0;i<d->count;i++)keys[i]=d->pairs[i].key;
    void *a=im2_array_create(keys,d->count,0,1);free(keys);return (uintptr_t)a;
}
static void dict_dealloc(Dictionary *d,const char *sel){(void)sel;dict_clear(d);}
static uintptr_t enumerate(void *identity,size_t count,uintptr_t *mutation,
    Im2FastEnumerationState *state,void **buffer,size_t length,int dictionary) {
    if(!state || !buffer || !length || length>LIMIT)fail("Invalid fast enumeration buffer");
    if(!state->state){state->extra[0]=(uintptr_t)identity;state->extra[1]=*mutation;}
    else if(state->extra[0]!=(uintptr_t)identity || state->extra[1]!=*mutation)fail("Collection changed during enumeration");
    if(state->state>count)fail("Invalid fast enumeration position");
    size_t n=count-state->state;if(n>length)n=length;
    for(size_t i=0;i<n;i++){size_t at=state->state+i;buffer[i]=dictionary?((Dictionary *)identity)->pairs[at].key:((Array *)identity)->items[at];}
    state->state+=n;state->items=buffer;state->mutations=mutation;return n;
}
static uintptr_t array_enumerate(Array *a,const char *sel,Im2FastEnumerationState *state,void **buffer,size_t length){(void)sel;return enumerate(a,a->count,&a->mutation,state,buffer,length,0);}
static uintptr_t dict_enumerate(Dictionary *d,const char *sel,Im2FastEnumerationState *state,void **buffer,size_t length){(void)sel;return enumerate(d,d->count,&d->mutation,state,buffer,length,1);}
#define M(s,t,f) {s,t,(Im2Imp)(f),0}
#define N(a) (sizeof(a)/sizeof(*(a)))
int im2_collections_init(Im2ObjcClass *root,void (*on_failure)(const char *)){
    const Im2ObjcClass *r=im2_objc_class_metadata(root);
    if(arrays || !r || r->data->ro->flags&1u || !im2_objc_class_named("NSString"))return -1;
    size_t checkpoint=im2_objc_registration_checkpoint();failure=on_failure;
    const Im2MethodSpec ai[]={M("init","@@:",array_init),M("initWithObjects:count:","@@:^@I",array_init_items),M("count","I@:",array_count),M("countByEnumeratingWithState:objects:count:","I@:^v^@I",array_enumerate),M("objectAtIndex:","@@:I",array_at),M("lastObject","@@:",array_last),M("copy","@@:",array_copy),M("mutableCopy","@@:",array_mutable_copy),M("dealloc","v@:",array_dealloc)};
    const Im2MethodSpec ac[]={M("array","@@:",array_factory),M("arrayWithObjects:count:","@@:^@I",array_factory_items),M("arrayWithObjects:","@@:@",array_factory_variadic)};
    arrays=im2_objc_register_class("NSArray",root,sizeof(Array),ai,N(ai),ac,N(ac));
    const Im2MethodSpec ma[]={M("addObject:","v@:@",array_add),M("replaceObjectAtIndex:withObject:","v@:I@",array_replace),M("removeObjectAtIndex:","v@:I",array_remove),M("removeLastObject","v@:",array_remove_last),M("removeAllObjects","v@:",array_remove_all)};
    mutable_arrays=arrays?im2_objc_register_class("NSMutableArray",arrays,sizeof(Array),ma,N(ma),NULL,0):NULL;
    const Im2MethodSpec di[]={M("init","@@:",dict_init),M("initWithObjects:forKeys:count:","@@:^@^@I",dict_init_items),M("count","I@:",dict_count),M("countByEnumeratingWithState:objects:count:","I@:^v^@I",dict_enumerate),M("objectForKey:","@@:@",dict_get),M("allKeys","@@:",dict_keys),M("copy","@@:",dict_copy),M("mutableCopy","@@:",dict_mutable_copy),M("dealloc","v@:",dict_dealloc)};
    const Im2MethodSpec dc[]={M("dictionary","@@:",dict_factory),M("dictionaryWithObject:forKey:","@@:@@",dict_factory_one),M("dictionaryWithObjectsAndKeys:","@@:@",dict_factory_variadic),M("dictionaryWithObjects:forKeys:count:","@@:^@^@I",dict_factory_items)};
    dictionaries=mutable_arrays?im2_objc_register_class("NSDictionary",root,sizeof(Dictionary),di,N(di),dc,N(dc)):NULL;
    const Im2MethodSpec md[]={M("setObject:forKey:","v@:@@",dict_put),M("removeObjectForKey:","v@:@",dict_remove),M("removeAllObjects","v@:",dict_remove_all)};
    mutable_dictionaries=dictionaries?im2_objc_register_class("NSMutableDictionary",dictionaries,sizeof(Dictionary),md,N(md),NULL,0):NULL;
    if(!mutable_dictionaries){(void)im2_objc_registration_rollback(checkpoint);arrays=mutable_arrays=dictionaries=mutable_dictionaries=NULL;return -1;}return 0;
}
