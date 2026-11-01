/* SPDX-License-Identifier: MIT. Reuses the project's packed Darwin formatter. */
#include "console_printf.h"
#include "darwin_printf.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#define OUTPUT_LIMIT (1024*1024)
void *im2_console_stdout(void){return stdout;}
int im2_console_printf(const char *format,...){
 if(!format){errno=22;return -1;}
 va_list arguments,measure;va_start(arguments,format);va_copy(measure,arguments);
 words first={&measure,NULL};int length=darwin_vformat_strict(NULL,0,format,&first);va_end(measure);
 if(length<0 || length>OUTPUT_LIMIT){va_end(arguments);errno=22;return -1;}
 if(!length){va_end(arguments);return 0;}
 char *output=malloc((size_t)length+1);
 if(!output){va_end(arguments);errno=12;return -1;}
 words second={&arguments,NULL};int actual=darwin_vformat_strict(output,(size_t)length+1,format,&second);va_end(arguments);
 if(actual!=length){free(output);errno=22;return -1;}
 size_t written=fwrite(output,1,(size_t)length,stdout);free(output);
 if(written!=(size_t)length){errno=5;return -1;}
 return length;
}
