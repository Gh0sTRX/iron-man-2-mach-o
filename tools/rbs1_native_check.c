/* Independent host-side RBS1 format verifier. Compile with cc -std=c11 -Wall -Wextra -Werror -O2. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int cmp(const void *a,const void *b){uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return (x>y)-(x<y);}
int main(int argc,char **argv){
 if(argc!=2){fprintf(stderr,"usage: %s rebase.bin\n",argv[0]);return 2;}
 FILE *f=fopen(argv[1],"rb");if(!f){perror("open");return 2;}
 unsigned char h[8];if(fread(h,1,8,f)!=8||memcmp(h,"RBS1",4)){fprintf(stderr,"BAD HEADER\n");fclose(f);return 1;}
 uint32_t n=u32(h+4);if(!n||n>1000000){fprintf(stderr,"BAD COUNT\n");fclose(f);return 1;}
 uint32_t *a=malloc((size_t)n*sizeof(*a));if(!a){fclose(f);return 2;}
 for(uint32_t i=0;i<n;i++){unsigned char b[4];if(fread(b,1,4,f)!=4){fprintf(stderr,"TRUNCATED\n");free(a);fclose(f);return 1;}a[i]=u32(b);}
 int extra=fgetc(f);fclose(f);if(extra!=EOF){fprintf(stderr,"TRAILING DATA\n");free(a);return 1;}
 qsort(a,n,sizeof(*a),cmp);
 for(uint32_t i=0;i<n;i++)if((a[i]&3)||(i&&a[i]==a[i-1])||a[i]<0x1000||a[i]>0x4fc000-4){fprintf(stderr,"BAD SLOT %08x\n",a[i]);free(a);return 1;}
 printf("PASS RBS1 count=%u first=0x%08x last=0x%08x\n",n,a[0],a[n-1]);free(a);return 0;
}
