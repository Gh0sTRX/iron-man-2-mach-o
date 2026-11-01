/* SPDX-License-Identifier: MIT. Real directories/bytes, shared virtual cwd and bounds. */
#define _POSIX_C_SOURCE 200809L
#include "fs_path.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char directory[IM2_FS_PATH_MAX+1];
static void real_file(const char *path,const char *data){FILE *f=fopen(path,"wb");assert(f && fwrite(data,1,strlen(data),f)==strlen(data) && !fclose(f));}
static void *worker(void *unused){(void)unused;char value[IM2_FS_PATH_MAX+1];assert(im2_fs_getcwd(value,sizeof(value)) && !strcmp(value,directory));assert(!im2_fs_chdir("../sub"));return NULL;}
int main(void){
 char original[IM2_FS_PATH_MAX+1],value[IM2_FS_PATH_MAX+1],path[IM2_FS_PATH_MAX+1];
 assert(getcwd(original,sizeof(original)));assert(!im2_fs_getcwd(value,sizeof(value)) && errno==22);
 char root[]="/tmp/im2-fs-XXXXXX";assert(mkdtemp(root));
 snprintf(path,sizeof(path),"%s/sub",root);assert(!mkdir(path,0700));
 snprintf(path,sizeof(path),"%s/other",root);assert(!mkdir(path,0700));
 snprintf(path,sizeof(path),"%s/sub/game.dat",root);real_file(path,"actual game bytes fixture");
 snprintf(path,sizeof(path),"%s/global.txt",root);real_file(path,"root");
 assert(!im2_fs_init(root) && !im2_fs_init(root));assert(im2_fs_init("/different")==-1 && errno==22);
 snprintf(directory,sizeof(directory),"%s/sub/game.dat",root);
 assert(im2_fs_path("sub/game.dat",value,sizeof(value)) && !strcmp(value,directory));
 FILE *f=fopen(value,"rb");char content[64]={0};assert(f && fread(content,1,sizeof(content)-1,f)==25 && !fclose(f));assert(!strcmp(content,"actual game bytes fixture"));
 assert(!im2_fs_chdir("sub"));snprintf(directory,sizeof(directory),"%s/sub",root);
 assert(im2_fs_getcwd(value,sizeof(value)) && !strcmp(value,directory));
 assert(im2_fs_path("/global.txt",value,sizeof(value)) && !strcmp(value,path));
 strcpy(value,"guard");assert(!im2_fs_getcwd(value,2) && errno==34 && !strcmp(value,"guard"));
 assert(!im2_fs_getcwd(value,0) && errno==22);char *allocated=im2_fs_getcwd(NULL,0);assert(allocated && !strcmp(allocated,directory));free(allocated);
 assert(im2_fs_chdir("missing")==-1 && errno==2);
 assert(im2_fs_chdir("game.dat")==-1 && errno==20);
 assert(im2_fs_chdir("game.dat/..")==-1 && errno==20);
 assert(im2_fs_getcwd(value,sizeof(value)) && !strcmp(value,directory));
 assert(!im2_fs_chdir("../other/./"));snprintf(directory,sizeof(directory),"%s/other",root);
 pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL) && !pthread_join(thread,NULL));
 snprintf(directory,sizeof(directory),"%s/sub",root);assert(im2_fs_getcwd(value,sizeof(value)) && !strcmp(value,directory));
 strcpy(value,"guard");assert(!im2_fs_path("game.dat",value,2) && errno==34 && !strcmp(value,"guard"));
 assert(!im2_fs_path("",value,sizeof(value)) && errno==2);
 assert(!im2_fs_path(NULL,value,sizeof(value)) && errno==14);
 char oversized[IM2_FS_PATH_MAX+2];memset(oversized,'x',sizeof(oversized)-1);oversized[sizeof(oversized)-1]=0;
 assert(!im2_fs_path(oversized,value,sizeof(value)) && errno==63);
 assert(im2_fs_path("ur0:/data/asset",value,sizeof(value)) && !strcmp(value,"ur0:/data/asset"));
 assert(im2_fs_path("app0:font.bin",value,sizeof(value)) && !strcmp(value,"app0:font.bin"));
 assert(im2_fs_getcwd(value,sizeof(value)));assert(!im2_fs_chdir(value)); /* getcwd roundtrip */
 assert(getcwd(value,sizeof(value)) && !strcmp(value,original)); /* Never changed native cwd. */
 snprintf(path,sizeof(path),"%s/sub/game.dat",root);assert(!unlink(path));snprintf(path,sizeof(path),"%s/global.txt",root);assert(!unlink(path));
 snprintf(path,sizeof(path),"%s/sub",root);assert(!rmdir(path));snprintf(path,sizeof(path),"%s/other",root);assert(!rmdir(path));assert(!rmdir(root));
 puts("PASS: real cwd directory validation/relative bytes, process-wide thread visibility, absolute/root/mount mapping, unchanged failure state, allocated/bounded getcwd and no native cwd changes");
}
