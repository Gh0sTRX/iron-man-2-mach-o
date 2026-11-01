/* SPDX-License-Identifier: MIT. Single-main-thread touch snapshots and responder delivery.
 * No UIKit affine transforms, controls, gesture recognizers or multi-tap clustering. */
#include "touches.h"
#include "sets.h"
#include "display_geometry.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
typedef struct {Im2ObjcClass *isa;void *view,*window;Im2Point32 point,previous,origin;uint64_t timestamp;uint32_t phase;} Touch;
typedef struct {Im2ObjcClass *isa;void *all;uint64_t timestamp;} Event;
typedef struct {int tracked,id;Touch *touch;} Slot;
static Im2ObjcClass *touch_class,*event_class;
static Im2UIKitGuestCall guest;
static pthread_t thread;
static Slot slots[IM2_TOUCH_LIMIT];
static int dispatching,poisoned,have_time;
static uint64_t last_time;
static int owner(void){return touch_class && pthread_equal(thread,pthread_self());}
static Im2Point32 project(float x,float y){
    /* Pixel centers; UIKit has top-left coordinates while texture V points up. */
    return (Im2Point32){IM2_FB_WIDTH*(1.f-(y+.5f)/IM2_SCREEN_HEIGHT),IM2_FB_HEIGHT*(x+.5f-IM2_PRESENT_X)/IM2_PRESENT_WIDTH};
}
int im2_touches_project(float x,float y,Im2Point32 *p){
 if(!p || !isfinite(x) || !isfinite(y) || x<IM2_PRESENT_X || x>=IM2_PRESENT_X+IM2_PRESENT_WIDTH || y<0 || y>=IM2_SCREEN_HEIGHT)return 0;
 *p=project(x,y);return 1;
}
static void touch_cleanup(void *o){Touch *t=o;if(t->view)im2_objc_release(t->view);if(t->window)im2_objc_release(t->window);t->view=t->window=NULL;}
static void event_cleanup(void *o){Event *e=o;if(e->all)im2_objc_release(e->all);e->all=NULL;}
static uint32_t phase(Touch *t,const char *s){(void)s;return t->phase;}
static uint32_t taps(Touch *t,const char *s){(void)t;(void)s;return 1;}
static uintptr_t view(Touch *t,const char *s){(void)s;return (uintptr_t)t->view;}
static uintptr_t window(Touch *t,const char *s){(void)s;return (uintptr_t)t->window;}
static uint64_t timestamp(Touch *t,const char *s){(void)s;return t->timestamp;}
static void location(Im2Point32 *out,Touch *t,const char *s,void *v){
 (void)s;if(!out)abort();Im2Point32 origin={0};
 if(v==t->view)origin=t->origin;
 else if(v && v!=t->window && im2_uikit_view_origin(v,t->window,&origin))abort();
 *out=(Im2Point32){t->point.x-origin.x,t->point.y-origin.y};
}
static void previous_location(Im2Point32 *out,Touch *t,const char *s,void *v){
 Im2Point32 saved=t->point;t->point=t->previous;location(out,t,s,v);t->point=saved;
}
static uintptr_t all_touches(Event *e,const char *s){(void)s;return (uintptr_t)e->all;}
static uint64_t event_timestamp(Event *e,const char *s){(void)s;return e->timestamp;}
static uint32_t event_type(Event *e,const char *s){(void)e;(void)s;return 0;}
static void touch_dealloc(void *t,const char *s){(void)s;touch_cleanup(t);}
static void event_dealloc(void *e,const char *s){(void)s;event_cleanup(e);}
static const char *digits(const char *p){while(*p>='0' && *p<='9')p++;return p;}
static int callback_type(const Im2ObjcTarget *t){
 if(!t->imp || t->stret_size || !t->types || (t->guest && !guest))return 0;
 const char *p=t->types;if(*p++!='v')return 0;p=digits(p);
 if(*p++!='@')return 0;
 p=digits(p);if(*p++!=':')return 0;
 p=digits(p);
 for(int i=0;i<2;i++){if(*p++!='@')return 0;p=digits(p);}return !*p;
}
static const char *names[]={"touchesBegan:withEvent:","touchesMoved:withEvent:",NULL,"touchesEnded:withEvent:","touchesCancelled:withEvent:"};
static int responder(void *hit,void **out){
 *out=NULL;
 for(size_t depth=0;hit && depth<128;depth++){
  Im2ObjcTarget t;if(!im2_objc_lookup_target(hit,names[0],NULL,&t)){
   for(size_t i=0;i<5;i++)if(names[i] && (im2_objc_lookup_target(hit,names[i],NULL,&t) || !callback_type(&t)))return -1;
   *out=hit;return 0;
  }
  /* UIControl has native action semantics that this subset cannot silently skip. */
  for(Im2ObjcClass *c=im2_objc_object_class(hit);c;){const Im2ObjcClass *m=im2_objc_class_metadata(c);if(!m)return -1;if(!strcmp(m->data->ro->name,"UIControl"))return -1;c=m->superclass;}
  hit=im2_uikit_view_parent(hit);
 }return 0;
}
static int dispatch(void **items,size_t n,Event *event,size_t *callbacks){
 const unsigned order[]={IM2_TOUCH_ENDED,IM2_TOUCH_CANCELLED,IM2_TOUCH_BEGAN,IM2_TOUCH_MOVED};
 for(size_t p=0;p<4;p++)for(size_t i=0;i<n;i++){
  Touch *first=items[i];if(first->phase!=order[p])continue;
  int prior=0;for(size_t j=0;j<i;j++){Touch *t=items[j];if(t->phase==first->phase && t->view==first->view){prior=1;break;}}if(prior)continue;
  void *group[IM2_TOUCH_LIMIT*2];size_t count=0;for(size_t j=i;j<n;j++){Touch *t=items[j];if(t->phase==first->phase && t->view==first->view)group[count++]=t;}
  void *set=im2_set_create(group,count,0);if(!set)return -1;
  const char *sel=im2_objc_sel_register(names[first->phase]);Im2ObjcTarget target;
  if(!sel || im2_objc_lookup_target(first->view,sel,NULL,&target) || !callback_type(&target)){im2_objc_release(set);return -1;}
  if(target.guest)guest((void *)target.imp,(uint32_t)(uintptr_t)first->view,(uint32_t)(uintptr_t)sel,(uint32_t)(uintptr_t)set,(uint32_t)(uintptr_t)event);
  else ((void(*)(void *,const char *,void *,void *))target.imp)(first->view,sel,set,event);
  im2_objc_release(set);(*callbacks)++;
 }return 0;
}
static int frame(const Im2TouchSample *samples,size_t count,uint64_t us,void *window,size_t *callbacks,int cancel){
 if(!owner() || dispatching || poisoned || !callbacks || count>IM2_TOUCH_LIMIT || (count && !samples) || (have_time && us<last_time))return -1;
 for(size_t i=0;i<count;i++){if(samples[i].id<0 || !isfinite(samples[i].x) || !isfinite(samples[i].y) || samples[i].x<0 || samples[i].x>=IM2_SCREEN_WIDTH || samples[i].y<0 || samples[i].y>=IM2_SCREEN_HEIGHT)return -1;for(size_t j=0;j<i;j++)if(samples[j].id==samples[i].id)return -1;}
 *callbacks=0;last_time=us;have_time=1;
 void *pool=im2_objc_alloc(im2_objc_class_named("NSAutoreleasePool"),NULL);if(!pool)return -1;if(im2_objc_pool_push(pool)){im2_objc_release(pool);return -1;}
 double seconds=us/1000000.;uint64_t bits;memcpy(&bits,&seconds,8);
 void *dead[IM2_TOUCH_LIMIT];size_t dead_count=0;void *items[IM2_TOUCH_LIMIT*2];size_t n=0;Event *event=NULL;int result=-1;dispatching=1;
 for(size_t i=0;i<IM2_TOUCH_LIMIT;i++)if(slots[i].tracked){int seen=0;for(size_t j=0;j<count;j++)if(samples[j].id==slots[i].id)seen=1;
  if(!seen || cancel){Touch *t=slots[i].touch;if(t){Im2Point32 origin;int lost=window!=t->window || !im2_uikit_view_visible(t->view,t->window) || im2_uikit_view_origin(t->view,t->window,&origin);
    if(!lost)t->origin=origin;
    t->previous=t->point;t->phase=cancel || lost?IM2_TOUCH_CANCELLED:IM2_TOUCH_ENDED;t->timestamp=bits;dead[dead_count++]=t;}slots[i]=(Slot){0};}
 }
 for(size_t i=0;i<count;i++){
  Slot *slot=NULL;for(size_t j=0;j<IM2_TOUCH_LIMIT;j++)if(slots[j].tracked && slots[j].id==samples[i].id)slot=slots+j;
  if(!slot){for(size_t j=0;j<IM2_TOUCH_LIMIT;j++)if(!slots[j].tracked){slot=slots+j;break;}if(!slot)goto done;*slot=(Slot){1,samples[i].id,NULL};
   Im2Point32 point;if(!im2_touches_project(samples[i].x,samples[i].y,&point))continue;
   void *target=NULL;if(responder(im2_uikit_hit_test(window,point),&target))goto done;if(!target)continue;
   int busy=0,exclusive_block=0;
   for(size_t j=0;j<IM2_TOUCH_LIMIT;j++)if(slots[j].touch){
    Touch *active=slots[j].touch;
    if(active->view==target)busy=1;
    else if(active->window==window && (im2_uikit_view_exclusive(target) || im2_uikit_view_exclusive(active->view)))exclusive_block=1;
   }
   if(exclusive_block)continue;
   if(busy && !im2_uikit_view_accepts_multiple(target))continue;
   Im2Point32 origin;if(im2_uikit_view_origin(target,window,&origin))goto done;
   Touch *t=im2_objc_alloc(touch_class,touch_cleanup);if(!t)goto done;
   t->view=im2_objc_retain(target);t->window=im2_objc_retain(window);if(!t->view || !t->window){im2_objc_release(t);goto done;}
   t->point=t->previous=point;t->origin=origin;t->phase=IM2_TOUCH_BEGAN;t->timestamp=bits;slot->touch=t;
  }else if(slot->touch){Touch *t=slot->touch;Im2Point32 origin;
   if(window!=t->window || !im2_uikit_view_visible(t->view,t->window) || im2_uikit_view_origin(t->view,t->window,&origin)){
    t->phase=IM2_TOUCH_CANCELLED;t->timestamp=bits;dead[dead_count++]=t;slot->touch=NULL;continue;
   }
   t->origin=origin;t->previous=t->point;t->point=project(samples[i].x,samples[i].y);
   t->phase=t->point.x!=t->previous.x || t->point.y!=t->previous.y?IM2_TOUCH_MOVED:IM2_TOUCH_STATIONARY;t->timestamp=bits;
  }
 }
 for(size_t i=0;i<dead_count;i++)items[n++]=dead[i];
 for(size_t i=0;i<IM2_TOUCH_LIMIT;i++)if(slots[i].touch)items[n++]=slots[i].touch;
 if(!n){result=0;goto done;}
 event=im2_objc_alloc(event_class,event_cleanup);if(!event)goto done;event->timestamp=bits;event->all=im2_set_create(items,n,0);if(!event->all)goto done;
 result=dispatch(items,n,event,callbacks);
done:
 if(event)im2_objc_release(event);
 for(size_t i=0;i<dead_count;i++)im2_objc_release(dead[i]);
 if(im2_objc_pool_drain(pool))result=-1;
 dispatching=0;if(result)poisoned=1;return result;
}
int im2_touches_frame(const Im2TouchSample *s,size_t n,uint64_t us,void *w,size_t *callbacks){return frame(s,n,us,w,callbacks,0);}
int im2_touches_cancel(uint64_t us,size_t *callbacks){return frame(NULL,0,us,NULL,callbacks,1);}
int im2_touches_shutdown(void){if(!owner() || dispatching)return -1;for(size_t i=0;i<IM2_TOUCH_LIMIT;i++){if(slots[i].touch)im2_objc_release(slots[i].touch);slots[i]=(Slot){0};}poisoned=1;return 0;}
#define M(s,t,f,n) {s,t,(Im2Imp)(f),n}
int im2_touches_init(Im2ObjcClass *root,Im2UIKitGuestCall bridge){
 const Im2ObjcClass *r=im2_objc_class_metadata(root);if(touch_class || !r || r->data->ro->flags&1 || !im2_objc_class_named("NSSet") || !im2_objc_class_named("NSAutoreleasePool"))return -1;
 _Static_assert(sizeof(Im2Point32)==8,"Darwin CGPoint stret size");
 const Im2MethodSpec tm[]={M("phase","I@:",phase,0),M("tapCount","I@:",taps,0),M("view","@@:",view,0),M("window","@@:",window,0),M("timestamp","d@:",timestamp,0),M("locationInView:","{CGPoint=ff}@:@",location,8),M("previousLocationInView:","{CGPoint=ff}@:@",previous_location,8),M("dealloc","v@:",touch_dealloc,0)};
 const Im2MethodSpec em[]={M("allTouches","@@:",all_touches,0),M("timestamp","d@:",event_timestamp,0),M("type","I@:",event_type,0),M("dealloc","v@:",event_dealloc,0)};
 size_t checkpoint=im2_objc_registration_checkpoint();touch_class=im2_objc_register_class("UITouch",root,sizeof(Touch),tm,sizeof(tm)/sizeof(*tm),NULL,0);
 event_class=touch_class?im2_objc_register_class("UIEvent",root,sizeof(Event),em,sizeof(em)/sizeof(*em),NULL,0):NULL;
 if(!event_class){(void)im2_objc_registration_rollback(checkpoint);touch_class=NULL;return -1;}
 guest=bridge;thread=pthread_self();return 0;
}
