/* SPDX-License-Identifier: MIT. Native UIKit event chain with deterministic sample fixtures. */
#define _POSIX_C_SOURCE 200809L
#include "touches.h"
#include "sets.h"
#include "numbers.h"
#include "collections.h"
#include "foundation.h"
#include "uikit_application.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
static void *root_window,*game_view,*held_touch,*temporary,*kept_event;
static Im2Point32 latest,previous;
static unsigned calls,counts[5],last_count,last_all,guest_calls;
static uint32_t last_phase;
static uint64_t tick=1000000;
static Im2ObjcTarget target(void *o,const char *sel){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,sel,NULL,&t));return t;}
static Im2Imp method(void *o,const char *sel){Im2ObjcTarget t=target(o,sel);assert(!t.guest && !t.stret_size);return t.imp;}
static uintptr_t get(void *o,const char *sel){return ((uintptr_t(*)(void *,const char *))method(o,sel))(o,sel);}
static uint32_t word(void *o,const char *sel){return ((uint32_t(*)(void *,const char *))method(o,sel))(o,sel);}
static void send0(void *o,const char *sel){((void(*)(void *,const char *))method(o,sel))(o,sel);}
static uintptr_t send1(void *o,const char *sel,void *arg){return ((uintptr_t(*)(void *,const char *,void *))method(o,sel))(o,sel,arg);}
static void flag(void *o,const char *sel,uint32_t value){((void(*)(void *,const char *,uint32_t))method(o,sel))(o,sel,value);}
static void add(void *o,void *child){((void(*)(void *,const char *,void *))method(o,"addSubview:"))(o,"addSubview:",child);}
static void frame_init(void *o,Im2Rect32 r){assert(((uintptr_t(*)(void *,const char *,Im2Rect32))method(o,"initWithFrame:"))(o,"initWithFrame:",r)==(uintptr_t)o);}
static void destroy(void *o){send0(o,"dealloc");}
static Im2Point32 point(void *touch,const char *sel,void *view){Im2Point32 p={NAN,NAN};Im2ObjcTarget t=target(touch,sel);assert(!t.guest && t.stret_size==8);((void(*)(Im2Point32 *,void *,const char *,void *))t.imp)(&p,touch,sel,view);return p;}
static void callback(void *self,const char *sel,void *changed,void *event){
 assert(self==game_view);calls++;
 if(strstr(sel,"Began"))last_phase=IM2_TOUCH_BEGAN;else if(strstr(sel,"Moved"))last_phase=IM2_TOUCH_MOVED;else if(strstr(sel,"Ended"))last_phase=IM2_TOUCH_ENDED;else last_phase=IM2_TOUCH_CANCELLED;
 counts[last_phase]++;last_count=(unsigned)get(changed,"count");void *all=(void *)get(event,"allTouches");last_all=(unsigned)get(all,"count");assert(last_count && last_all>=last_count);
 Im2FastEnumerationState state={0};void *buffer[16];const char *enum_sel="countByEnumeratingWithState:objects:count:";
 size_t n=((uintptr_t(*)(void *,const char *,Im2FastEnumerationState *,void **,size_t))method(changed,enum_sel))(changed,enum_sel,&state,buffer,16);assert(n==last_count && state.mutations && !*state.mutations);
 for(size_t i=0;i<n;i++){void *t=buffer[i];assert(word(t,"phase")==last_phase && word(t,"tapCount")==1 && get(t,"view")== (uintptr_t)self && get(t,"window")== (uintptr_t)root_window);assert(send1(all,"member:",t)==(uintptr_t)t);}
 void *touch=buffer[0];
 uint64_t time_bits=((uint64_t(*)(void *,const char *))method(touch,"timestamp"))(touch,"timestamp");
 uint64_t event_bits=((uint64_t(*)(void *,const char *))method(event,"timestamp"))(event,"timestamp");
 double seconds;memcpy(&seconds,&time_bits,8);assert(time_bits==event_bits && fabs(seconds-tick/1000000.)<1e-9);
 latest=point(touch,"locationInView:",self);previous=point(touch,"previousLocationInView:",self);
 if(!held_touch && last_phase==IM2_TOUCH_BEGAN)held_touch=im2_objc_retain(touch);
 temporary=im2_string_create("input callback temporary",1);assert(temporary);
 if(!kept_event)kept_event=im2_objc_retain(event);
 size_t nested;assert(im2_touches_frame(NULL,0,tick,root_window,&nested)==-1);
}
static void never_native(void){assert(!"guest touch method must cross the ARM bridge");}
static uint32_t bridge(void *imp,uint32_t self,uint32_t sel,uint32_t touches,uint32_t event){assert(imp==(void *)never_native && self && sel && touches && event);guest_calls++;return 0;}
static size_t samples(const Im2TouchSample *points,size_t count){size_t n;tick+=1000;assert(!im2_touches_frame(points,count,tick,root_window,&n));assert(!temporary || !im2_objc_retain_count(temporary));return n;}
static void release_held(void){if(held_touch){assert(!im2_objc_release(held_touch));held_touch=NULL;}if(kept_event){assert(!im2_objc_release(kept_event));kept_event=NULL;}}
static void *worker(void *p){(void)p;size_t n;assert(im2_touches_frame(NULL,0,tick,root_window,&n)==-1 && im2_touches_shutdown()==-1);return NULL;}
int main(void){
 Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root);
 assert(im2_objc_register_class("NSAutoreleasePool",root,sizeof(Im2ObjcObject),NULL,0,NULL,0));
 Im2FoundationConfig cfg={"/tmp",NULL,NULL,NULL};assert(!im2_foundation_init(root,&cfg) && !im2_collections_init(root,NULL) && !im2_numbers_init(root) && !im2_sets_init(root));
 assert(!im2_uikit_hierarchy_init(root,NULL) && !im2_uikit_application_init(root,NULL));assert(!im2_touches_init(root,bridge) && im2_touches_init(root,bridge));
 const Im2MethodSpec methods[]={{"touchesBegan:withEvent:","v16@0:4@8@12",(Im2Imp)callback,0},{"touchesMoved:withEvent:","v16@0:4@8@12",(Im2Imp)callback,0},{"touchesEnded:withEvent:","v16@0:4@8@12",(Im2Imp)callback,0},{"touchesCancelled:withEvent:","v16@0:4@8@12",(Im2Imp)callback,0}};
 Im2ObjcClass *view=im2_objc_register_class("FixtureGameView",im2_objc_class_named("UIView"),36,methods,4,NULL,0);assert(view);
 root_window=im2_objc_alloc(im2_objc_class_named("UIWindow"),destroy);game_view=im2_objc_alloc(view,destroy);assert(root_window && game_view);
 frame_init(root_window,(Im2Rect32){0,0,320,480});frame_init(game_view,(Im2Rect32){20,30,280,400});add(root_window,game_view);send0(root_window,"makeKeyAndVisible");assert(im2_uikit_key_window()==root_window);
 void *app=(void *)get(im2_objc_class_named("UIApplication"),"sharedApplication");flag(app,"setIdleTimerDisabled:",1);assert(im2_uikit_idle_timer_disabled());flag(app,"setIdleTimerDisabled:",0x100);assert(!im2_uikit_idle_timer_disabled());
 Im2Point32 projected;assert(im2_touches_project(479.5f,271.5f,&projected) && projected.x==160 && projected.y==240);assert(!im2_touches_project(0,10,&projected) && !im2_touches_project(NAN,10,&projected));
 Im2TouchSample one={42,479.5f,271.5f};assert(samples(&one,1)==1 && last_phase==IM2_TOUCH_BEGAN && last_count==1 && latest.x==140 && latest.y==210 && previous.x==140);
 void *original=held_touch;assert(samples(&one,1)==0 && word(original,"phase")==IM2_TOUCH_STATIONARY);
 one.x+=10;one.y+=10;assert(samples(&one,1)==1 && last_phase==IM2_TOUCH_MOVED && held_touch==original && previous.x==140 && previous.y==210);
 one.x=80;one.y=530;assert(samples(&one,1)==1 && (latest.x<0 || latest.y<0)); /* Captured view continues outside its bounds. */
 assert(samples(NULL,0)==1 && last_phase==IM2_TOUCH_ENDED && word(original,"phase")==IM2_TOUCH_ENDED);release_held();assert(!im2_objc_retain_count(original));
 Im2TouchSample bar={7,0,100};assert(!samples(&bar,1));bar.x=479.5f;bar.y=271.5f;assert(!samples(&bar,1) && !samples(NULL,0)); /* No phantom begin on entering from letterbox. */
 Im2TouchSample two[]={{1,479.5f,271.5f},{2,500,290}};
 assert(samples(two,2)==1 && last_count==1);assert(samples(NULL,0)==1);release_held(); /* Default single-touch policy. */
 flag(game_view,"setMultipleTouchEnabled:",1);assert(im2_uikit_view_accepts_multiple(game_view));
 assert(samples(two,2)==1 && last_count==2 && last_all==2);
 size_t n;Im2TouchSample duplicate[]={{1,400,200},{1,410,210}};assert(im2_touches_frame(duplicate,2,++tick,root_window,&n)==-1);
 assert(im2_touches_frame(two,7,tick,root_window,&n)==-1); /* Bounds check occurs before reading array. */
 flag(game_view,"setHidden:",1);assert(samples(two,2)==1 && last_phase==IM2_TOUCH_CANCELLED && last_count==2);release_held();
 flag(game_view,"setHidden:",0x100);assert(!word(game_view,"isHidden"));assert(!samples(two,2));assert(!samples(NULL,0));
 assert(samples(two,2)==1);tick+=1000;assert(!im2_touches_cancel(tick,&n) && n==1 && last_phase==IM2_TOUCH_CANCELLED);release_held();
 /* Hardware-ID replacement can end six old contacts and begin six new ones in one turn. */
 Im2TouchSample six[6];for(int i=0;i<6;i++)six[i]=(Im2TouchSample){i,450.f+i*4,250};assert(samples(six,6)==1 && last_count==6);
 for(int i=0;i<6;i++)six[i].id+=10;
 unsigned ends=counts[IM2_TOUCH_ENDED];assert(samples(six,6)==2 && counts[IM2_TOUCH_ENDED]==ends+1 && last_phase==IM2_TOUCH_BEGAN && last_all==12);
 assert(samples(NULL,0)==1);release_held();
 /* A real guest class identity must resolve the original callback bridge. */
 send0(game_view,"removeFromSuperview");assert(!im2_objc_release(game_view));
 Im2MethodSpec gm[4];for(int i=0;i<4;i++){gm[i]=methods[i];gm[i].imp=(Im2Imp)never_native;}
 Im2ObjcClass *guest=im2_objc_register_foreign_class("EAGLView",(void *)0x26000000,(void *)0x26000020,im2_objc_class_named("UIView"),128,gm,4,NULL,0);assert(guest);
 game_view=im2_objc_alloc(guest,destroy);frame_init(game_view,(Im2Rect32){0,0,320,480});add(root_window,game_view);
 assert(samples(&one,1)==1 && guest_calls==1);assert(samples(NULL,0)==1 && guest_calls==2);
 pthread_t th;assert(!pthread_create(&th,NULL,worker,NULL) && !pthread_join(th,NULL));
 assert(im2_touches_frame(NULL,0,tick-1,root_window,&n)==-1);
 assert(!im2_touches_shutdown());assert(!im2_objc_release(game_view));assert(!im2_uikit_application_shutdown());assert(!im2_objc_release(root_window));
 puts("PASS: original-style native/guest touch callbacks, CGPoint stret, identity/phases/previous location, grouped multitouch, capture/cancel, letterbox policy, bounds/thread guards and owned event lifetimes");
}
