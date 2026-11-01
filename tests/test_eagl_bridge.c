/* SPDX-License-Identifier: MIT.
 * Production EAGLContext/CAEAGLLayer IMP tests. The fake callbacks below do
 * NOT claim a Vita GPU exists: success is only injected in these host tests.
 */
#include "eagl_bridge.h"
#include "gles_oes.h"
#include "uikit_hierarchy.h"
#include "foundation.h"
#include "collections.h"
#include "numbers.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int ready,storage_success,present_success,stored,presented;
static uint32_t expected_format=IM2_OES_RGB565;
static int gpu_ready(void) {return ready;}
static int gpu_storage(uint32_t target,uint32_t format) {
    assert(target==IM2_GL_RENDERBUFFER_OES);
    assert(format==expected_format);
    stored++;
    return storage_success;
}
static int gpu_present(uint32_t target) {
    assert(target==IM2_GL_RENDERBUFFER_OES);
    presented++;
    return present_success;
}
static Im2Imp imp(void *receiver,const char *selector) {
    Im2ObjcTarget target={0};
    assert(!im2_objc_lookup_target(receiver,selector,NULL,&target));
    assert(!target.guest && !target.stret_size);
    return target.imp;
}
static void *send0(void *receiver,const char *selector) {
    return (void *)((uintptr_t(*)(void *,const char *))imp(receiver,selector))(
        receiver,selector);
}
static void *send_uint(void *receiver,const char *selector,uint32_t number) {
    return (void *)((uintptr_t(*)(void *,const char *,uint32_t))imp(receiver,selector))(
        receiver,selector,number);
}
static void send_void1(void *receiver,const char *selector,void *object) {
    ((void(*)(void *,const char *,void *))imp(receiver,selector))(
        receiver,selector,object);
}
static uint32_t send_bool1(void *receiver,const char *selector,void *object) {
    return ((uint32_t(*)(void *,const char *,void *))imp(receiver,selector))(
        receiver,selector,object);
}
static uint32_t send_bool_uint(void *receiver,const char *selector,uint32_t number) {
    return ((uint32_t(*)(void *,const char *,uint32_t))imp(receiver,selector))(
        receiver,selector,number);
}
static uint32_t send_bool_uint_object(void *receiver,const char *selector,
    uint32_t number,void *object) {
    return ((uint32_t(*)(void *,const char *,uint32_t,void *))imp(receiver,selector))(
        receiver,selector,number,object);
}
static void destroy_view(void *view) {
    ((void(*)(void *,const char *))imp(view,"dealloc"))(view,"dealloc");
}

int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,
        sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    Im2FoundationConfig config={"/tmp",NULL,NULL,NULL};
    assert(!im2_foundation_init(root,&config));
    assert(!im2_collections_init(root,NULL));
    assert(!im2_numbers_init(root));
    void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
    assert(!im2_uikit_hierarchy_init(root,NULL));
    assert(!im2_eagl_init(root,NULL));
    assert(im2_eagl_init(root,NULL));
    Im2ObjcClass *context_class=im2_objc_class_named("EAGLContext");
    Im2ObjcClass *layer_class=im2_objc_class_named("CAEAGLLayer");
    Im2ObjcClass *calayer=im2_objc_class_named("CALayer");
    assert(context_class && layer_class && calayer);
    assert(im2_objc_class_metadata(layer_class)->superclass==calayer);
    assert(im2_objc_class_metadata(context_class)->superclass==root);

    assert(eagl_imports_count==3);
    for(int i=0;i<eagl_imports_count;i++) {
        assert(eagl_imports[i].data==IM2_IMPORT_DATA);
        void *constant=*(void **)(uintptr_t)eagl_imports[i].addr;
        const char *text=im2_string_utf8(constant);
        assert(text && *text && !strcmp(text,eagl_imports[i].name+2));
        assert(im2_objc_retain_count(constant)==1);
    }
    /* The public kEAGL... symbol names include a k prefix; actual NSString
     * property-key values deliberately omit it. */
    assert(!strcmp(im2_string_utf8(*(void **)(uintptr_t)eagl_imports[1].addr),
        "EAGLDrawablePropertyColorFormat"));

    Im2ObjcClass *eaglview=im2_objc_register_foreign_class("EAGLView",
        (void *)(uintptr_t)0x24000000,(void *)(uintptr_t)0x24000020,
        im2_objc_class_named("UIView"),128,NULL,0,NULL,0);
    assert(eaglview);
    void *view=im2_objc_alloc(eaglview,destroy_view);
    void *ordinary=im2_objc_alloc(im2_objc_class_named("UIView"),destroy_view);
    assert(view && ordinary);
    void *layer=send0(view,"layer");
    assert(layer && layer==send0(view,"layer"));
    assert(im2_objc_object_class(layer)==layer_class);
    assert(im2_objc_object_class(send0(ordinary,"layer"))==calayer);
    assert(im2_objc_retain_count(layer)==1);
    assert(((uint32_t(*)(void *,const char *))imp(layer,"isOpaque"))(layer,"isOpaque")==0);
    ((void(*)(void *,const char *,uint32_t))imp(layer,"setOpaque:"))(layer,"setOpaque:",3);
    assert(((uint32_t(*)(void *,const char *))imp(layer,"isOpaque"))(layer,"isOpaque")==1);
    ((void(*)(void *,const char *,uint32_t))imp(layer,"setOpaque:"))(layer,"setOpaque:",0x100);
    assert(((uint32_t(*)(void *,const char *))imp(layer,"isOpaque"))(layer,"isOpaque")==0);

    void *key=im2_string_create("EAGLDrawablePropertyColorFormat",0);
    void *value=im2_string_create("EAGLColorFormatRGB565",0);
    assert(key && value);
    void *keys[]={key},*values[]={value};
    void *properties=im2_dictionary_create(values,keys,1,0,0);
    assert(properties);
    send_void1(layer,"setDrawableProperties:",properties);
    assert(send0(layer,"drawableProperties")==properties);
    assert(im2_objc_retain_count(properties)==2);
    assert(!im2_objc_release(properties));
    assert(im2_objc_retain_count(properties)==1);

    void *invalid=im2_objc_alloc(context_class,NULL);
    assert(invalid && !send_uint(invalid,"initWithAPI:",2));
    Im2ObjcClass *cls=context_class;
    void *context=im2_objc_alloc(cls,NULL);
    assert(context && send_uint(context,"initWithAPI:",1)==context);
    assert((uintptr_t)send0(context,"API")==1u);
    assert(!send0(cls,"currentContext"));
    assert(!send_bool1(cls,"setCurrentContext:",context));
    assert(!send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",
        IM2_GL_RENDERBUFFER_OES,layer));
    assert(!send_bool_uint(context,"presentRenderbuffer:",IM2_GL_RENDERBUFFER_OES));
    assert(!im2_eagl_set_backend((Im2EaglBackend){
        gpu_ready,gpu_storage,gpu_present}));
    assert(im2_eagl_set_backend((Im2EaglBackend){
        gpu_ready,gpu_storage,gpu_present}));
    ready=1;
    assert(send_bool1(cls,"setCurrentContext:",context)==1u);
    assert(send0(cls,"currentContext")==context);
    assert(im2_objc_retain_count(context)==2);
    assert(!send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",
        0x8D40u,layer));
    assert(!send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",
        IM2_GL_RENDERBUFFER_OES,ordinary));
    assert(!send_bool_uint(context,"presentRenderbuffer:",IM2_GL_RENDERBUFFER_OES));
    assert(stored==0 && presented==0);
    storage_success=0;
    assert(!send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",
        IM2_GL_RENDERBUFFER_OES,layer));
    assert(stored==1 && !presented);
    storage_success=1;
    assert(send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",
        IM2_GL_RENDERBUFFER_OES,layer));
    assert(stored==2);
    assert(im2_objc_retain(properties)==properties);
    void *backing_key=*(void **)(uintptr_t)eagl_imports[0].addr;
    void *backing_properties=im2_dictionary_create(&value,&backing_key,1,0,0);assert(backing_properties);
    send_void1(layer,"setDrawableProperties:",backing_properties);
    assert(!send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",IM2_GL_RENDERBUFFER_OES,layer));
    assert(stored==2); /* Unsupported retained-backing semantics never reach the GPU callback. */
    send_void1(layer,"setDrawableProperties:",properties);
    assert(!im2_objc_release(backing_properties) && !im2_objc_release(properties));
    present_success=0;
    assert(!send_bool_uint(context,"presentRenderbuffer:",IM2_GL_RENDERBUFFER_OES));
    assert(presented==1);
    present_success=1;
    assert(send_bool_uint(context,"presentRenderbuffer:",IM2_GL_RENDERBUFFER_OES));
    assert(presented==2);
    storage_success=0;
    assert(!send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",IM2_GL_RENDERBUFFER_OES,layer));
    assert(!send_bool_uint(context,"presentRenderbuffer:",IM2_GL_RENDERBUFFER_OES));
    assert(presented==2);
    send_void1(layer,"setDrawableProperties:",NULL);expected_format=IM2_OES_RGBA8;storage_success=1;
    assert(send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",IM2_GL_RENDERBUFFER_OES,layer));
    void *no=send_uint(im2_objc_class_named("NSNumber"),"numberWithBool:",0),*yes=send_uint(im2_objc_class_named("NSNumber"),"numberWithBool:",1);assert(no && yes);
    assert(im2_objc_retain(no)==no && im2_objc_retain(yes)==yes);
    void *no_properties=im2_dictionary_create(&no,&backing_key,1,0,0);
    void *yes_properties=im2_dictionary_create(&yes,&backing_key,1,0,0);assert(no_properties && yes_properties);
    send_void1(layer,"setDrawableProperties:",no_properties);
    assert(send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",IM2_GL_RENDERBUFFER_OES,layer));
    void *both_keys[]={backing_key,key},*both_values[]={no,value};
    void *both=im2_dictionary_create(both_values,both_keys,2,0,0);assert(both);
    send_void1(layer,"setDrawableProperties:",both);expected_format=IM2_OES_RGB565;
    assert(send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",IM2_GL_RENDERBUFFER_OES,layer));
    int before=stored;
    send_void1(layer,"setDrawableProperties:",yes_properties);
    assert(!send_bool_uint_object(context,"renderbufferStorage:fromDrawable:",IM2_GL_RENDERBUFFER_OES,layer) && stored==before);
    send_void1(layer,"setDrawableProperties:",NULL);
    assert(!im2_objc_release(no_properties) && !im2_objc_release(yes_properties) && !im2_objc_release(both));
    assert(!im2_objc_release(no) && !im2_objc_release(yes));
    ready=0;
    assert(!send_bool_uint(context,"presentRenderbuffer:",IM2_GL_RENDERBUFFER_OES));
    assert(presented==2);
    assert(send_bool1(cls,"setCurrentContext:",NULL));
    assert(!send0(cls,"currentContext"));
    assert(im2_objc_retain_count(context)==1);
    assert(!im2_objc_release(context));

    assert(!im2_objc_release(key));
    assert(!im2_objc_release(value));
    assert(!im2_objc_release(view)); /* Releases CAEAGLLayer & property dictionary. */
    assert(im2_objc_retain_count(layer)==0);
    assert(!im2_objc_release(ordinary));
    assert(im2_objc_retain_count(properties)==0);
    assert(!im2_objc_pool_drain(pool) && !im2_objc_retain_count(no) && !im2_objc_retain_count(yes));
    puts("PASS: EAGL original class identities, exact constant data pointers, UIView.layer, real context/thread state and GPU-failure gates");
    return 0;
}
