/* SPDX-License-Identifier: MIT. Owned immutable membership, not a placeholder class.
 * Value equality supports native strings/numbers/sets; other objects must use
 * NSObject identity semantics. Arbitrary custom equality/hash is not emulated. */
#include "sets.h"
#include "collections.h"
#include "foundation.h"
#include "numbers.h"
#include <stdlib.h>
#include <string.h>
#define LIMIT 4096u
typedef struct {Im2ObjcClass *isa;void **items;size_t count;uintptr_t mutation;} Set;
static Im2ObjcClass *set_class;
static int equal(const void *a,const void *b,unsigned depth){
 if(a==b)return 1;
 if(depth>64)return -1;
 const char *sa=im2_string_utf8(a),*sb=im2_string_utf8(b);
 if(sa && sb){size_t n=im2_string_byte_length(a);return n==im2_string_byte_length(b) && !memcmp(sa,sb,n);}
 int value;if(!im2_numbers_equal(a,b,&value))return value;
 if(im2_objc_object_class(a)==set_class && im2_objc_object_class(b)==set_class){
  const Set *x=a,*y=b;if(x->count!=y->count)return 0;
  for(size_t i=0;i<x->count;i++){int found=0;for(size_t j=0;j<y->count;j++){int e=equal(x->items[i],y->items[j],depth+1);if(e<0)return -1;if(e){found=1;break;}}if(!found)return 0;}return 1;
 }
 /* Distinct supported value types are unequal. */
 if(sa || sb || !im2_number_bool_value(a,&value) || !im2_number_bool_value(b,&value) || im2_objc_object_class(a)==set_class || im2_objc_object_class(b)==set_class)return 0;
 Im2ObjcTarget t;
 if(!im2_objc_lookup_target(a,"isEqual:",NULL,&t) || !im2_objc_lookup_target(b,"isEqual:",NULL,&t))return -1;
 return 0;
}
static void cleanup(void *o){Set *s=o;for(size_t i=0;i<s->count;i++)im2_objc_release(s->items[i]);free(s->items);s->items=NULL;s->count=0;}
void *im2_set_create(void *const *items,size_t count,int autorelease){
 if(!set_class || count>LIMIT || (count && !items))return NULL;
 Set *s=im2_objc_alloc(set_class,cleanup);if(!s)return NULL;
 s->items=count?calloc(count,sizeof(*s->items)):NULL;if(count && !s->items){im2_objc_release(s);return NULL;}
 for(size_t i=0;i<count;i++){
  if(!items[i] || !im2_objc_retain_count(items[i]))goto bad;
  int found=0;for(size_t j=0;j<s->count;j++){int e=equal(items[i],s->items[j],0);if(e<0)goto bad;if(e){found=1;break;}}
  if(!found){void *held=im2_objc_retain(items[i]);if(!held)goto bad;s->items[s->count++]=held;}
 }
 if(autorelease && !im2_objc_autorelease(s))goto bad;
 return s;
bad:im2_objc_release(s);return NULL;
}
static uintptr_t count(Set *s,const char *sel){(void)sel;return s->count;}
static uintptr_t any(Set *s,const char *sel){(void)sel;return s->count?(uintptr_t)s->items[0]:0;}
static uintptr_t member(Set *s,const char *sel,void *o){(void)sel;for(size_t i=0;i<s->count;i++){int e=equal(o,s->items[i],0);if(e<0)abort();if(e)return (uintptr_t)s->items[i];}return 0;}
static uint32_t contains(Set *s,const char *sel,void *o){return member(s,sel,o)!=0;}
static uint32_t is_equal(Set *s,const char *sel,void *o){(void)sel;int e=equal(s,o,0);if(e<0)abort();return e!=0;}
static uintptr_t all(Set *s,const char *sel){(void)sel;return (uintptr_t)im2_array_create(s->items,s->count,0,1);}
static uintptr_t copy(void *s,const char *sel){(void)sel;return (uintptr_t)im2_objc_retain(s);}
static void dealloc(void *s,const char *sel){(void)sel;cleanup(s);}
static uintptr_t factory(void *cls,const char *sel,void *const *items,size_t n){(void)sel;if(cls!=set_class)return 0;return (uintptr_t)im2_set_create(items,n,1);}
static uintptr_t empty(void *cls,const char *sel){return factory(cls,sel,NULL,0);}
static uintptr_t one(void *cls,const char *sel,void *o){return factory(cls,sel,&o,1);}
static uintptr_t from_array(void *cls,const char *sel,void *array){
 Im2ObjcClass *base=im2_objc_class_named("NSArray"),*cursor=im2_objc_object_class(array);
 while(cursor && cursor!=base){const Im2ObjcClass *m=im2_objc_class_metadata(cursor);cursor=m?m->superclass:NULL;}if(!cursor)return 0;
 Im2ObjcTarget c,a;if(!array || im2_objc_lookup_target(array,"count",NULL,&c) || c.guest || c.stret_size || im2_objc_lookup_target(array,"objectAtIndex:",NULL,&a) || a.guest || a.stret_size)return 0;
 size_t n=((uintptr_t(*)(void *,const char *))c.imp)(array,"count");if(n>LIMIT)return 0;
 void **items=n?calloc(n,sizeof(*items)):NULL;if(n && !items)return 0;
 for(size_t i=0;i<n;i++)items[i]=(void *)((uintptr_t(*)(void *,const char *,size_t))a.imp)(array,"objectAtIndex:",i);
 uintptr_t result=factory(cls,sel,items,n);free(items);return result;
}
static uintptr_t enumerate(Set *s,const char *sel,Im2FastEnumerationState *state,void **buffer,size_t length){
 (void)sel;if(!state || !buffer || !length || length>LIMIT || state->state>s->count)abort();
 if(state->state && state->extra[0]!=(uintptr_t)s)abort();
 state->extra[0]=(uintptr_t)s;
 size_t n=s->count-state->state;if(n>length)n=length;
 for(size_t i=0;i<n;i++)buffer[i]=s->items[state->state+i];
 state->state+=n;state->items=buffer;state->mutations=&s->mutation;return n;
}
#define M(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_sets_init(Im2ObjcClass *root){
 const Im2ObjcClass *r=im2_objc_class_metadata(root);if(set_class || !r || r->data->ro->flags&1 || !im2_objc_class_named("NSArray"))return -1;
 const Im2MethodSpec im[]={M("count","I@:",count),M("anyObject","@@:",any),M("member:","@@:@",member),M("containsObject:","c@:@",contains),M("allObjects","@@:",all),M("copy","@@:",copy),M("isEqual:","c@:@",is_equal),M("dealloc","v@:",dealloc),M("countByEnumeratingWithState:objects:count:","I@:^v^@I",enumerate)};
 const Im2MethodSpec cm[]={M("set","@@:",empty),M("setWithObject:","@@:@",one),M("setWithArray:","@@:@",from_array),M("setWithObjects:count:","@@:^@I",factory)};
 set_class=im2_objc_register_class("NSSet",root,sizeof(Set),im,sizeof(im)/sizeof(*im),cm,sizeof(cm)/sizeof(*cm));return set_class?0:-1;
}
