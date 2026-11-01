/* SPDX-License-Identifier: MIT
 * Minimal, stateful UIKit superclass compatibility for the original iOS binary.
 * Sizes below are derived from the owner's guest subclass instance_start fields.
 * UIKit IVAR offsets, UIKit rendering and event dispatch are NOT reproduced.
 */
#include "uikit_hierarchy.h"
#include <pthread.h>
#include <math.h>
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
    void *layer; /* strong lifetime, +1 from provider */
    void *background_color;
    void *parent; /* non-owning; parent retains each child through UiChild */
    UiChild *children;
    int hidden,tag,frame_valid,bounds_valid,needs_layout,needs_display;
    int interaction_enabled,multiple_touch_enabled,exclusive_touch,opaque;
    struct UiState *next;
} UiState;

static Im2ObjcClass *responder,*view,*control,*button,*image_view,
    *view_controller,*navigation_controller,*window_class;
static Im2ObjcClass *color_class;
typedef struct {Im2ObjcObject object;float rgba[4];} Color;
int im2_uikit_color_rgba(const void *color,float out[4]) {
    if(!out || !color_class || im2_objc_object_class(color)!=color_class)return -1;
    memcpy(out,((const Color *)color)->rgba,4*sizeof(float));return 0;
}
static void *palette_color(void *cls,const char *selector) {
    if(cls!=color_class)return NULL;
    Color *color=im2_objc_alloc(color_class,NULL);if(!color)return NULL;
    float value=!strcmp(selector,"whiteColor")?1.f:0.f;
    color->rgba[0]=color->rgba[1]=color->rgba[2]=value;
    color->rgba[3]=!strcmp(selector,"clearColor")?0.f:1.f;
    if(!im2_objc_autorelease(color)){im2_objc_release(color);return NULL;}
    return color;
}
static UiState *states;
static pthread_mutex_t state_lock=PTHREAD_MUTEX_INITIALIZER;
static void (*failure)(const char *);
static int (*activate_window)(void *);
static void *(*make_layer)(void *);

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
        if(s){s->owner=owner;s->interaction_enabled=1;s->opaque=1;s->next=states;states=s;}
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
        if(s->layer)im2_objc_release(s->layer);
        if(s->background_color)im2_objc_release(s->background_color);
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
static void view_set_background(void *self,const char *selector,void *color) {
    (void)selector;float rgba[4];
    if(color && im2_uikit_color_rgba(color,rgba))fail("Unsupported UIView background color");
    if(color && !im2_objc_retain(color))fail("Unknown UIView background color");
    UiState *state=state_for(self,1);void *old=state->background_color;
    state->background_color=color;state->needs_display=1;
    if(old)im2_objc_release(old);
}
static void *view_background(void *self,const char *selector) {
    (void)selector;UiState *state=state_for(self,0);return state?state->background_color:NULL;
}
/* Use a sidecar: never reinterpret unverified guest UIKit ivar bytes. */
static IM2_UIKIT_PCS uintptr_t view_init_frame(void *self,const char *selector,Im2Rect32 frame) {
    (void)selector;
    if(!kind_of(self,view))fail("initWithFrame: receiver is not UIView");
    UiState *s=state_for(self,1);
    s->frame=frame;s->frame_valid=1;
    s->bounds=(Im2Rect32){0,0,frame.width,frame.height};s->bounds_valid=1;
    if(kind_of(self,window_class))s->hidden=1;
    return (uintptr_t)self;
}
static IM2_UIKIT_PCS void view_set_frame(void *self,const char *selector,Im2Rect32 frame) {
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
static IM2_UIKIT_PCS void view_set_bounds(void *self,const char *selector,Im2Rect32 bounds) {
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
int im2_uikit_bind_layer_factory(void *(*factory)(void *)) {
    if(!view || !factory || make_layer)return -1;
    make_layer=factory;return 0;
}
static uintptr_t view_layer(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("UIView layer receiver invalid");
    if(!make_layer)fail("UIView layer creation requires registered CAEAGLLayer backend");
    UiState *s=state_for(self,1);
    if(!s->layer) {
        void *layer=make_layer(self);
        if(!layer || !im2_objc_retain_count(layer))
            fail("UIView layer provider did not create a valid owned layer");
        s->layer=layer;
    }
    return (uintptr_t)s->layer;
}
static void view_set_hidden(void *self,const char *selector,int hidden) {
    (void)selector;
    if(!kind_of(self,view))fail("setHidden: receiver is not UIView");
    state_for(self,1)->hidden=(uint8_t)hidden!=0;
}
static uint32_t view_hidden(void *self,const char *selector) {
    (void)selector;
    if(!kind_of(self,view))fail("isHidden receiver is not UIView");
    UiState *s=state_for(self,0);
    return s?(uint32_t)s->hidden:(uint32_t)kind_of(self,window_class);
}
static void view_interaction(void *self,const char *sel,uint32_t enabled){(void)sel;if(!kind_of(self,view))fail("Not a UIView");state_for(self,1)->interaction_enabled=(uint8_t)enabled!=0;}
static uint32_t view_interaction_get(void *self,const char *sel){(void)sel;UiState *s=state_for(self,0);return s?s->interaction_enabled:1;}
static void view_multiple(void *self,const char *sel,uint32_t enabled){(void)sel;if(!kind_of(self,view))fail("Not a UIView");state_for(self,1)->multiple_touch_enabled=(uint8_t)enabled!=0;}
static uint32_t view_multiple_get(void *self,const char *sel){(void)sel;UiState *s=state_for(self,0);return s?s->multiple_touch_enabled:0;}
static void view_exclusive(void *self,const char *sel,uint32_t enabled){(void)sel;if(!kind_of(self,view))fail("Not a UIView");state_for(self,1)->exclusive_touch=(uint8_t)enabled!=0;}
static uint32_t view_exclusive_get(void *self,const char *sel){(void)sel;UiState *s=state_for(self,0);return s?s->exclusive_touch:0;}
static void view_opaque(void *self,const char *sel,uint32_t enabled){(void)sel;if(!kind_of(self,view))fail("Not a UIView");UiState *s=state_for(self,1);s->opaque=(uint8_t)enabled!=0;s->needs_display=1;}
static uint32_t view_opaque_get(void *self,const char *sel){(void)sel;UiState *s=state_for(self,0);return s?s->opaque:1;}
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
    const Im2MethodSpec colors[]={METHOD("blackColor","@@:",palette_color),
        METHOD("whiteColor","@@:",palette_color),METHOD("clearColor","@@:",palette_color)};
    color_class=im2_objc_register_class("UIColor",nsobject,sizeof(Color),NULL,0,colors,3);
    if(!color_class){(void)im2_objc_registration_rollback(checkpoint);responder=NULL;return -1;}
    const Im2MethodSpec vm[]={
        METHOD("initWithFrame:","@@:{CGRect={CGPoint=ff}{CGSize=ff}}",view_init_frame),
        METHOD("setFrame:","v@:{CGRect={CGPoint=ff}{CGSize=ff}}",view_set_frame),
        {"frame","{CGRect={CGPoint=ff}{CGSize=ff}}@:",(Im2Imp)view_get_frame,sizeof(Im2Rect32)},
        {"bounds","{CGRect={CGPoint=ff}{CGSize=ff}}@:",(Im2Imp)view_get_bounds,sizeof(Im2Rect32)},
        METHOD("setBounds:","v@:{CGRect={CGPoint=ff}{CGSize=ff}}",view_set_bounds),
        METHOD("layer","@@:",view_layer),
        METHOD("setBackgroundColor:","v@:@",view_set_background),
        METHOD("backgroundColor","@@:",view_background),
        METHOD("setNeedsLayout","v@:",view_set_needs_layout),
        METHOD("setNeedsDisplay","v@:",view_set_needs_display),
        METHOD("setHidden:","v@:c",view_set_hidden),
        METHOD("isHidden","c@:",view_hidden),
        METHOD("setUserInteractionEnabled:","v@:c",view_interaction),
        METHOD("isUserInteractionEnabled","c@:",view_interaction_get),
        METHOD("setMultipleTouchEnabled:","v@:c",view_multiple),
        METHOD("isMultipleTouchEnabled","c@:",view_multiple_get),
        METHOD("setExclusiveTouch:","v@:c",view_exclusive),
        METHOD("isExclusiveTouch","c@:",view_exclusive_get),
        METHOD("setOpaque:","v@:c",view_opaque),
        METHOD("isOpaque","c@:",view_opaque_get),
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
            navigation_controller=window_class=color_class=NULL;
        return -1;
    }
    return 0;
}

void *im2_uikit_view_parent(void *self){
    if(!kind_of(self,view))return NULL;
    UiState *s=state_for(self,0);return s?s->parent:NULL;
}
int im2_uikit_view_origin(void *self,void *window,Im2Point32 *out){
    if(!out || !kind_of(self,view) || !kind_of(window,window_class))return -1;
    Im2Point32 point={0};void *cursor=self;
    for(size_t depth=0;cursor && depth<128;depth++){
        if(cursor==window){*out=point;return 0;}
        UiState *s=state_for(cursor,0);
        if(!s || !s->frame_valid || !s->bounds_valid || !isfinite(s->frame.x) || !isfinite(s->frame.y) || !isfinite(s->bounds.x) || !isfinite(s->bounds.y))return -1;
        point.x+=s->frame.x-s->bounds.x;point.y+=s->frame.y-s->bounds.y;cursor=s->parent;
    }return -1;
}
int im2_uikit_view_visible(void *self,void *window){
    if(!kind_of(self,view) || !kind_of(window,window_class))return 0;
    for(size_t depth=0;self && depth<128;depth++){
        UiState *s=state_for(self,0);if(!s || s->hidden || !s->interaction_enabled)return 0;
        if(self==window)return 1;
        self=s->parent;
    }return 0;
}
static void *hit_view(void *self,Im2Point32 point,unsigned depth){
    if(depth>128)return NULL;
    UiState *s=state_for(self,0);
    if(!s || s->hidden || !s->interaction_enabled || !s->bounds_valid || !isfinite(s->bounds.x) || !isfinite(s->bounds.y) || !isfinite(s->bounds.width) || !isfinite(s->bounds.height) || s->bounds.width<=0 || s->bounds.height<=0 || point.x<s->bounds.x || point.y<s->bounds.y || point.x>=s->bounds.x+s->bounds.width || point.y>=s->bounds.y+s->bounds.height)return NULL;
    /* addSubview appends in z order; the last matching child wins. */
    void *hit=NULL;
    for(UiChild *c=s->children;c;c=c->next){UiState *child=state_for(c->view,0);if(!child || !child->frame_valid || !child->bounds_valid)continue;
        Im2Point32 local={point.x-child->frame.x+child->bounds.x,point.y-child->frame.y+child->bounds.y};
        if(!isfinite(local.x) || !isfinite(local.y))continue;
        void *candidate=hit_view(c->view,local,depth+1);if(candidate)hit=candidate;
    }return hit?hit:self;
}
void *im2_uikit_hit_test(void *window,Im2Point32 point){
    return kind_of(window,window_class) && isfinite(point.x) && isfinite(point.y)?hit_view(window,point,0):NULL;
}

int im2_uikit_view_accepts_multiple(void *self){return kind_of(self,view)?(int)view_multiple_get(self,NULL):0;}
int im2_uikit_view_exclusive(void *self){return kind_of(self,view)?(int)view_exclusive_get(self,NULL):0;}
