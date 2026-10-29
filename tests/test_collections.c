/* SPDX-License-Identifier: MIT. Tests invoke the actual registered native collection methods. */
#define _POSIX_C_SOURCE 200809L
#include "collections.h"
#include "foundation.h"
#include "cfstring.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static Im2Imp method(void *o,const char *sel){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,sel,NULL,&t));assert(!t.guest && !t.stret_size);return t.imp;}
static uintptr_t send0(void *o,const char *sel){return ((uintptr_t(*)(void *,const char *))method(o,sel))(o,sel);}
static uintptr_t send1(void *o,const char *sel,void *value){return ((uintptr_t(*)(void *,const char *,void *))method(o,sel))(o,sel,value);}
static void call0(void *o,const char *sel){((void(*)(void *,const char *))method(o,sel))(o,sel);}
static void call1(void *o,const char *sel,void *value){((void(*)(void *,const char *,void *))method(o,sel))(o,sel,value);}
static void set(void *o,void *value,void *key){((void(*)(void *,const char *,void *,void *))method(o,"setObject:forKey:"))(o,"setObject:forKey:",value,key);}
static void replace(void *o,size_t index,void *value){((void(*)(void *,const char *,size_t,void *))method(o,"replaceObjectAtIndex:withObject:"))(o,"replaceObjectAtIndex:withObject:",index,value);}
static uintptr_t at(void *o,size_t index){return ((uintptr_t(*)(void *,const char *,size_t))method(o,"objectAtIndex:"))(o,"objectAtIndex:",index);}
static void destroy(void *o){call0(o,"dealloc");}
static uintptr_t allocate(Im2ObjcClass *cls,const char *sel){(void)sel;return (uintptr_t)im2_objc_alloc(cls,destroy);}
static void failure(const char *why){(void)why;_exit(73);}
static uintptr_t enumerate(void *o,Im2FastEnumerationState *state,void **items,size_t n){
    const char *sel="countByEnumeratingWithState:objects:count:";
    return ((uintptr_t(*)(void *,const char *,Im2FastEnumerationState *,void **,size_t))method(o,sel))(o,sel,state,items,n);
}
typedef uintptr_t(*VariadicFactory)(void *,const char *,void *,...);
static void expect_failure(void *object,int which){
    pid_t child=fork();assert(child>=0);
    if(!child){if(which==0)(void)at(object,999);else if(which==1)call1(object,"addObject:",NULL);else if(which==2)set(object,NULL,NULL);else if(which==3)(void)send1(object,"objectForKey:",object);
    else if(which==5){void *cls=im2_objc_class_named("NSArray");(void)((VariadicFactory)method(cls,"arrayWithObjects:"))(cls,"arrayWithObjects:",object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,NULL);}
    else if(which==6){void *cls=im2_objc_class_named("NSDictionary");(void)((VariadicFactory)method(cls,"dictionaryWithObjectsAndKeys:"))(cls,"dictionaryWithObjectsAndKeys:",object,NULL);}
    else if(which==7){void *cls=im2_objc_class_named("NSDictionary");(void)((VariadicFactory)method(cls,"dictionaryWithObjectsAndKeys:"))(cls,"dictionaryWithObjectsAndKeys:",object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,object,NULL);}
    else {
        Im2FastEnumerationState state={0};void *buffer[1];assert(enumerate(object,&state,buffer,1)==1);
        call0(object,"removeAllObjects");(void)enumerate(object,&state,buffer,1);
    }_exit(74);}
    int status;assert(waitpid(child,&status,0)==child);assert(WIFEXITED(status) && WEXITSTATUS(status)==73);
}
static void check_missing(void *o,const char *sel){Im2ObjcTarget t;assert(im2_objc_lookup_target(o,sel,NULL,&t));}
int main(void){
    const Im2MethodSpec cm[]={{"alloc","@@:",(Im2Imp)allocate,0}};
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,cm,1);assert(root);
    assert(im2_collections_init(root,failure)); /* NSString must already exist. */
    Im2FoundationConfig cfg={"/tmp",NULL,NULL,NULL};assert(!im2_foundation_init(root,&cfg));
    assert(!im2_collections_init(root,failure));assert(im2_collections_init(root,failure));
    void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
    void *a=im2_string_create("one",0),*b=im2_string_create("two",0),*key=im2_string_create("key",0),*equal=im2_string_create("key",0);assert(a && b && key && equal);
    expect_failure(a,5);expect_failure(a,6);expect_failure(a,7);
    void *items[]={a,b};void *array=im2_array_create(items,2,0,0);assert(array);
    assert(im2_objc_retain_count(a)==2 && send0(array,"count")==2 && at(array,0)==(uintptr_t)a && send0(array,"lastObject")== (uintptr_t)b);
    check_missing(array,"addObject:");
    Im2FastEnumerationState state={0};void *buffer[1];
    assert(enumerate(array,&state,buffer,1)==1 && buffer[0]==a && state.items==buffer && state.mutations && *state.mutations==state.extra[1]);
    assert(enumerate(array,&state,buffer,1)==1 && buffer[0]==b);
    assert(!enumerate(array,&state,buffer,1));
    void *variadic=(void *)((VariadicFactory)method(im2_objc_class_named("NSArray"),"arrayWithObjects:"))(im2_objc_class_named("NSArray"),"arrayWithObjects:",a,b,a,b,NULL);
    assert(variadic && send0(variadic,"count")==4 && at(variadic,3)==(uintptr_t)b);
    void *empty=(void *)((VariadicFactory)method(im2_objc_class_named("NSArray"),"arrayWithObjects:"))(im2_objc_class_named("NSArray"),"arrayWithObjects:",NULL);
    assert(empty && !send0(empty,"count"));
    void *copy=(void *)send0(array,"copy");assert(copy==array && im2_objc_retain_count(array)==2);assert(!im2_objc_release(copy));
    void *mutable=(void *)send0(array,"mutableCopy");assert(mutable && mutable!=array);
    call1(mutable,"addObject:",a);assert(send0(mutable,"count")==3 && send0(array,"count")==2);
    expect_failure(mutable,4);
    replace(mutable,0,b);assert(at(mutable,0)==(uintptr_t)b);
    call0(mutable,"removeLastObject");assert(send0(mutable,"count")==2);
    void *snapshot=(void *)send0(mutable,"copy");assert(snapshot && snapshot!=mutable);check_missing(snapshot,"addObject:");
    call0(mutable,"removeAllObjects");assert(!send0(mutable,"count") && send0(snapshot,"count")==2);
    expect_failure(array,0);expect_failure(mutable,1);
    assert(!im2_array_create(NULL,1,0,0));assert(!im2_array_create(items,65537,0,0));
    void *baditems[]={a,NULL};size_t before=im2_objc_retain_count(a);assert(!im2_array_create(baditems,2,0,0));assert(im2_objc_retain_count(a)==before);
    void *dict=im2_dictionary_create(&a,&key,1,1,0);assert(dict && send0(dict,"count")==1);
    assert(send1(dict,"objectForKey:",equal)==(uintptr_t)a);
    set(dict,b,equal);assert(send0(dict,"count")==1 && send1(dict,"objectForKey:",key)==(uintptr_t)b);
    set(dict,b,key);
    expect_failure(dict,4);
    state=(Im2FastEnumerationState){0};assert(enumerate(dict,&state,buffer,1)==1 && !strcmp(im2_string_utf8(buffer[0]),"key"));assert(!enumerate(dict,&state,buffer,1));
    void *var_dict=(void *)((VariadicFactory)method(im2_objc_class_named("NSDictionary"),"dictionaryWithObjectsAndKeys:"))(im2_objc_class_named("NSDictionary"),"dictionaryWithObjectsAndKeys:",a,key,b,equal,NULL);
    assert(var_dict && send0(var_dict,"count")==1 && send1(var_dict,"objectForKey:",key)==(uintptr_t)b);
    /* replacement with identical value must retain before release */
    void *keys=(void *)send0(dict,"allKeys");assert(keys && send0(keys,"count")==1);
    void *stored=(void *)at(keys,0);assert(stored!=key && stored!=equal && !strcmp(im2_string_utf8(stored),"key"));
    void *dictcopy=(void *)send0(dict,"copy");assert(dictcopy && dictcopy!=dict);check_missing(dictcopy,"setObject:forKey:");
    void *dictmutable=(void *)send0(dictcopy,"mutableCopy");assert(dictmutable);
    call1(dict,"removeObjectForKey:",equal);assert(!send0(dict,"count") && send0(dictcopy,"count")==1);
    call1(dict,"removeObjectForKey:",equal);assert(!send0(dict,"count"));
    assert(!send1(dict,"objectForKey:",key));expect_failure(dict,2);expect_failure(dict,3);
    /* Binary-safe key equality must distinguish embedded NUL tails. */
    void *nulkey=im2_string_create_bytes("k\0a",3,0),*nulkey2=im2_string_create_bytes("k\0b",3,0),*nulmatch=im2_string_create_bytes("k\0a",3,0);assert(nulkey && nulkey2 && nulmatch);
    assert(send0(nulkey,"length")==3);set(dict,a,nulkey);set(dict,b,nulkey2);
    assert(send0(dict,"count")==2 && send1(dict,"objectForKey:",nulmatch)== (uintptr_t)a);
    void *dupvalues[]={a,b},*dupkeys[]={key,equal};void *dedup=im2_dictionary_create(dupvalues,dupkeys,2,0,0);assert(dedup && send0(dedup,"count")==1 && send1(dedup,"objectForKey:",key)== (uintptr_t)b);
    void *invalidkeys[]={key,array};before=im2_objc_retain_count(a);assert(!im2_dictionary_create(dupvalues,invalidkeys,2,0,0));assert(im2_objc_retain_count(a)==before);
    void *invalidvalues[]={a,NULL};assert(!im2_dictionary_create(invalidvalues,dupkeys,2,0,0));assert(im2_objc_retain_count(a)==before);
    void *nested=im2_array_create(&dict,1,0,0);assert(nested && im2_objc_retain_count(dict)==2);
    assert(!im2_objc_release(nested));
    /* Inherited allocation and initialization use production collection layouts. */
    void *allocated=(void *)send0(im2_objc_class_named("NSMutableArray"),"alloc");assert(allocated && send0(allocated,"init")== (uintptr_t)allocated);call1(allocated,"addObject:",a);assert(at(allocated,0)== (uintptr_t)a);
    void *factory=(void *)send0(im2_objc_class_named("NSMutableDictionary"),"dictionary");assert(factory);set(factory,a,key);
    call0(dictmutable,"removeAllObjects");
    assert(!im2_objc_release(allocated) && !im2_objc_release(dedup));
    assert(!im2_objc_release(dictmutable) && !im2_objc_release(dictcopy) && !im2_objc_release(dict));
    assert(!im2_objc_release(nulkey) && !im2_objc_release(nulkey2) && !im2_objc_release(nulmatch));
    assert(!im2_objc_release(snapshot) && !im2_objc_release(mutable) && !im2_objc_release(array));
    assert(!im2_objc_release(key) && !im2_objc_release(equal));
    /* Factories/allKeys keep their values until the actual pool drains. */
    assert(!im2_objc_pool_drain(pool));assert(im2_objc_retain_count(a)==1 && im2_objc_retain_count(b)==1);
    assert(!im2_objc_release(a) && !im2_objc_release(b));
    assert(!im2_array_create(NULL,0,0,1) && !im2_dictionary_create(NULL,NULL,0,0,1));
    puts("PASS: actual array/dictionary methods, retained contents, string-key copies, binary-safe equality, mutation/snapshots, bounds rejection and nested pool lifetimes");
}
