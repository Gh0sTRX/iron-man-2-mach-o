/* SPDX-License-Identifier: MIT.
 * Real native Objective-C identities for an intentionally limited iOS ES1
 * compatibility layer. Actual GPU operations MUST pass a backend capability
 * check; unsupported graphics never report success merely to advance boot.
 */
#include "eagl_bridge.h"
#include "foundation.h"
#include "collections.h"
#include "gles_oes.h"
#include "uikit_hierarchy.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Im2ObjcClass *isa;
    uint32_t api;
    int surface_configured;
} EaglContext;

typedef struct {
    Im2ObjcClass *isa;
    void *properties;
} EaglLayer;

static Im2ObjcClass *calayer_class,*eagl_layer_class,*context_class;
static Im2EaglBackend backend;
static pthread_key_t current_key;
static pthread_once_t key_once=PTHREAD_ONCE_INIT;
static int key_valid;
static void *retained_backing,*color_format_key,*rgb565_value;

static void current_destructor(void *object) {
    if(object)(void)im2_objc_release(object);
}
static void make_current_key(void) {
    key_valid=pthread_key_create(&current_key,current_destructor)==0;
}
static void *get_current(void) {
    pthread_once(&key_once,make_current_key);
    return key_valid?pthread_getspecific(current_key):NULL;
}
static int of_class(const void *object,Im2ObjcClass *klass) {
    Im2ObjcClass *cls=im2_objc_object_class(object);
    for(size_t depth=0;cls && depth<128;depth++) {
        if(cls==klass)return 1;
        const Im2ObjcClass *meta=im2_objc_class_metadata(cls);
        if(!meta)break;
        cls=meta->superclass;
    }
    return 0;
}
static void layer_destroy(void *object) {
    EaglLayer *layer=object;
    if(layer->properties)(void)im2_objc_release(layer->properties);
    layer->properties=NULL;
}
static uintptr_t layer_properties(void *receiver,const char *sel) {
    (void)sel;
    return of_class(receiver,eagl_layer_class)?
        (uintptr_t)((EaglLayer *)receiver)->properties:0u;
}
static void layer_set_properties(void *receiver,const char *sel,void *properties) {
    (void)sel;
    if(!of_class(receiver,eagl_layer_class) ||
        (properties && !of_class(properties,im2_objc_class_named("NSDictionary"))))
        abort(); /* Never install malformed drawable properties. */
    void *held=properties?im2_objc_retain(properties):NULL;
    if(properties && !held)abort();
    EaglLayer *layer=receiver;
    void *old=layer->properties;layer->properties=held;
    if(old)(void)im2_objc_release(old);
}
static uintptr_t context_init_api(void *receiver,const char *sel,uint32_t api) {
    (void)sel;
    if(!of_class(receiver,context_class))return 0;
    if(api!=IM2_EAGL_ES1) {
        (void)im2_objc_release(receiver);
        return 0; /* Owner's OpenGL ES 1.1 build; ES2 not represented. */
    }
    ((EaglContext *)receiver)->api=api;
    return (uintptr_t)receiver;
}
static uint32_t context_api(void *receiver,const char *sel) {
    (void)sel;
    return of_class(receiver,context_class)?((EaglContext *)receiver)->api:0u;
}
static uintptr_t context_current(void *cls,const char *sel) {
    (void)sel;
    return cls==context_class?(uintptr_t)get_current():0u;
}
static uint32_t context_set_current(void *cls,const char *sel,void *context) {
    (void)sel;
    if(cls!=context_class)return 0;
    if(context && (!of_class(context,context_class) ||
       ((EaglContext *)context)->api!=IM2_EAGL_ES1 ||
       !backend.ready || !backend.ready()))
        return 0;
    pthread_once(&key_once,make_current_key);
    if(!key_valid)return 0;
    void *held=context?im2_objc_retain(context):NULL;
    if(context && !held)return 0;
    void *old=pthread_getspecific(current_key);
    if(pthread_setspecific(current_key,held)) {
        if(held)(void)im2_objc_release(held);
        return 0;
    }
    if(old)(void)im2_objc_release(old);
    return 1;
}
static uint32_t context_storage(void *receiver,const char *sel,
    uint32_t target,void *drawable) {
    (void)sel;
    if(!of_class(receiver,context_class) || receiver!=get_current() ||
       target!=IM2_GL_RENDERBUFFER_OES ||
       !of_class(drawable,eagl_layer_class) ||
       !backend.ready || !backend.ready() || !backend.storage)
        return 0;
    uint32_t format=IM2_OES_RGBA8;
    void *properties=((EaglLayer *)drawable)->properties;
    if(properties){
        Im2ObjcTarget lookup;
        if(im2_objc_lookup_target(properties,"objectForKey:",NULL,&lookup) || lookup.guest || lookup.stret_size)return 0;
        /* Retained backing needs NSNumber parsing and a preservation contract.
         * Refuse this property until both exist, including an explicit false value. */
        if(((uintptr_t(*)(void *,const char *,void *))lookup.imp)(properties,"objectForKey:",retained_backing))return 0;
        void *value=(void *)((uintptr_t(*)(void *,const char *,void *))lookup.imp)(properties,"objectForKey:",color_format_key);
        if(value){const char *name=im2_string_utf8(value);if(!name || strlen(name)!=im2_string_byte_length(value))return 0;
            if(!strcmp(name,"EAGLColorFormatRGB565"))format=IM2_OES_RGB565;
            else if(strcmp(name,"EAGLColorFormatRGBA8"))return 0;
        }
    }
    ((EaglContext *)receiver)->surface_configured=0;
    int result=backend.storage(target,format);
    if(result)((EaglContext *)receiver)->surface_configured=1;
    return result?1u:0u;
}
static uint32_t context_present(void *receiver,const char *sel,uint32_t target) {
    (void)sel;
    if(!of_class(receiver,context_class) || receiver!=get_current() ||
       !((EaglContext *)receiver)->surface_configured ||
       target!=IM2_GL_RENDERBUFFER_OES || !backend.ready || !backend.ready() ||
       !backend.present)
        return 0;
    return backend.present(target)?1u:0u;
}

static void *eagl_layer_for_view(void *owner) {
    /* Only the original EAGLView needs a CAEAGLLayer backing. Other UIView
     * objects use CALayer; the guest's +layerClass is not replaced. */
    const Im2ObjcClass *meta=im2_objc_class_metadata(im2_objc_object_class(owner));
    const char *name=meta && meta->data && meta->data->ro?
        meta->data->ro->name:NULL;
    Im2ObjcClass *cls=name && !strcmp(name,"EAGLView")?
        eagl_layer_class:calayer_class;
    return im2_objc_alloc(cls,layer_destroy);
}

int im2_eagl_set_backend(Im2EaglBackend selected) {
    if(!context_class || backend.ready || !selected.ready ||
       !selected.storage || !selected.present)
        return -1;
    backend=selected;return 0;
}

#define METHOD(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_eagl_init(Im2ObjcClass *root,void (*failure)(const char *)) {
    (void)failure;
    if(context_class || !root || !im2_objc_class_named("UIView") ||
       !im2_objc_class_named("NSString") ||
       !im2_objc_class_named("NSDictionary"))
        return -1;
    size_t checkpoint=im2_objc_registration_checkpoint();
    calayer_class=im2_objc_register_class("CALayer",root,
        sizeof(EaglLayer),NULL,0,NULL,0);
    const Im2MethodSpec lm[]={
        METHOD("drawableProperties","@@:",layer_properties),
        METHOD("setDrawableProperties:","v@:@",layer_set_properties)
    };
    eagl_layer_class=calayer_class?im2_objc_register_class(
        "CAEAGLLayer",calayer_class,sizeof(EaglLayer),
        lm,sizeof(lm)/sizeof(*lm),NULL,0):NULL;
    const Im2MethodSpec cm[]={
        METHOD("initWithAPI:","@@:I",context_init_api),
        METHOD("API","I@:",context_api),
        METHOD("renderbufferStorage:fromDrawable:","c@:I@",context_storage),
        METHOD("presentRenderbuffer:","c@:I",context_present)
    };
    const Im2MethodSpec ccm[]={
        METHOD("currentContext","@@:",context_current),
        METHOD("setCurrentContext:","c@:@",context_set_current)
    };
    context_class=eagl_layer_class?im2_objc_register_class("EAGLContext",
        root,sizeof(EaglContext),cm,sizeof(cm)/sizeof(*cm),
        ccm,sizeof(ccm)/sizeof(*ccm)):NULL;
    if(!context_class)goto fail;
    retained_backing=im2_string_create("EAGLDrawablePropertyRetainedBacking",0);
    color_format_key=im2_string_create("EAGLDrawablePropertyColorFormat",0);
    rgb565_value=im2_string_create("EAGLColorFormatRGB565",0);
    if(!retained_backing || !color_format_key || !rgb565_value)goto fail;
    if(im2_uikit_bind_layer_factory(eagl_layer_for_view))
        goto fail;
    return 0;
fail:
    if(retained_backing)(void)im2_objc_release(retained_backing);
    if(color_format_key)(void)im2_objc_release(color_format_key);
    if(rgb565_value)(void)im2_objc_release(rgb565_value);
    retained_backing=color_format_key=rgb565_value=NULL;
    if(im2_objc_registration_rollback(checkpoint))abort();
    calayer_class=eagl_layer_class=context_class=NULL;
    return -1;
}

/* Externally imported dylib symbols contain an NSString * value. The guest
 * indirect non-lazy pointer slot must point to the address of that variable. */
const import_entry eagl_imports[]={
    {"_kEAGLDrawablePropertyRetainedBacking",
        (uintptr_t)&retained_backing,IM2_IMPORT_DATA},
    {"_kEAGLDrawablePropertyColorFormat",
        (uintptr_t)&color_format_key,IM2_IMPORT_DATA},
    {"_kEAGLColorFormatRGB565",
        (uintptr_t)&rgb565_value,IM2_IMPORT_DATA}
};
const int eagl_imports_count=sizeof(eagl_imports)/sizeof(*eagl_imports);
