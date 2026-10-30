/* SPDX-License-Identifier: MIT. Independently authored; no game/Apple code. */
#include "gles_oes.h"
#include <stdlib.h>
#define LIMIT 64
/* Color renderbuffers are GPU textures; depth uses the backend's depth storage.
 * Only RGBA8, RGB565 and DEPTH_COMPONENT16 are supported in this initial subset. */
typedef struct {uint32_t name,handle,color,depth;} Framebuffer;
typedef struct {uint32_t name,handle,format;int width,height;} Renderbuffer;
struct Im2Oes {Im2OesBackend b;Framebuffer f[LIMIT];Renderbuffer r[LIMIT];uint32_t next_f,next_r,bound_f,bound_r;};
static Framebuffer *fb(Im2Oes *s,uint32_t n){for(int i=0;i<LIMIT;i++)if(n && s->f[i].name==n)return s->f+i;return NULL;}
static Renderbuffer *rb(Im2Oes *s,uint32_t n){for(int i=0;i<LIMIT;i++)if(n && s->r[i].name==n)return s->r+i;return NULL;}
static void restore(Im2Oes *s){Framebuffer *f=fb(s,s->bound_f);s->b.bind_framebuffer(s->b.ctx,f?f->handle:0);}
Im2Oes *im2_oes_create(Im2OesBackend b){
    if(!b.new_framebuffer || !b.delete_framebuffer || !b.bind_framebuffer || !b.new_storage || !b.delete_storage || !b.attach || !b.check)return NULL;
    Im2Oes *s=calloc(1,sizeof(*s));if(s){s->b=b;s->next_f=s->next_r=1;}return s;
}
void im2_oes_destroy(Im2Oes *s){if(!s)return;s->b.bind_framebuffer(s->b.ctx,0);for(int i=0;i<LIMIT;i++){
    if(s->f[i].handle)s->b.delete_framebuffer(s->b.ctx,s->f[i].handle);
    if(s->r[i].handle)s->b.delete_storage(s->b.ctx,s->r[i].format,s->r[i].handle);
}free(s);}
uint32_t im2_oes_gen(Im2Oes *s,uint32_t target,int n,uint32_t *out){
    if(!s)return IM2_OES_OPERATION;
    if(target!=IM2_OES_FB && target!=IM2_OES_RB)return IM2_OES_ENUM;
    if(n<0 || (n && !out))return IM2_OES_VALUE;
    if(!n)return 0;
    if(n>LIMIT)return IM2_OES_MEMORY;
    int indices[LIMIT],found=0;uint32_t handles[LIMIT]={0};
    for(int i=0;i<LIMIT && found<n;i++)if(target==IM2_OES_FB?!s->f[i].name:!s->r[i].name)indices[found++]=i;
    uint32_t *next=target==IM2_OES_FB?&s->next_f:&s->next_r;
    if(found<n || (uint64_t)*next+n>UINT32_MAX)return IM2_OES_MEMORY;
    if(target==IM2_OES_FB)for(int i=0;i<n;i++)if(!(handles[i]=s->b.new_framebuffer(s->b.ctx))){
        for(int j=0;j<i;j++)s->b.delete_framebuffer(s->b.ctx,handles[j]);
        restore(s);return IM2_OES_MEMORY;
    }
    for(int i=0;i<n;i++){uint32_t name=(*next)++;out[i]=name;if(target==IM2_OES_FB)s->f[indices[i]]=(Framebuffer){name,handles[i],0,0};else s->r[indices[i]]=(Renderbuffer){name,0,0,0,0};}
    restore(s);return 0;
}
uint32_t im2_oes_bind(Im2Oes *s,uint32_t target,uint32_t name){
    if(!s)return IM2_OES_OPERATION;
    if(target==IM2_OES_FB){Framebuffer *f=fb(s,name);if(name && !f)return IM2_OES_OPERATION;s->bound_f=name;restore(s);return 0;}
    if(target!=IM2_OES_RB)return IM2_OES_ENUM;
    if(name && !rb(s,name))return IM2_OES_OPERATION;
    s->bound_r=name;return 0;
}
uint32_t im2_oes_binding(Im2Oes *s,uint32_t target){return !s?0:target==IM2_OES_FB?s->bound_f:target==IM2_OES_RB?s->bound_r:0;}
uint32_t im2_oes_storage(Im2Oes *s,uint32_t format,int w,int h){
    if(!s)return IM2_OES_OPERATION;
    Renderbuffer *r=rb(s,s->bound_r);if(!r)return IM2_OES_OPERATION;
    if(format!=IM2_OES_RGBA8 && format!=IM2_OES_RGB565 && format!=IM2_OES_DEPTH16)return IM2_OES_ENUM;
    if(w<=0 || h<=0 || w>2048 || h>2048)return IM2_OES_VALUE;
    /* Reallocation while attached needs full GL attachment lifetime handling.
     * Refuse it instead of leaving GPU attachments pointing at freed storage. */
    for(int i=0;i<LIMIT;i++)if(s->f[i].name && (s->f[i].color==r->name || s->f[i].depth==r->name))return IM2_OES_OPERATION;
    uint32_t handle=s->b.new_storage(s->b.ctx,format,w,h);restore(s);
    if(!handle)return IM2_OES_MEMORY;
    if(r->handle)s->b.delete_storage(s->b.ctx,r->format,r->handle);
    r->handle=handle;r->format=format;r->width=w;r->height=h;return 0;
}
uint32_t im2_oes_attach(Im2Oes *s,uint32_t attachment,uint32_t name){
    if(!s)return IM2_OES_OPERATION;
    if(attachment!=IM2_OES_COLOR && attachment!=IM2_OES_DEPTH)return IM2_OES_ENUM;
    Framebuffer *f=fb(s,s->bound_f);Renderbuffer *r=rb(s,name);
    if(!f || (name && (!r || !r->handle)))return IM2_OES_OPERATION;
    uint32_t format=attachment==IM2_OES_COLOR?(r?r->format:IM2_OES_RGBA8):IM2_OES_DEPTH16;
    if(r && ((attachment==IM2_OES_COLOR && format!=IM2_OES_RGBA8 && format!=IM2_OES_RGB565) || (attachment==IM2_OES_DEPTH && r->format!=IM2_OES_DEPTH16)))return IM2_OES_OPERATION;
    int ok=s->b.attach(s->b.ctx,f->handle,attachment,format,r?r->handle:0);restore(s);
    if(!ok)return IM2_OES_OPERATION;
    if(attachment==IM2_OES_COLOR)f->color=name;else f->depth=name;return 0;
}
static uint32_t status(Im2Oes *s,Framebuffer *f){
    if(!f)return IM2_OES_MISSING;
    Renderbuffer *c=rb(s,f->color),*d=rb(s,f->depth);
    if(!c || !c->handle)return IM2_OES_MISSING;
    if(f->depth && (!d || !d->handle))return IM2_OES_INCOMPLETE;
    if(d && (c->width!=d->width || c->height!=d->height))return IM2_OES_DIMENSIONS;
    uint32_t result=s->b.check(s->b.ctx,f->handle);restore(s);return result;
}
uint32_t im2_oes_status(Im2Oes *s){return s?status(s,fb(s,s->bound_f)):IM2_OES_INCOMPLETE;}
uint32_t im2_oes_parameter(Im2Oes *s,uint32_t pname,int *out){
    if(!s)return IM2_OES_OPERATION;
    Renderbuffer *r=rb(s,s->bound_r);if(!out)return IM2_OES_VALUE;if(!r)return IM2_OES_OPERATION;
    switch(pname){case 0x8D42:*out=r->width;break;case 0x8D43:*out=r->height;break;case 0x8D44:*out=(int)r->format;break;default:return IM2_OES_ENUM;}return 0;
}
uint32_t im2_oes_delete(Im2Oes *s,uint32_t target,int n,const uint32_t *names){
    if(!s)return IM2_OES_OPERATION;
    if(target!=IM2_OES_FB && target!=IM2_OES_RB)return IM2_OES_ENUM;
    if(n<0 || (n && !names))return IM2_OES_VALUE;
    for(int j=0;j<n;j++)if(target==IM2_OES_FB){Framebuffer *f=fb(s,names[j]);if(!f)continue;
        if(s->bound_f==f->name)s->bound_f=0;
        s->b.delete_framebuffer(s->b.ctx,f->handle);*f=(Framebuffer){0};
    }else{Renderbuffer *r=rb(s,names[j]);if(!r)continue;
        /* Explicit subset: detach this renderbuffer from every owned FBO. */
        for(int i=0;i<LIMIT;i++){Framebuffer *f=s->f+i;
            if(f->color==r->name){if(!s->b.attach(s->b.ctx,f->handle,IM2_OES_COLOR,IM2_OES_RGBA8,0)){restore(s);return IM2_OES_OPERATION;}f->color=0;}
            if(f->depth==r->name){if(!s->b.attach(s->b.ctx,f->handle,IM2_OES_DEPTH,IM2_OES_DEPTH16,0)){restore(s);return IM2_OES_OPERATION;}f->depth=0;}
        }
        if(s->bound_r==r->name)s->bound_r=0;
        if(r->handle)s->b.delete_storage(s->b.ctx,r->format,r->handle);
        *r=(Renderbuffer){0};
    }
    restore(s);return 0;
}
int im2_oes_drawable(Im2Oes *s,uint32_t *framebuffer,uint32_t *texture){
    if(!s || !framebuffer || !texture)return 0;
    Renderbuffer *r=rb(s,s->bound_r);if(!r || !r->handle || (r->format!=IM2_OES_RGBA8 && r->format!=IM2_OES_RGB565))return 0;
    for(int i=0;i<LIMIT;i++)if(s->f[i].name && s->f[i].color==r->name && status(s,s->f+i)==IM2_OES_COMPLETE){*framebuffer=s->f[i].handle;*texture=r->handle;return 1;}
    return 0;
}
