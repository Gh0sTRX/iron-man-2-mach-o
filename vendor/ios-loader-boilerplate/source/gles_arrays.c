/* SPDX-License-Identifier: MIT. Byte-stride-correct GL_FIXED conversion.
 * Caller must supply valid client memory for referenced vertices and indices.
 * No buffer-object offsets, arbitrary pointer validity or GPU proof is claimed.
 */
#include "gles_arrays.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define ENUM 0x500u
#define VALUE 0x501u
#define OP 0x502u
#define OOM 0x505u
#define BYTE 0x1400u
#define UBYTE 0x1401u
#define SHORT 0x1402u
#define USHORT 0x1403u
#define FLOAT 0x1406u
#define FIXED 0x140cu
#define TEX0 0x84c0u
#define VERTICES 65536u
#define INDICES 1048576u
typedef struct {int size,stride,enabled,ready;uint32_t type;const void *ptr;float *converted;size_t capacity;} Array;
struct Im2Arrays {Im2ArrayBackend backend;Array arrays[IM2_ARRAY_SLOTS];unsigned unit;};
static int cap_slot(const Im2Arrays *a,uint32_t cap){
 switch(cap){case 0x8074:return IM2_ARRAY_VERTEX;case 0x8076:return IM2_ARRAY_COLOR;case 0x8075:return IM2_ARRAY_NORMAL;case 0x8078:return IM2_ARRAY_TEX0+(int)a->unit;default:return -1;}
}
static void apply(Im2Arrays *a,unsigned slot){
 Array *p=&a->arrays[slot];int fixed=p->type==FIXED;
 if(slot>=IM2_ARRAY_TEX0)a->backend.client_unit(a->backend.context,TEX0+slot-IM2_ARRAY_TEX0);
 a->backend.pointer(a->backend.context,slot,p->size,fixed?FLOAT:p->type,fixed?0:p->stride,
                    fixed?(p->ready?p->converted:NULL):p->ptr);
 if(slot>=IM2_ARRAY_TEX0)a->backend.client_unit(a->backend.context,TEX0+a->unit);
}
Im2Arrays *im2_arrays_create(Im2ArrayBackend b){
 if(!b.pointer || !b.enable || !b.client_unit || !b.draw_arrays || !b.draw_elements)return NULL;
 Im2Arrays *a=calloc(1,sizeof(*a));if(!a)return NULL;a->backend=b;
 for(unsigned s=0;s<IM2_ARRAY_SLOTS;s++){a->arrays[s].size=s==IM2_ARRAY_NORMAL?3:4;a->arrays[s].type=FLOAT;}
 return a;
}
void im2_arrays_destroy(Im2Arrays *a){if(a){for(unsigned s=0;s<IM2_ARRAY_SLOTS;s++)free(a->arrays[s].converted);free(a);}}
uint32_t im2_arrays_set(Im2Arrays *a,unsigned slot,int size,uint32_t type,int stride,const void *ptr){
 if(!a)return OP;
 if(slot>=IM2_ARRAY_SLOTS)return ENUM;
 if(stride<0 || (slot==IM2_ARRAY_COLOR?size!=4:slot==IM2_ARRAY_NORMAL?size!=3:(size<2 || size>4)))return VALUE;
 if(type!=FIXED && type!=FLOAT && (slot==IM2_ARRAY_COLOR?type!=UBYTE:(type!=BYTE && type!=SHORT)))return ENUM;
 Array *p=&a->arrays[slot];p->size=size;p->type=type;p->stride=stride;p->ptr=ptr;p->ready=0;apply(a,slot);return 0;
}
uint32_t im2_arrays_enable(Im2Arrays *a,uint32_t cap,int on){
 if(!a)return OP;
 int slot=cap_slot(a,cap);if(slot<0)return ENUM;
 a->arrays[slot].enabled=!!on;
 if(slot>=IM2_ARRAY_TEX0)a->backend.client_unit(a->backend.context,TEX0+a->unit);
 a->backend.enable(a->backend.context,(unsigned)slot,!!on);return 0;
}
uint32_t im2_arrays_client_unit(Im2Arrays *a,uint32_t unit){
 if(!a)return OP;
 if(unit<TEX0 || unit>TEX0+1)return ENUM;
 a->unit=unit-TEX0;a->backend.client_unit(a->backend.context,unit);return 0;
}
uint32_t im2_arrays_unit(const Im2Arrays *a){return TEX0+(a?a->unit:0);}
uint32_t im2_arrays_get_pointer(const Im2Arrays *a,uint32_t pname,void **out){
 if(!a)return OP;
 if(!out)return VALUE;
 int slot;
 switch(pname){case 0x8090:slot=IM2_ARRAY_VERTEX;break;case 0x8092:slot=IM2_ARRAY_COLOR;break;case 0x8091:slot=IM2_ARRAY_NORMAL;break;case 0x8098:slot=IM2_ARRAY_TEX0+(int)a->unit;break;default:return ENUM;}
 *out=(void *)a->arrays[slot].ptr;return 0;
}
int im2_arrays_parameter(const Im2Arrays *a,uint32_t pname,int *out){
 if(!a || !out)return 0;
 int slot=cap_slot(a,pname);if(slot>=0){*out=a->arrays[slot].enabled;return 1;}
 if(pname==0x84e1){*out=(int)(TEX0+a->unit);return 1;}
 int field;
 switch(pname){
 case 0x807a:slot=IM2_ARRAY_VERTEX;field=0;break;case 0x807b:slot=IM2_ARRAY_VERTEX;field=1;break;case 0x807c:slot=IM2_ARRAY_VERTEX;field=2;break;
 case 0x8081:slot=IM2_ARRAY_COLOR;field=0;break;case 0x8082:slot=IM2_ARRAY_COLOR;field=1;break;case 0x8083:slot=IM2_ARRAY_COLOR;field=2;break;
 case 0x807e:slot=IM2_ARRAY_NORMAL;field=1;break;case 0x807f:slot=IM2_ARRAY_NORMAL;field=2;break;
 case 0x8088:slot=IM2_ARRAY_TEX0+(int)a->unit;field=0;break;case 0x8089:slot=IM2_ARRAY_TEX0+(int)a->unit;field=1;break;case 0x808a:slot=IM2_ARRAY_TEX0+(int)a->unit;field=2;break;
 default:return 0;
 }
 const Array *p=&a->arrays[slot];*out=field==0?p->size:field==1?(int)p->type:p->stride;return 1;
}
static size_t component_bytes(uint32_t type){return type==BYTE || type==UBYTE?1:type==SHORT?2:4;}
static int span(const Array *p,unsigned last){
 size_t bytes=component_bytes(p->type)*(size_t)p->size;
 uint64_t step=p->stride?(uint32_t)p->stride:bytes;
 uint64_t end=(uint64_t)last*step+bytes-1;
 return p->ptr && end<=UINTPTR_MAX && (uintptr_t)p->ptr<=UINTPTR_MAX-(uintptr_t)end;
}
static uint32_t prepare(Im2Arrays *a,unsigned end){
 for(unsigned s=0;s<IM2_ARRAY_SLOTS;s++){
  Array *p=&a->arrays[s];if(!p->enabled)continue;
  if(!span(p,end-1))return OP;
  if(p->type==FIXED){
   size_t need=(size_t)end*(size_t)p->size;
   if(need>p->capacity){float *grown=realloc(p->converted,need*sizeof(float));if(!grown)return OOM;memset(grown+p->capacity,0,(need-p->capacity)*sizeof(float));p->converted=grown;p->capacity=need;}
   p->ready=0;
  }
 }
 return 0;
}
static void convert(Array *p,unsigned index){
 size_t step=p->stride?(size_t)p->stride:(size_t)p->size*4;
 const unsigned char *src=(const unsigned char *)p->ptr+(size_t)index*step;
 for(int c=0;c<p->size;c++){int32_t raw;memcpy(&raw,src+(size_t)c*4,4);p->converted[(size_t)index*(size_t)p->size+(size_t)c]=(float)raw*(1.f/65536.f);}
}
static void bind_enabled(Im2Arrays *a){
 for(unsigned s=0;s<IM2_ARRAY_SLOTS;s++)if(a->arrays[s].enabled){a->arrays[s].ready=1;apply(a,s);}
}
uint32_t im2_arrays_draw(Im2Arrays *a,uint32_t mode,int first,int count){
 if(!a)return OP;
 if(mode>6)return ENUM;
 if(first<0 || count<0)return VALUE;
 if(!count)return 0;
 uint64_t end=(uint64_t)(unsigned)first+(unsigned)count;if(end>VERTICES)return OOM;
 if(!a->arrays[IM2_ARRAY_VERTEX].enabled)return 0;
 uint32_t e=prepare(a,(unsigned)end);if(e)return e;
 for(unsigned s=0;s<IM2_ARRAY_SLOTS;s++)if(a->arrays[s].enabled && a->arrays[s].type==FIXED)
  for(unsigned i=(unsigned)first;i<(unsigned)end;i++)convert(&a->arrays[s],i);
 bind_enabled(a);a->backend.draw_arrays(a->backend.context,mode,first,count);return 0;
}
static unsigned element(const unsigned char *p,uint32_t type,unsigned i){if(type==UBYTE)return p[i];uint16_t v;memcpy(&v,p+(size_t)i*2,2);return v;}
uint32_t im2_arrays_elements(Im2Arrays *a,uint32_t mode,int count,uint32_t type,const void *indices){
 if(!a)return OP;
 if(mode>6)return ENUM;
 if(count<0)return VALUE;
 if(type!=UBYTE && type!=USHORT)return ENUM;
 if(!count)return 0;
 if((unsigned)count>INDICES)return OOM;
 size_t bytes=(size_t)(unsigned)count*(type==USHORT?2:1);
 if(!indices || (uintptr_t)indices>UINTPTR_MAX-(bytes-1))return OP;
 if(!a->arrays[IM2_ARRAY_VERTEX].enabled)return 0;
 unsigned largest=0;const unsigned char *p=indices;
 for(unsigned i=0;i<(unsigned)count;i++){unsigned v=element(p,type,i);if(v>largest)largest=v;}
 uint32_t e=prepare(a,largest+1);if(e)return e;
 for(unsigned s=0;s<IM2_ARRAY_SLOTS;s++)if(a->arrays[s].enabled && a->arrays[s].type==FIXED)
  for(unsigned i=0;i<(unsigned)count;i++)convert(&a->arrays[s],element(p,type,i));
 bind_enabled(a);a->backend.draw_elements(a->backend.context,mode,count,type,indices);return 0;
}
void im2_arrays_restore(Im2Arrays *a){
 if(!a)return;
 for(unsigned s=0;s<IM2_ARRAY_SLOTS;s++){
  apply(a,s);
  if(s>=IM2_ARRAY_TEX0)a->backend.client_unit(a->backend.context,TEX0+s-IM2_ARRAY_TEX0);
  a->backend.enable(a->backend.context,s,a->arrays[s].enabled);
 }
 a->backend.client_unit(a->backend.context,TEX0+a->unit);
}
