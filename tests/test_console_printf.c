/* SPDX-License-Identifier: MIT. Packed words, strict formats and exact stdout bytes. */
#include "console_printf.h"
#include "darwin_printf.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char output[4096];static size_t count;static int short_write,oom;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t n){return oom?NULL:__real_malloc(n);}
size_t __wrap_fwrite(const void *bytes,size_t size,size_t n,FILE *stream){
 assert(stream==stdout && size==1 && n<=sizeof(output));
 size_t result=short_write?n-1:n;memcpy(output,bytes,result);count=result;return result;
}
static const char text[]="fixture";
static const char *object(const void *value){return (uintptr_t)value==0x1234?text:NULL;}
int main(void){
 assert((uintptr_t)text<=UINT32_MAX);assert(im2_console_stdout()==stdout);
 assert(im2_console_printf("ready\n")==6 && count==6 && !memcmp(output,"ready\n",6));
 union {double d;uint64_t bits;} real={.d=1.25};
 const char expected[]="fixture -7 1.25 123456789abcdef0";
 assert(im2_console_printf("%s %d %.2f %llx",(uint32_t)(uintptr_t)text,(uint32_t)-7,
  (uint32_t)real.bits,(uint32_t)(real.bits>>32),0x9abcdef0u,0x12345678u)==(int)strlen(expected));
 assert(count==strlen(expected) && !memcmp(output,expected,count));
 assert(im2_console_printf("a%cb",0u)==3 && count==3 && output[0]=='a' && !output[1] && output[2]=='b');
 darwin_set_object_string_resolver(object);assert(im2_console_printf("%@",0x1234u)==7 && !memcmp(output,text,7));
 assert(im2_console_printf("%@",0x9876u)==-1 && errno==22);
 uint32_t guard=0xabcdef01;assert(im2_console_printf("%n",(uint32_t)(uintptr_t)&guard)==-1 && errno==22 && guard==0xabcdef01);
 assert(im2_console_printf("bad%") == -1 && im2_console_printf("%ls",0u)==-1 && im2_console_printf("%y")==-1);
 assert(im2_console_printf("%1048577d",1u)==-1 && errno==22);
 short_write=1;assert(im2_console_printf("short")==-1 && errno==5);short_write=0;
 oom=1;assert(im2_console_printf("allocation")==-1 && errno==12);oom=0;
 assert(im2_console_printf("")==0 && im2_console_printf(NULL)==-1 && errno==22);
 puts("PASS: packed printf integer/double/64-bit/object values, embedded NUL/stdout bytes, strict rejection, write/OOM/budget errors");
}
