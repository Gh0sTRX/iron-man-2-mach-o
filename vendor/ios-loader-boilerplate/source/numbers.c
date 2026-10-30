/* SPDX-License-Identifier: MIT. Independently authored integral NSNumber objects.
 * Float/double getters return integer bit patterns for iOS SoftFP r0[/r1].
 * Floating point constructors and arbitrary NSObject hashing are not implemented. */
#include "numbers.h"
#include "foundation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {BOOL_KIND,SIGNED32,UNSIGNED32,SIGNED64,UNSIGNED64};
typedef struct {Im2ObjcClass *isa;uint64_t bits;unsigned kind;} Number;
static Im2ObjcClass *numbers;
static int is_number(const void *o){return numbers && im2_objc_object_class(o)==numbers;}
static int is_signed(const Number *n){return n->kind==SIGNED32 || n->kind==SIGNED64;}
static int64_t signed_bits(const Number *n){int64_t v;memcpy(&v,&n->bits,8);return v;}
static void *create(uint64_t bits,unsigned kind,int autorelease){
 if(!numbers)return NULL;
 Number *n=im2_objc_alloc(numbers,NULL);if(!n)return NULL;n->bits=bits;n->kind=kind;
 if(autorelease && !im2_objc_autorelease(n)){im2_objc_release(n);return NULL;}return n;
}
void *im2_number_bool(int value,int autorelease){return create(!!value,BOOL_KIND,autorelease);}
void *im2_number_signed(int64_t value,int autorelease){return create((uint64_t)value,SIGNED64,autorelease);}
void *im2_number_unsigned(uint64_t value,int autorelease){return create(value,UNSIGNED64,autorelease);}
int im2_number_bool_value(const void *o,int *value){if(!value || !is_number(o))return -1;*value=((const Number *)o)->bits!=0;return 0;}
static void factory_class(void *cls){if(cls!=numbers)abort();}
static uintptr_t with_bool(void *cls,const char *sel,uint32_t v){(void)sel;factory_class(cls);return (uintptr_t)im2_number_bool((uint8_t)v!=0,1);}
static uintptr_t with_int(void *cls,const char *sel,int32_t v){(void)sel;factory_class(cls);return (uintptr_t)create((uint64_t)(int64_t)v,SIGNED32,1);}
static uintptr_t with_uint(void *cls,const char *sel,uint32_t v){(void)sel;factory_class(cls);return (uintptr_t)create(v,UNSIGNED32,1);}
static uintptr_t with_longlong(void *cls,const char *sel,uint32_t lo,uint32_t hi){(void)sel;factory_class(cls);return (uintptr_t)create((uint64_t)hi<<32|lo,SIGNED64,1);}
static uintptr_t with_ulonglong(void *cls,const char *sel,uint32_t lo,uint32_t hi){(void)sel;factory_class(cls);return (uintptr_t)create((uint64_t)hi<<32|lo,UNSIGNED64,1);}
static uint32_t bool_value(Number *n,const char *sel){(void)sel;return n->bits!=0;}
static uint32_t int_value(Number *n,const char *sel){(void)sel;return (uint32_t)n->bits;}
static uint64_t longlong_value(Number *n,const char *sel){(void)sel;return n->bits;}
static uint32_t float_value(Number *n,const char *sel){(void)sel;float f=is_signed(n)?(float)signed_bits(n):(float)n->bits;uint32_t bits;memcpy(&bits,&f,4);return bits;}
static uint64_t double_value(Number *n,const char *sel){(void)sel;double d=is_signed(n)?(double)signed_bits(n):(double)n->bits;uint64_t bits;memcpy(&bits,&d,8);return bits;}
static uintptr_t type_value(Number *n,const char *sel){(void)sel;static const char *types[]={"c","i","I","q","Q"};return (uintptr_t)types[n->kind];}
static uint32_t equal_value(Number *a,const char *sel,void *other){
 (void)sel;if(!is_number(other))return 0;Number *b=other;
 int aneg=is_signed(a) && signed_bits(a)<0,bneg=is_signed(b) && signed_bits(b)<0;
 return aneg==bneg && a->bits==b->bits;
}
static uintptr_t copy_value(void *o,const char *sel){(void)sel;return (uintptr_t)im2_objc_retain(o);}
static uintptr_t string_value(Number *n,const char *sel){(void)sel;char out[32];if(is_signed(n))snprintf(out,sizeof(out),"%lld",(long long)signed_bits(n));else snprintf(out,sizeof(out),"%llu",(unsigned long long)n->bits);return (uintptr_t)im2_string_create(out,1);}
#define M(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_numbers_init(Im2ObjcClass *root){
 const Im2ObjcClass *r=im2_objc_class_metadata(root);if(numbers || !r || r->data->ro->flags&1)return -1;
 const Im2MethodSpec im[]={M("boolValue","c@:",bool_value),M("intValue","i@:",int_value),M("integerValue","i@:",int_value),M("unsignedIntValue","I@:",int_value),M("unsignedIntegerValue","I@:",int_value),M("longLongValue","q@:",longlong_value),M("unsignedLongLongValue","Q@:",longlong_value),M("floatValue","f@:",float_value),M("doubleValue","d@:",double_value),M("objCType","*@:",type_value),M("copy","@@:",copy_value),M("isEqual:","c@:@",equal_value),M("isEqualToNumber:","c@:@",equal_value),M("stringValue","@@:",string_value),M("description","@@:",string_value)};
 const Im2MethodSpec cm[]={M("numberWithBool:","@@:c",with_bool),M("numberWithInt:","@@:i",with_int),M("numberWithInteger:","@@:i",with_int),M("numberWithUnsignedInt:","@@:I",with_uint),M("numberWithUnsignedInteger:","@@:I",with_uint),M("numberWithLongLong:","@@:q",with_longlong),M("numberWithUnsignedLongLong:","@@:Q",with_ulonglong)};
 numbers=im2_objc_register_class("NSNumber",root,sizeof(Number),im,sizeof(im)/sizeof(*im),cm,sizeof(cm)/sizeof(*cm));return numbers?0:-1;
}
