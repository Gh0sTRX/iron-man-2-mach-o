/* SPDX-License-Identifier: MIT. Immutable NSSet native behavior and membership ownership. */
#include "sets.h"
#include "numbers.h"
#include "collections.h"
#include "foundation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Im2Imp method(void *o,const char *s){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,s,NULL,&t) && !t.guest && !t.stret_size);return t.imp;}
static uintptr_t get(void *o,const char *s){return ((uintptr_t(*)(void *,const char *))method(o,s))(o,s);}
static uintptr_t get1(void *o,const char *s,void *v){return ((uintptr_t(*)(void *,const char *,void *))method(o,s))(o,s,v);}
static uint32_t eq(void *o,const char *s,void *v){return ((uint32_t(*)(void *,const char *,void *))method(o,s))(o,s,v);}
int main(void){
 Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root);Im2FoundationConfig cfg={"/tmp",NULL,NULL,NULL};assert(!im2_foundation_init(root,&cfg) && !im2_collections_init(root,NULL) && !im2_numbers_init(root) && !im2_sets_init(root));
 assert(im2_sets_init(root));
 void *a=im2_string_create("same",0),*b=im2_string_create("same",0),*one=im2_number_signed(1,0),*yes=im2_number_bool(1,0);assert(a && b && one && yes);
 void *items[]={a,b,one,yes};void *set=im2_set_create(items,4,0);assert(set && get(set,"count")==2 && get1(set,"member:",b)==(uintptr_t)a && eq(set,"containsObject:",yes));
 void *items2[]={yes,b};void *same=im2_set_create(items2,2,0);assert(same && eq(set,"isEqual:",same));
 void *bad[]={a,NULL};size_t refs=im2_objc_retain_count(a);assert(!im2_set_create(bad,2,0) && im2_objc_retain_count(a)==refs);assert(!im2_set_create(items,4097,0));
 void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
 void *all=(void *)get(set,"allObjects");assert(all && get(all,"count")==2);
 void *from=(void *)get1(im2_objc_class_named("NSSet"),"setWithArray:",all);assert(from && eq(from,"isEqual:",set));
 Im2FastEnumerationState state={0};void *buffer[1];const char *sel="countByEnumeratingWithState:objects:count:";
 typedef uintptr_t(*Enumerate)(void *,const char *,Im2FastEnumerationState *,void **,size_t);
 Enumerate enumerate=(Enumerate)method(set,sel);assert(enumerate(set,sel,&state,buffer,1)==1 && state.items==buffer && state.mutations && !*state.mutations);assert(enumerate(set,sel,&state,buffer,1)==1);assert(!enumerate(set,sel,&state,buffer,1));
 void *copy=(void *)get(set,"copy");assert(copy==set && !im2_objc_release(copy));
 assert(!im2_objc_release(set) && !im2_objc_release(same));assert(!im2_objc_pool_drain(pool));
 assert(im2_objc_retain_count(a)==1 && im2_objc_retain_count(one)==1);assert(!im2_objc_release(a) && !im2_objc_release(b) && !im2_objc_release(one) && !im2_objc_release(yes));
 assert(!im2_set_create(NULL,0,1));puts("PASS: real NSSet deduplication, native string/number value equality, membership, NSArray bridge, immutable enumeration and ownership");
}
