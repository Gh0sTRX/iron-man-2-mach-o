/* SPDX-License-Identifier: MIT. No game or Apple implementation code. */
#include "macho_init.h"
#include <stdlib.h>
#include <string.h>
int im2_run_initializers(const uint32_t *functions,size_t count,uint32_t base,
    uint32_t size,Im2InitCall call,void *ctx) {
    if((count && (!functions || !call)) || count>1000000 || !size ||
       (uint64_t)base+size>UINT32_MAX) return -1;
    if(!count)return 0;
    uint32_t *snapshot=malloc(count*sizeof(*snapshot));if(!snapshot)return -1;
    memcpy(snapshot,functions,count*sizeof(*snapshot));
    for(size_t i=0;i<count;i++) {
        uint32_t fn=snapshot[i],pc=fn&~1u;
        if(!fn) continue; /* Preserve zero/padding slots without calling address zero. */
        uint32_t width=fn&1u?2u:4u;
        if(pc<base || pc%width || (uint64_t)pc+width>(uint64_t)base+size) {free(snapshot);return -1;}
    }
    for(size_t i=0;i<count;i++) if(snapshot[i]) call(ctx,snapshot[i]);
    free(snapshot);return 0;
}
