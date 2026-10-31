/* SPDX-License-Identifier: MIT. Real wall clock, not process uptime.
 * Calendar/local-time formatting, timezone preferences and custom NSDate subclasses
 * are outside this measured subset. Native libc errno mapping remains separate.
 */
#include "time_bridge.h"
#include "imports.h"
#include "log.h"
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#define APPLE_EPOCH 978307200.0
#if defined(__arm__)
#define TIME_PCS SOFTFP
#else
#define TIME_PCS
#endif
typedef struct {int32_t sec,min,hour,mday,mon,year,wday,yday,isdst,gmtoff;const char *zone;} DarwinTm;
#if defined(__arm__)
_Static_assert(sizeof(time_t)==4 && sizeof(struct timeval)==8,"Pinned SDK clock ABI");
_Static_assert(sizeof(DarwinTm)==44 && offsetof(DarwinTm,gmtoff)==36 && offsetof(DarwinTm,zone)==40,"Darwin32 tm layout");
#endif
static int wall(struct timeval *tv){
 if(gettimeofday(tv,NULL))return -1;
 if(tv->tv_usec<0 || tv->tv_usec>=1000000){errno=EINVAL;return -1;}
 return 0;
}
static double current_reference(void){struct timeval tv;if(wall(&tv))fatal_error("Wall clock unavailable");return ((double)tv.tv_sec-APPLE_EPOCH)+(double)tv.tv_usec/1000000.;}
TIME_PCS static double cf_now(void){return current_reference();}
TIME_PCS static double time_difference(int32_t end,int32_t start){return (double)end-(double)start;}
static int32_t time_now(int32_t *out){
 struct timeval tv;if(wall(&tv))return -1;
 if((int64_t)tv.tv_sec<INT32_MIN || (int64_t)tv.tv_sec>INT32_MAX){errno=EOVERFLOW;return -1;}
 int32_t value=(int32_t)tv.tv_sec;if(out)*out=value;return value;
}
static DarwinTm utc_result;
static DarwinTm *utc(const int32_t *seconds){
 if(!seconds){errno=EINVAL;return NULL;}
 time_t input=(time_t)*seconds;struct tm *t=gmtime(&input);if(!t)return NULL;
 utc_result=(DarwinTm){t->tm_sec,t->tm_min,t->tm_hour,t->tm_mday,t->tm_mon,t->tm_year,t->tm_wday,t->tm_yday,t->tm_isdst,0,"GMT"};return &utc_result;
}
typedef struct {Im2ObjcClass *isa;double reference_seconds;} Date;
static Im2ObjcClass *date_class;
static Date *date_checked(void *o){if(!date_class || im2_objc_object_class(o)!=date_class || !im2_objc_retain_count(o))fatal_error("Unsupported NSDate receiver");return o;}
static double decoded(uint64_t raw){double d;memcpy(&d,&raw,8);if(!isfinite(d))fatal_error("Non-finite NSDate interval unsupported");return d;}
static uint64_t encoded(double d){uint64_t raw;memcpy(&raw,&d,8);return raw;}
static Date *create_date(double reference_seconds){
 if(!isfinite(reference_seconds))fatal_error("NSDate interval overflow");
 Date *d=im2_objc_alloc(date_class,NULL);if(!d)return NULL;d->reference_seconds=reference_seconds;
 if(!im2_objc_autorelease(d)){im2_objc_release(d);return NULL;}return d;
}
static void check_factory(void *cls){if(cls!=date_class)fatal_error("Unsupported NSDate factory subclass");}
static uintptr_t date_now(void *cls,const char *sel){(void)sel;check_factory(cls);return (uintptr_t)create_date(current_reference());}
static uintptr_t date_unix(void *cls,const char *sel,uint64_t raw){(void)sel;check_factory(cls);return (uintptr_t)create_date(decoded(raw)-APPLE_EPOCH);}
static uintptr_t date_ref(void *cls,const char *sel,uint64_t raw){(void)sel;check_factory(cls);return (uintptr_t)create_date(decoded(raw));}
static uintptr_t init_now(void *self,const char *sel){(void)sel;Date *d=date_checked(self);d->reference_seconds=current_reference();return (uintptr_t)self;}
static uintptr_t init_unix(void *self,const char *sel,uint64_t raw){(void)sel;date_checked(self)->reference_seconds=decoded(raw)-APPLE_EPOCH;return (uintptr_t)self;}
static uintptr_t init_ref(void *self,const char *sel,uint64_t raw){(void)sel;double d=decoded(raw);if(!isfinite(d))fatal_error("NSDate interval overflow");date_checked(self)->reference_seconds=d;return (uintptr_t)self;}
static uint64_t since_unix(void *self,const char *sel){(void)sel;return encoded(date_checked(self)->reference_seconds+APPLE_EPOCH);}
static uint64_t since_ref(void *self,const char *sel){(void)sel;return encoded(date_checked(self)->reference_seconds);}
static uint64_t class_since_ref(void *cls,const char *sel){(void)sel;check_factory(cls);return encoded(current_reference());}
static uint64_t since_date(void *self,const char *sel,void *other){(void)sel;return encoded(date_checked(self)->reference_seconds-date_checked(other)->reference_seconds);}
static int32_t compare(void *self,const char *sel,void *other){(void)sel;double a=date_checked(self)->reference_seconds,b=date_checked(other)->reference_seconds;return a<b?-1:a>b?1:0;}
static uint32_t equal(void *self,const char *sel,void *other){(void)sel;return date_class && im2_objc_object_class(other)==date_class && date_checked(self)->reference_seconds==date_checked(other)->reference_seconds;}
static uintptr_t copy_date(void *self,const char *sel){(void)sel;date_checked(self);return (uintptr_t)im2_objc_retain(self);}
#define M(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_dates_init(Im2ObjcClass *root){
 if(!root || date_class)return -1;
 const Im2MethodSpec cm[]={M("date","@@:",date_now),M("dateWithTimeIntervalSince1970:","@@:d",date_unix),M("dateWithTimeIntervalSinceReferenceDate:","@@:d",date_ref),M("timeIntervalSinceReferenceDate","d@:",class_since_ref)};
 const Im2MethodSpec im[]={M("init","@@:",init_now),M("initWithTimeIntervalSince1970:","@@:d",init_unix),M("initWithTimeIntervalSinceReferenceDate:","@@:d",init_ref),M("timeIntervalSince1970","d@:",since_unix),M("timeIntervalSinceReferenceDate","d@:",since_ref),M("timeIntervalSinceDate:","d@:@",since_date),M("compare:","i@:@",compare),M("isEqualToDate:","c@:@",equal),M("isEqual:","c@:@",equal),M("copy","@@:",copy_date)};
 date_class=im2_objc_register_class("NSDate",root,sizeof(Date),im,sizeof(im)/sizeof(*im),cm,sizeof(cm)/sizeof(*cm));return date_class?0:-1;
}
const import_entry time_bridge_imports[]={F(CFAbsoluteTimeGetCurrent,cf_now),F(time,time_now),F(difftime,time_difference),F(gmtime,utc)};
const int time_bridge_imports_count=sizeof(time_bridge_imports)/sizeof(time_bridge_imports[0]);
