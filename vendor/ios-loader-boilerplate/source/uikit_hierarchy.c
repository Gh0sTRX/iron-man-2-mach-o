/* SPDX-License-Identifier: MIT
 * Minimal, stateful UIKit superclass compatibility for the original iOS binary.
 * Sizes below are derived from the owner's guest subclass instance_start fields.
 * UIKit IVAR offsets, UIKit rendering and event dispatch are NOT reproduced.
 */
#include "uikit_hierarchy.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(Im2Rect32)==16,"legacy iOS ARM32 CGRect must be 16 bytes");
#endif

typedef struct UiChild {
    void *view;
    struct UiChild *next;
} UiChild;
typedef struct UiState {
    void *owner;
    Im2Rect32 frame;
    void *controller_view;
    void *parent; /* non-owning; parent retains each child through UiChild */
    UiChild *children;
    int hidden,tag,frame_valid;
    struct UiState *next;
} UiState;

static Im2ObjcClass *responder,*view,*control,*button,*image_view,
    *view_controller,*navigation_controller,*window_class;
static UiState *states;
static pthread_mutex_t state_lock=PTHREAD_MUTEX_INITIALIZER;
static void (*failure)(const char *);
static int (*activate_window)(void *);

static void fail(const char *why) {
    if(failure)failure(why);
    abort(); /* Never turn an unsupported UIKit operation into fake success. */
}
static UiState *state_for(void *owner,int create) {
    if(!owner || !im2_objc_object_class(owner))fail("Unregistered UIKit receiver");
    pthread_mutex_lock(&state_lock);
    UiState *s=states;
    while(s && s->owner!=owner)s=s->next;
    if(!s && create) {
        s=calloc(1,sizeof(*s));
        if(s){s->owner=owner;s->next=states;states=s;}
    }
    pthread_mutex_unlock(&state_lock);
    if(!s && create)fail("UIKit state allocation failed");
    return s;
}
static void discard_state(void *owner) {
    pthread_mutex_lock(&state_lock);
    UiState **p=&states;
    while(*p && (*p)->owner!=owner)p=&(*p)->next;
    UiState *s=*p;
    if(s)*p=s->next;
    pthread_mutex_unlock(&state_lock);
    if(s) {
        /* Unlink all children before releasing them: their final release may
         * recursively invoke -dealloc and re-enter this sidecar registry. */
        UiChild *child=s->children;
        while(child) {
            UiChild *next=child->next;
            UiState *cs=state_for(child->view,0);
            if(cs && cs->parent==owner)cs->parent=NULL;
            im2_objc_release(child->view);
            free(child);
            child=next;
        }
        if(s->controller_view)im2_objc_release(s->controller_view);
        free(s);
    }
}
static int kind_of(const void *o,Im2ObjcClass *klass) {
    for(Im2ObjcClass *c=im2_objc_object_class(o);c;) {
        if(c==klass)return 1;
        const Im2ObjcClass *m=im2_objc_class_metadata(c);
        if(!m)return 0;
        c=m->superclass;
    }
    return 0;
}
/* Use a sidecar: never reinterpret unverified guest UIKit ivar bytes. */
static uintptr_t view_init_frame(void *self,const char *selector,Im2Rect32 frame) {
    (void)selector;
    if(!kind_of(self,view))fail("initWithFrame: receiver is not UIView");
    UiState *s=state_for(self,1);
    s->frame=frame;s->frame_valid=1;
    if(kind_of(self,window_class))s->hidden=1;
    return (uintptr_t)self;
}
static void view_set_frame(void *self,const char *selector,Im2Rect32 frame) {
    (void)selector;
    if(!kind_of(self,view))fail("setFrame: receiver is not UIView");
    UiState *s=state_for(self,1);s->frame=frame;s->frame_valid=1;
}
int im2_uikit_view_frame(const void *receiver,Im2Rect32 *out) {
    if(!out || !kind_of(receiver,view))return -1;
    UiState *s=state_for((void *)receiver,0);
    if(!s || !s->frame_valid)return -1;
    *out=s->frame;return 0;
}
/* CGRect methods use the ARM32 objc_msgSend_stret hidden return pointer.
 * The guest frame and bounds are distinct: bounds has a local (0,0) origin. */
static void view_get_frame(Im2Rect32 *out,void *self,const char *selector) {
    (void)selector;
    if(!out || !kind_of(self,view))fail("UIView frame receiver invalid");
    UiState *s=state_for(self,0);
    if(!s || !s->frame_valid)fail("UIView frame requested before initialization");
    *out=s->frame;
}
static void view_get_bounds(Im2Rect32 *out,void *self,const char *selector) {
    (void)selector;
    if(!out || !kind_of(self,view))fail("UIView bounds receiver invalid");
    UiState *s=state_for(self,0);
    if(!s || !s->frame_valid)fail("UIView bounds requested before initialization");
    *out=(Im2Rect32){0.f,0.f,s->frame.width,s->frame.height};
}
static void view_set_hidden(void *self,const char *selector,int hidden) {
    (void)selector;
    if(!kind_of(self,view))fail("setHidden: receiver is not UIView");
    state_for(self,1)->hidden=!!hidden;
}
static uint32_t view_hidden(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("isHidden receiver is not UIView");
    UiState *s=state_for(self,0);
    return s?(uint32_t)s->hidden:(uint32_t)kind_of(self,window_class);
}
static void view_set_tag(void *self,const char *selector,int tag) {
    (void)selector;
    if(!kind_of(self,view))fail("setTag: receiver is not UIView");
    state_for(self,1)->tag=tag;
}
static int view_tag(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("tag receiver is not UIView");
    UiState *s=state_for(self,0);
    return s?s->tag:0;
}
static void view_dealloc(void *self,const char *selector) {
    (void)selector;discard_state(self);
}
int im2_uikit_bind_window_activation(int (*callback)(void *)) {
    if(!window_class || !callback || activate_window)return -1;
    activate_window=callback;return 0;
}
static void window_make_key_and_visible(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,window_class))fail("makeKeyAndVisible requires UIWindow");
    if(!activate_window || activate_window(self))
        fail("makeKeyAndVisible cannot activate window without UIApplication");
    view_set_hidden(self,"setHidden:",0);
}
static void controller_set_view(void *self,const char *selector,void *child) {
    (void)selector;
    if(!kind_of(self,view_controller))fail("setView: receiver is not UIViewController");
    if(child && !kind_of(child,view))fail("UIViewController setView requires UIView");
    void *held=child?im2_objc_retain(child):NULL;
    if(child && !held)fail("UIViewController could not retain view");
    UiState *s=state_for(self,1);
    void *old=s->controller_view;
    s->controller_view=held;
    if(old)im2_objc_release(old);
}
static uintptr_t controller_get_view(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view_controller))fail("view receiver is not UIViewController");
    UiState *s=state_for(self,0);
    /* Real UIKit invokes loadView when needed. Until it exists, reject. */
    if(!s || !s->controller_view)fail("UIViewController lazy loadView is not implemented");
    return (uintptr_t)s->controller_view;
}
static void controller_dealloc(void *self,const char *selector) {
    (void)selector;discard_state(self);
}
#define METHOD(s,t,f) {s,t,(Im2Imp)(f),0}
static Im2ObjcClass *reg(const char *name,Im2ObjcClass *parent,size_t bytes,
    const Im2MethodSpec *methods,size_t count) {
    return parent?im2_objc_register_class(name,parent,bytes,methods,count,NULL,0):NULL;
}
int im2_uikit_hierarchy_init(Im2ObjcClass *nsobject,void (*on_failure)(const char *)) {
    if(responder || !nsobject || !im2_objc_class_metadata(nsobject))
        return -1;
    size_t checkpoint=im2_objc_registration_checkpoint();
    failure=on_failure;
    responder=reg("UIResponder",nsobject,sizeof(Im2ObjcObject),NULL,0);
    const Im2MethodSpec vm[]={
        METHOD("initWithFrame:","@@:{CGRect={CGPoint=ff}{CGSize=ff}}",view_init_frame),
        METHOD("setFrame:","v@:{CGRect={CGPoint=ff}{CGSize=ff}}",view_set_frame),
        {"frame","{CGRect={CGPoint=ff}{CGSize=ff}}@:",(Im2Imp)view_get_frame,sizeof(Im2Rect32)},
        {"bounds","{CGRect={CGPoint=ff}{CGSize=ff}}@:",(Im2Imp)view_get_bounds,sizeof(Im2Rect32)},
        METHOD("setHidden:","v@:c",view_set_hidden),
        METHOD("isHidden","c@:",view_hidden),
        METHOD("setTag:","v@:i",view_set_tag),
        METHOD("tag","i@:",view_tag),
        METHOD("dealloc","v@:",view_dealloc)
    };
    view=reg("UIView",responder,36,vm,sizeof(vm)/sizeof(*vm));
    control=reg("UIControl",view,36,NULL,0); /* No UIControl ivar ABI claimed. */
    button=reg("UIButton",control,126,NULL,0);
    image_view=reg("UIImageView",view,40,NULL,0);
    const Im2MethodSpec cm[]={
        METHOD("setView:","v@:@",controller_set_view),
        METHOD("view","@@:",controller_get_view),
        METHOD("dealloc","v@:",controller_dealloc)
    };
    view_controller=reg("UIViewController",responder,92,cm,sizeof(cm)/sizeof(*cm));
    navigation_controller=reg("UINavigationController",view_controller,168,NULL,0);
    /* UIWindow's real ivar layout is not reconstructed; it inherits only
     * safe sidecar-backed UIView operations, not display activation. */
    const Im2MethodSpec wm[]={METHOD("makeKeyAndVisible","v@:",window_make_key_and_visible)};
    window_class=reg("UIWindow",view,36,wm,sizeof(wm)/sizeof(*wm));
    if(!window_class || !navigation_controller || !image_view || !button) {
        (void)im2_objc_registration_rollback(checkpoint);
        responder=view=control=button=image_view=view_controller=
            navigation_controller=window_class=NULL;
        return -1;
    }
    return 0;
}
