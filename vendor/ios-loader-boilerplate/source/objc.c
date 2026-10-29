/** Partial Foundation/CoreGraphics/IME bridge. Native methods are registered by class;
 * unsupported selectors fail explicitly. Foundation objects live in foundation.c. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "darwin_printf.h"
#include "hle.h"
#include "imports.h"
#include "log.h"
#include "macho.h"
#include "objc_runtime.h"
#include "objc.h"
#include "foundation.h"
#include "collections.h"
#include "darwin.h"

// ---------------------------------------------------------------- NSString

typedef Im2String ns_string;
static Im2ObjcClass *string_class,*pool_class;
static import_entry class_imports[19];
static void bind_class_imports(void) {
    static const char *names[]={"_OBJC_CLASS_$_NSObject","_OBJC_METACLASS_$_NSObject",
        "_OBJC_CLASS_$_NSString","_OBJC_METACLASS_$_NSString",
        "_OBJC_CLASS_$_NSAutoreleasePool","_OBJC_METACLASS_$_NSAutoreleasePool",
        "_OBJC_CLASS_$_NSData","_OBJC_METACLASS_$_NSData",
        "_OBJC_CLASS_$_NSBundle","_OBJC_METACLASS_$_NSBundle",
        "_OBJC_CLASS_$_NSArray","_OBJC_METACLASS_$_NSArray",
        "_OBJC_CLASS_$_NSMutableArray","_OBJC_METACLASS_$_NSMutableArray",
        "_OBJC_CLASS_$_NSDictionary","_OBJC_METACLASS_$_NSDictionary",
        "_OBJC_CLASS_$_NSMutableDictionary","_OBJC_METACLASS_$_NSMutableDictionary"};
    for(size_t i=0;i<18;i++) {
        const char *name=names[i];
        Im2ObjcClass *c=im2_objc_class_named(i&1?name+18:name+14);
        const Im2ObjcClass *m=im2_objc_class_metadata(c);
        if(!m)fatal_error("Missing native class import: %s",name);
        class_imports[i]=(import_entry){name,(uintptr_t)(i&1?m->isa:c),IM2_IMPORT_DATA};
    }
    class_imports[18]=(import_entry){"___CFConstantStringClassReference",(uintptr_t)string_class,IM2_IMPORT_DATA};
}
static ns_string *ns_new(const char *text) {
    ns_string *s=im2_string_create(text,1);
    if(!s)fatal_error("NSString UTF-8 creation or autorelease failed");
    return s;
}
static const char *ns_cstr(const void *o) {return im2_string_utf8(o);}

static uint32_t utf8_next(const unsigned char **p) {
    const unsigned char *s = *p;
    uint32_t c = *s++;
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= extra ? 0x3F >> extra : 0xFF;
    while (extra-- && (*s & 0xC0) == 0x80)
        c = (c << 6) | (*s++ & 0x3F);
    *p = s;
    return c;
}

static int utf16_len(const char *s) {
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p;)
        n += utf8_next(&p) > 0xFFFF ? 2 : 1;
    return n;
}

// stringWithFormat: using Darwin variadic printf (%@, unaligned 64-bit words).
static ns_string *ns_format(const char *fmt, va_list ap) {
    char out[1024];
    va_list cp;
    va_copy(cp, ap);
    words w = {&cp, NULL};
    int result=darwin_vformat(out, sizeof(out), fmt, &w);
    va_end(cp);
    if(result<0 || (size_t)result>=sizeof(out))fatal_error("Unsupported or oversized NSString format");
    return ns_new(out);
}

// ---------------------------------------------------------------- fonts (FreeType)

typedef struct {
    uint32_t key; // size << 24 | codepoint
    int16_t left, top, adv;
    uint16_t w, h;
    uint8_t *bmp;
} glyph;

#define GLYPH_CACHE 4096
static glyph cache[GLYPH_CACHE];
static FT_Library ft;
static FT_Face faces[4];
static int nfaces = -1;

static void fonts_init(void) {
    // Custom font.ttf checked first, then PS Vita system fonts.
    static const char *paths[] = {DATA_PATH "font.ttf", "sa0:data/font/pvf/ltn0.pvf",
                                  "sa0:data/font/pvf/kr0.pvf", "sa0:data/font/pvf/jpn0.pvf"};
    nfaces = 0;
    if (FT_Init_FreeType(&ft))
        return l_error("FreeType failed to initialize: font rendering disabled");
    for (int i = 0; i < 4; i++)
        if (!FT_New_Face(ft, paths[i], 0, &faces[nfaces])) {
            l_info("loaded font: %s", paths[i]);
            nfaces++;
        }
    if (!nfaces)
        l_error("no fonts loaded: text rendering disabled");
}

static const glyph *get_glyph(int size, uint32_t cp) {
    static glyph tmp;
    uint32_t key = (uint32_t)size << 24 | (cp & 0xFFFFFF);
    uint32_t h = (key * 2654435761u) % GLYPH_CACHE;
    glyph *g = NULL;
    for (int i = 0; i < 16; i++) {
        glyph *c = &cache[(h + i) % GLYPH_CACHE];
        if (c->key == key)
            return c;
        if (!c->key) {
            g = c;
            break;
        }
    }
    if (!g) { // cache full for this bucket: rasterize directly without caching
        free(tmp.bmp);
        g = &tmp;
    }
    memset(g, 0, sizeof(*g));
    g->key = key;
    if (nfaces < 0)
        fonts_init();
    FT_Face f = NULL;
    for (int i = 0; i < nfaces && !f; i++)
        if (FT_Get_Char_Index(faces[i], cp))
            f = faces[i];
    if (!f) {
        g->adv = cp == ' ' ? size / 3 : size / 2;
        return g;
    }
    FT_Set_Pixel_Sizes(f, 0, size);
    if (FT_Load_Char(f, cp, FT_LOAD_RENDER))
        return g;
    FT_GlyphSlot s = f->glyph;
    g->left = s->bitmap_left;
    g->top = s->bitmap_top;
    g->adv = s->advance.x >> 6;
    g->w = s->bitmap.width;
    g->h = s->bitmap.rows;
    g->bmp = malloc(g->w * g->h + 1);
    for (int y = 0; y < g->h; y++)
        memcpy(g->bmp + y * g->w, s->bitmap.buffer + y * s->bitmap.pitch, g->w);
    return g;
}

// Line height and baseline approximations matching iOS 3 Helvetica (14pt->18, 13->16, 11->14).
// Adjust here if vertical alignment requires calibration.
static int line_height(int size) { return size == 14 ? 18 : size == 13 ? 16 : size == 11 ? 14 : (size * 5 + 2) / 4; }
static int baseline(int size) { return (line_height(size) - size) / 2 + (size * 4 + 2) / 5; }

static int text_width(int size, const char *s) {
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p;)
        w += get_glyph(size, utf8_next(&p))->adv;
    return w;
}

// ---------------------------------------------------------------- CoreGraphics / UIKit

typedef struct {
    uint8_t *data;
    int w, h, bpr;
    float gray;
} cg_ctx;

static cg_ctx *cur_ctx;

static cg_ctx *cg_BitmapContextCreate(void *data, int w, int h, int bpc, int bpr, void *cs, int info) {
    cg_ctx *c = calloc(1, sizeof(*c));
    *c = (cg_ctx){data, w, h, bpr, 1.0f};
    return c;
}
static void cg_ContextRelease(cg_ctx *c) {
    if (cur_ctx == c)
        cur_ctx = NULL;
    free(c);
}
SOFTFP static void cg_SetGrayFillColor(cg_ctx *c, float gray, float alpha) { c->gray = gray * alpha; }
static void *cg_ColorSpaceCreate(void) { return (void *)1; }
static void cg_noop(void) {}
static void ui_PushContext(cg_ctx *c) { cur_ctx = c; }
static void ui_PopContext(void) { cur_ctx = NULL; }

typedef struct {
    uint32_t tag;
    int size;
} ui_font;

static ui_font *font_of_size(float pt) {
    static ui_font fonts[32];
    int size = (int)pt;
    for (int i = 0; i < 32; i++)
        if (!fonts[i].tag || fonts[i].size == size) {
            fonts[i] = (ui_font){0x464F4E54, size};
            return &fonts[i];
        }
    return &fonts[31];
}

// UIKit draws with Y-axis pointing downwards; atlas context is un-inverted,
// placing text upside-down in memory (glyph row j -> row h-1-j), as expected by guest quad blitters.
static void draw_text(cg_ctx *c, const char *s, int x, int y, int size) {
    int pen = x, base = baseline(size);
    uint8_t ink = (uint8_t)(c->gray * 255.0f);
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        const glyph *g = get_glyph(size, utf8_next(&p));
        for (int gy = 0; gy < g->h; gy++) {
            int row = c->h - 1 - (y + base - g->top + gy);
            if (row < 0 || row >= c->h)
                continue;
            for (int gx = 0; gx < g->w; gx++) {
                int col = pen + g->left + gx;
                if (col < 0 || col >= c->w)
                    continue;
                uint8_t v = g->bmp[gy * g->w + gx] * ink / 255, *d = &c->data[row * c->bpr + col];
                if (v > *d)
                    *d = v;
            }
        }
        pen += g->adv;
    }
}

// ---------------------------------------------------------------- text input (IME)

static int ime_type = -1;
static char ime_utf8[128];
static uint16_t ime_buf[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1], ime_title[16], ime_initial[1];

static void ime_open(int type) {
    static const char *titles[] = {"Name", "Number"};
    const char *t = titles[type == 1];
    for (int i = 0; (ime_title[i] = t[i]); i++) {}
    SceImeDialogParam p;
    sceImeDialogParamInit(&p);
    p.type = type == 1 ? SCE_IME_TYPE_NUMBER : SCE_IME_TYPE_DEFAULT;
    p.title = ime_title;
    p.maxTextLength = type == 1 ? 4 : 12;
    p.initialText = ime_initial;
    p.inputTextBuffer = ime_buf;
    ime_utf8[0] = 0;
    int r = sceImeDialogInit(&p);
    if (r < 0)
        l_error("sceImeDialogInit = 0x%08X", r);
    ime_type = type; // even if init fails: ime_poll will close dialog event so game doesn't stall
}

int ime_active(void) { return ime_type >= 0; }

void ime_poll(void) {
    if (ime_type < 0)
        return;
    SceCommonDialogStatus st = sceImeDialogGetStatus();
    if (st == SCE_COMMON_DIALOG_STATUS_RUNNING)
        return;
    if (st == SCE_COMMON_DIALOG_STATUS_FINISHED) {
        SceImeDialogResult res;
        memset(&res, 0, sizeof(res));
        sceImeDialogGetResult(&res);
        if (res.button == SCE_IME_DIALOG_BUTTON_ENTER) {
            size_t n = 0;
            for (int i = 0; ime_buf[i] && n < sizeof(ime_utf8) - 4; i++) {
                uint16_t c = ime_buf[i];
                if (c < 0x80)
                    ime_utf8[n++] = c;
                else if (c < 0x800)
                    n += sprintf(ime_utf8 + n, "%c%c", 0xC0 | c >> 6, 0x80 | (c & 0x3F));
                else
                    n += sprintf(ime_utf8 + n, "%c%c%c", 0xE0 | c >> 12, 0x80 | ((c >> 6) & 0x3F), 0x80 | (c & 0x3F));
            }
            ime_utf8[n] = 0;
        }
        sceImeDialogTerm();
    }
    int type = ime_type;
    ime_type = -1;
    l_info("input text entered (%d): \"%s\"", type, ime_utf8);
    uintptr_t text_close_fn = macho_sym("_Ex_HandleEventTextClose");
    if (text_close_fn)
        call_guest((void *)text_close_fn, type, 0, 0, 0);
}

// ---------------------------------------------------------------- objc_msgSend

static uintptr_t string_format(void *self,const char *selector,void *format,...) {
    (void)self;(void)selector;
    const char *text=ns_cstr(format);if(!text)fatal_error("Unsupported NSString format object");
    va_list ap;va_start(ap,format);ns_string *result=ns_format(text,ap);va_end(ap);
    return (uintptr_t)result;
}
static void foundation_failure(const char *message){fatal_error("Foundation: %s",message);}

// Class-specific dispatch for explicit native classes and validated guest metadata.
// UIKit behavior and aggregate guest-return ABIs remain incomplete.
static void object_destroy(void *self) {
    Im2ObjcTarget target;
    if(im2_objc_lookup_target(self,"dealloc",NULL,&target) || target.stret_size)
        fatal_error("Unsupported Objective-C dealloc implementation");
    const char *sel=im2_objc_sel_register("dealloc");
    if(target.guest) call_guest((void *)target.imp,(uint32_t)self,(uint32_t)sel,0,0);
    else ((void (*)(void *,const char *))target.imp)(self,sel);
}
static void *object_alloc(Im2ObjcClass *self,const char *sel) {
    (void)sel; return im2_objc_alloc(self,object_destroy);
}
static void object_dealloc(void *self,const char *sel) {
    (void)sel;
    /* The outer release owns storage disposal after the dealloc chain returns. */
    if(im2_objc_retain_count(self)!=0) fatal_error("Direct dealloc outside final release is unsupported");
}
static void *object_init(void *self,const char *sel) { (void)sel; return self; }
static void *object_retain(void *self,const char *sel) { (void)sel; return im2_objc_retain(self); }
static void object_release(void *self,const char *sel) {
    (void)sel;
    if(im2_objc_object_class(self)==pool_class) {
        if(im2_objc_pool_drain(self)) fatal_error("Invalid autorelease pool release order");
    } else if(im2_objc_release(self)) fatal_error("release on unknown Objective-C object");
}
static void *object_autorelease(void *self,const char *sel) {
    (void)sel; if(!im2_objc_autorelease(self)) fatal_error("autorelease without active pool"); return self;
}
static Im2ObjcClass *object_class(void *self,const char *sel) { (void)sel; return im2_objc_object_class(self); }
static Im2ObjcClass *class_class(Im2ObjcClass *self,const char *sel) { (void)sel;return self; }
static uint32_t object_responds(void *self,const char *sel,const char *query) {
    (void)sel;return im2_objc_lookup(self,query,NULL)!=NULL;
}
static void *pool_init(void *self,const char *sel) {
    (void)sel; if(im2_objc_pool_push(self)) fatal_error("Autorelease pool initialization failed");return self;
}
static void *im2_get_class(const char *name) { return im2_objc_class_named(name); }
static void *im2_get_metaclass(const char *name) {
    const Im2ObjcClass *c=im2_objc_class_metadata(im2_objc_class_named(name));return c?c->isa:NULL;
}
static const char *im2_selector_name(const char *name) {return name;}
extern void im2_objc_msgSend_entry(void),im2_objc_msgSend_stret_entry(void);
extern void im2_objc_msgSendSuper2_entry(void),im2_objc_msgSendSuper2_stret_entry(void);
static uint64_t lookup_target_words(void *self,const char *selector,uint32_t stret,const void *current) {
    Im2ObjcTarget target;
    if(im2_objc_lookup_target(self,selector,current,&target) || target.stret_size==SIZE_MAX ||
       !!stret != !!target.stret_size)
        fatal_error("Unsupported ObjC receiver/selector/return ABI: %p %s stret=%u",self,selector?selector:"(null)",(unsigned)stret);
    /* AAPCS uint64_t return: r0=IMP, r1=host flag. Guest IMPs keep Darwin SP/registers. */
    return (uint32_t)(uintptr_t)target.imp | ((uint64_t)!target.guest<<32);
}
uint64_t im2_objc_lookup_words(void *self,const char *selector,uint32_t stret) {
    return lookup_target_words(self,selector,stret,NULL);
}
uint64_t im2_objc_lookup_super_words(const void *frame,const char *selector,uint32_t stret) {
    if(!frame) fatal_error("Null objc_super2 frame");
    void *const *words=frame;
    return lookup_target_words(words[0],selector,stret,words[1]);
}
#define METHOD(sel,types,imp) {sel,types,(Im2Imp)(imp),0}
void objc_init(void) {
    if(string_class) return;
    const Im2MethodSpec root_instance[]={
        METHOD("init","@@:",object_init),METHOD("retain","@@:",object_retain),
        METHOD("release","v@:",object_release),METHOD("autorelease","@@:",object_autorelease),
        METHOD("class","#@:",object_class),METHOD("respondsToSelector:","c@::",object_responds),
        METHOD("dealloc","v@:",object_dealloc)};
    const Im2MethodSpec root_class[]={METHOD("alloc","@@:",object_alloc),METHOD("class","#@:",class_class)};
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),
        root_instance,7,root_class,2);
    const Im2FoundationConfig foundation={BUNDLE_PATH,(Im2Imp)string_format,foundation_failure,darwin_path};
    if(im2_foundation_init(root,&foundation))fatal_error("Foundation registration failed");
    if(im2_collections_init(root,foundation_failure))fatal_error("Collection registration failed");
    string_class=im2_objc_class_named("NSString");
    darwin_set_object_string_resolver(im2_string_utf8);
    const Im2MethodSpec pool_methods[]={METHOD("init","@@:",pool_init),METHOD("drain","v@:",object_release)};
    pool_class=im2_objc_register_class("NSAutoreleasePool",root,sizeof(Im2ObjcObject),pool_methods,2,NULL,0);
    if(!root || !string_class || !pool_class) fatal_error("ObjC class registry initialization failed");
    bind_class_imports();
}
const import_entry *objc_class_import(const char *name) {
    if(!name)return NULL;
    for(size_t i=0;i<19;i++)if(class_imports[i].name && !strcmp(name,class_imports[i].name))return &class_imports[i];
    return NULL;
}

static uint32_t cf_string_get_cstring(void *string,char *buffer,int32_t capacity,uint32_t encoding) {
    if(capacity<=0)return 0;
    uint32_t nsencoding=encoding==0x08000100u?IM2_NS_UTF8:encoding==0x0600u?IM2_NS_ASCII:0;
    return nsencoding && im2_string_get_cstring(string,buffer,(size_t)capacity,nsencoding)==0;
}
static void cf_release(void *value) {
    if(!value || im2_objc_release(value))fatal_error("CFRelease on unsupported object");
}
static void *cf_retain(void *value) {
    if(!value || !im2_objc_retain(value))fatal_error("CFRetain on unsupported object");
    return value;
}
static void *ns_class_from_string(void *string) {
    const char *name=im2_string_utf8(string);
    if(!name)fatal_error("NSClassFromString requires a supported NSString");
    return im2_objc_class_named(name);
}

static void enumeration_mutation(void *object) {
    fatal_error("Collection mutated during Objective-C enumeration: %p",object);
}

// ---------------------------------------------------------------- table

#undef F
#define F(name, fn) {"_" #name, (uintptr_t)(fn), 0}

const import_entry objc_imports[] = {
    F(CFStringGetCString, cf_string_get_cstring),F(CFRelease, cf_release),F(CFRetain, cf_retain),
    F(NSClassFromString, ns_class_from_string),F(objc_enumerationMutation, enumeration_mutation),
    F_ABI(objc_msgSend, im2_objc_msgSend_entry), F_ABI(objc_msgSend_stret, im2_objc_msgSend_stret_entry),
    F_ABI(objc_msgSendSuper2, im2_objc_msgSendSuper2_entry),
    F_ABI(objc_msgSendSuper2_stret, im2_objc_msgSendSuper2_stret_entry),
    F(objc_getClass, im2_get_class), F(objc_getMetaClass, im2_get_metaclass),
    F(sel_registerName, im2_objc_sel_register), F(sel_getName, im2_selector_name),
    F(CGBitmapContextCreate, cg_BitmapContextCreate), F(CGContextRelease, cg_ContextRelease),
    F(CGContextSetGrayFillColor, cg_SetGrayFillColor), F(CGContextSetTextDrawingMode, cg_noop),
    F(CGColorSpaceCreateDeviceGray, cg_ColorSpaceCreate), F(CGColorSpaceCreateDeviceRGB, cg_ColorSpaceCreate),
    F(CGColorSpaceRelease, cg_noop), F(UIGraphicsPushContext, ui_PushContext),
    F(UIGraphicsPopContext, ui_PopContext),
};
const int objc_imports_count = sizeof(objc_imports) / sizeof(objc_imports[0]);
