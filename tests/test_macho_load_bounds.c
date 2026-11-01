/* Real macho_load execution with synthetic files and failure-injected host VM.
 * These host allocations do NOT establish Vita permissions or cache behavior. */
#define _GNU_SOURCE
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdarg.h>
#include "../vendor/ios-loader-boilerplate/source/macho.c"
static int allocs, frees, fail_alloc, fail_base, writes;
static int mallocs, fail_malloc;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size) { return ++mallocs==fail_malloc?NULL:__real_malloc(size); }
static void *blocks[3]; static size_t sizes[3];
SceUID sceKernelAllocMemBlockForVM(const char *name,size_t size) {
    (void)name; ++allocs;
    if (allocs == fail_alloc) return -7;
    assert(allocs <= 2 && size && size <= IM2_VM_MAX_SPAN && !(size & 4095));
    void *p=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    assert(p!=MAP_FAILED); blocks[allocs]=p; sizes[allocs]=size; return allocs;
}
int sceKernelGetMemBlockBase(SceUID id,void **out) {
    if(id==fail_base)return -8;
    *out=blocks[id];return 0;
}
int sceKernelFreeMemBlock(SceUID id) {
    assert(id>0 && id<=2 && blocks[id]);assert(!munmap(blocks[id],sizes[id]));blocks[id]=NULL;frees++;return 0;
}
void kuKernelCpuUnrestrictedMemcpy(void *dst,const void *src,size_t n){writes++;memcpy(dst,src,n);}
void kuKernelFlushCaches(void *p,size_t n){(void)p;(void)n;}
void l_info(const char *fmt,...){(void)fmt;}
void l_warn(const char *fmt,...){(void)fmt;}
void l_error(const char *fmt,...){(void)fmt;}
void fatal_error(const char *fmt,...){(void)fmt;abort();}
void ml_align_call(void){}
uint32_t call_guest(void *fn,uint32_t a,uint32_t b,uint32_t c,uint32_t d){(void)fn;(void)a;(void)b;(void)c;(void)d;abort();}
const import_entry *imports_lookup(const char *name){(void)name;return NULL;}
size_t im2_objc_registration_checkpoint(void){return 0;}
int im2_objc_registration_rollback(size_t n){assert(!n);return 0;}
int im2_objc_register_guest_list(uint32_t a,size_t n,Im2GuestRead r,void *c,char *e,size_t s){(void)a;(void)n;(void)r;(void)c;(void)e;(void)s;abort();}
int im2_objc_validate_guest_refs(uint32_t a,size_t n,Im2GuestRead r,void *c,char *e,size_t s){(void)a;(void)n;(void)r;(void)c;(void)e;(void)s;abort();}
int im2_foundation_register_cfstrings(uint32_t a,size_t n,Im2GuestRead r,void *c,char *e,size_t s){(void)a;(void)n;(void)r;(void)c;(void)e;(void)s;abort();}
int im2_cfstrings_clear(void){abort();}
static void fixture(const char *path,segment_command text,segment_command data) {
    uint8_t file[4096]={0};uint32_t header[]={0xfeedface,12,6,2,2,112,0};
    memcpy(file,header,sizeof header);memcpy(file+28,&text,56);memcpy(file+84,&data,56);
    /* Rebase one real word in __DATA; no machine code is executed. */
    uint32_t pointer=0x1000;memcpy(file+512,&pointer,4);
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(file,1,sizeof file,f)==sizeof file);assert(!fclose(f));
}
static void rejected(const char *path,const char *rbs,segment_command t,segment_command d,int alloc_fail,int base_fail,int want_alloc,int want_free) {
    allocs=frees=writes=0;fail_alloc=alloc_fail;fail_base=base_fail;
    fixture(path,t,d);assert(macho_load(path,rbs)==-1);
    assert(allocs==want_alloc && frees==want_free && writes==0);
    assert(!guest_exec_ready && !macho_get_runtime_slide() && !macho_sym("_main"));
    assert(image_blk==-1 && thunk_blk==-1 && !text_buf && !symtab && !strtab);
}
int main(void){
    char path[]="/tmp/im2-macho-test-XXXXXX",rbs[]="/tmp/im2-rbs-test-XXXXXX";
    int fd=mkstemp(path);assert(fd>=0);close(fd);fd=mkstemp(rbs);assert(fd>=0);
    uint32_t table[]={0x31534252,1,0x4000};assert(write(fd,table,sizeof table)==sizeof table);close(fd);
    segment_command t={.cmd=1,.cmdsize=56,.segname="__TEXT",.vmaddr=0x1000,.vmsize=4096,.filesize=512};
    segment_command d={.cmd=1,.cmdsize=56,.segname="__DATA",.vmaddr=0x4000,.vmsize=4096,.fileoff=512,.filesize=4};
    segment_command bad=t;
    const uint32_t overflows[]={0xfffff001,0xffffffff,0xfffff000,0};
    for(unsigned i=0;i<sizeof overflows/sizeof *overflows;i++) {bad=t;bad.vmsize=overflows[i];rejected(path,rbs,bad,d,0,0,0,0);}
    bad=t;bad.filesize=4097;rejected(path,rbs,bad,d,0,0,0,0);
    bad=t;bad.vmsize=0x3001;rejected(path,rbs,bad,d,0,0,0,0);
    bad=t;bad.fileoff=UINT32_MAX;rejected(path,rbs,bad,d,0,0,0,0);
    bad=d;bad.vmsize=UINT32_MAX;rejected(path,rbs,t,bad,0,0,0,0);
    bad=d;bad.vmsize=0;rejected(path,rbs,t,bad,0,0,0,0);
    bad=d;bad.filesize=4097;rejected(path,rbs,t,bad,0,0,0,0);
    bad=d;bad.fileoff=UINT32_MAX;rejected(path,rbs,t,bad,0,0,0,0);
    bad=d;bad.vmaddr=0xfffff000;rejected(path,rbs,t,bad,0,0,0,0);
    bad=d;bad.vmaddr=0x1000;rejected(path,rbs,t,bad,0,0,0,0);
    bad=d;bad.vmaddr=IM2_VM_MAX_SPAN+0x1000;rejected(path,rbs,t,bad,0,0,0,0);
    bad=d;bad.vmaddr++;rejected(path,rbs,t,bad,0,0,0,0);
    rejected(path,rbs,t,d,1,0,1,0);rejected(path,rbs,t,d,0,1,1,1);
    rejected(path,rbs,t,d,2,0,2,1);rejected(path,rbs,t,d,0,2,2,2);
    rejected(path,"/tmp/im2-required-rbs-does-not-exist",t,d,0,0,2,2);
    for(int failure=1;failure<=3;failure++) {
        mallocs=0;fail_malloc=failure;
        rejected(path,rbs,t,d,0,0,failure==1?0:2,failure==1?0:2);
    }
    fail_malloc=0;
    allocs=frees=writes=0;fail_alloc=fail_base=0;fixture(path,t,d);
    assert(macho_load(path,rbs)==-2); /* Mapping success remains execution-gated. */
    assert(allocs==2 && !frees && writes==1 && !guest_exec_ready && macho_run_inits()==-1);
    uint32_t slide=macho_get_runtime_slide();
    assert(sizes[1]==0x4000); /* Image and gap occupy one allocation, thunks a second. */
    assert((uintptr_t)blocks[1]==(uint64_t)slide+t.vmaddr);
    assert((uintptr_t)blocks[1]+(d.vmaddr-t.vmaddr)==(uint64_t)slide+d.vmaddr);
    assert(*(uint32_t *)((uint8_t *)blocks[1]+0x3000)==slide+0x1000);
    for(size_t i=4;i<4096;i++)assert(!((uint8_t *)blocks[1])[0x3000+i]);
    assert(macho_load(path,rbs)==-1 && macho_get_runtime_slide()==slide && allocs==2);
    /* Test-only teardown of fixture without registered borrowed guest identities. */
    free(image_raw);image_raw=NULL;symtab=NULL;strtab=NULL;
    sceKernelFreeMemBlock(thunk_blk);sceKernelFreeMemBlock(image_blk);
    unlink(path);unlink(rbs);puts("PASS: real macho_load malformed sizes/ranges, VM failure cleanup, contiguous slide and closed execution gate (host only)");
}
