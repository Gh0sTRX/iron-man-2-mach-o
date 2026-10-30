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
    Im2Rect32 bounds;
    void *controller_view;
    void *parent; /* non-owning; parent retains each child through UiChild */
    UiChild *children;
    int hidden,tag,frame_valid,bounds_valid,needs_layout,needs_display;
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
    s->bounds=(Im2Rect32){0,0,frame.width,frame.height};s->bounds_valid=1;
    if(kind_of(self,window_class))s->hidden=1;
    return (uintptr_t)self;
}
static void view_set_frame(void *self,const char *selector,Im2Rect32 frame) {
    (void)selector;
    if(!kind_of(self,view))fail("setFrame: receiver is not UIView");
    UiState *s=state_for(self,1);s->frame=frame;s->frame_valid=1;
    if(!s->bounds_valid){s->bounds=(Im2Rect32){0,0,frame.width,frame.height};s->bounds_valid=1;}
    else{s->bounds.width=frame.width;s->bounds.height=frame.height;}
    s->needs_layout=1;
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
    if(!s || !s->bounds_valid)fail("UIView bounds requested before initialization");
    *out=s->bounds;
}
static void view_set_bounds(void *self,const char *selector,Im2Rect32 bounds) {
    (void)selector;
    if(!kind_of(self,view))fail("setBounds: requires UIView");
    UiState *s=state_for(self,1);
    s->bounds=bounds;s->bounds_valid=1;s->needs_layout=1;
}
static void view_set_needs_layout(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("setNeedsLayout requires UIView");
    state_for(self,1)->needs_layout=1;
}
static void view_set_needs_display(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("setNeedsDisplay requires UIView");
    state_for(self,1)->needs_display=1;
}
/* A future Vita UI renderer can inspect these flags, but only its real
 * layout/draw phase may clear them. This code intentionally does not. */
int im2_uikit_view_invalidation(const void *self,int *layout,int *display) {
    if(!layout || !display || !kind_of(self,view))return -1;
    UiState *s=state_for((void *)self,0);
    *layout=s?s->needs_layout:0;
    *display=s?s->needs_display:0;
    return 0;
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
static void view_remove_super(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("removeFromSuperview receiver is not UIView");
    UiState *child=state_for(self,0);
    if(!child || !child->parent)return;
    void *parent=child->parent;
    UiState *ps=state_for(parent,0);
    if(!ps)fail("UIView parent sidecar unavailable");
    UiChild **link=&ps->children;
    while(*link && (*link)->view!=self)link=&(*link)->next;
    if(!*link)fail("UIView parent/child ownership mismatch");
    UiChild *node=*link;
    *link=node->next;
    child->parent=NULL;
    free(node);
    /* Removing a subview releases exactly the parent's one ownership. */
    if(im2_objc_release(self))fail("UIView child release failed");
}
static void view_add_subview(void *self,const char *selector,void *child) {
    (void)selector;
    if(!kind_of(self,view) || !kind_of(child,view))
        fail("addSubview requires two registered UIViews");
    if(self==child)fail("UIView cannot add itself as a subview");
    /* Fail closed on cycles, including reparenting an ancestor. */
    void *ancestor=self;
    for(size_t depth=0;ancestor;depth++) {
        if(depth>=128)fail("UIView ancestry depth exceeded");
        if(ancestor==child)fail("UIView hierarchy cycle rejected");
        UiState *state=state_for(ancestor,0);
        ancestor=state?state->parent:NULL;
    }
    UiState *cs=state_for(child,1);
    if(cs->parent==self)return; /* No duplicate strong references. */
    void *held=im2_objc_retain(child);
    if(!held)fail("Unable to retain UIView child");
    if(cs->parent)view_remove_super(child,"removeFromSuperview");
    UiChild *entry=calloc(1,sizeof(*entry));
    if(!entry) {
        im2_objc_release(held);
        fail("Unable to allocate UIView subview link");
    }
    entry->view=child;
    UiState *parent=state_for(self,1);
    UiChild **tail=&parent->children;
    while(*tail)tail=&(*tail)->next;
    *tail=entry;
    cs->parent=self;
}
static uintptr_t view_superview(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("superview receiver is not UIView");
    UiState *s=state_for(self,0);
    return (uintptr_t)(s?s->parent:NULL);
}
static uintptr_t view_window(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("window receiver is not UIView");
    void *cursor=self;
    for(size_t depth=0;cursor;depth++) {
        if(depth>=128)fail("UIView window ancestry cycle");
        if(kind_of(cursor,window_class))return (uintptr_t)cursor;
        UiState *s=state_for(cursor,0);
        cursor=s?s->parent:NULL;
    }
    return 0;
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
        METHOD("setBounds:","v@:{CGRect={CGPoint=ff}{CGSize=ff}}",view_set_bounds),
        METHOD("setNeedsLayout","v@:",view_set_needs_layout),
        METHOD("setNeedsDisplay","v@:",view_set_needs_display),
        METHOD("setHidden:","v@:c",view_set_hidden),
        METHOD("isHidden","c@:",view_hidden),
        METHOD("setTag:","v@:i",view_set_tag),
        METHOD("tag","i@:",view_tag),
        METHOD("addSubview:","v@:@",view_add_subview),
        METHOD("removeFromSuperview","v@:",view_remove_super),
        METHOD("superview","@@:",view_superview),
        METHOD("window","@@:",view_window),
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
