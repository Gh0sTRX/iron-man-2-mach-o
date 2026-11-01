/* SPDX-License-Identifier: MIT. Independent Vita/iOS path adapter.
 * Absolute iOS paths retain the existing data-root mapping. Relative paths use
 * a synchronized virtual cwd. Mount paths are explicit native paths. Directory
 * changes are checked by real stat before commit. Canonical cwd is lexical;
 * Unix symlink traversal semantics are not implemented by this Vita subset.
 */
#include "fs_path.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
const uint32_t im2_fs_stat_layout[3]={sizeof(struct stat),offsetof(struct stat,st_mode),sizeof(((struct stat *)0)->st_mode)};
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static char root[IM2_FS_PATH_MAX+1],cwd[IM2_FS_PATH_MAX+1];
static int ready;
static int error(int value){errno=value;return -1;}
static size_t length(const char *s){size_t n=0;while(n<=IM2_FS_PATH_MAX && s[n])n++;return n;}
static size_t mount(const char *s){return !strncmp(s,"ux0:",4) || !strncmp(s,"ur0:",4)?4:!strncmp(s,"app0:",5)?5:0;}
static int normalized(const char *path,char *out){
 size_t n=length(path);if(n>IM2_FS_PATH_MAX)return error(63); /* Darwin ENAMETOOLONG */
 size_t anchor=mount(path),used=anchor,pos=anchor;
 if(!anchor){if(*path!='/')return error(22);anchor=used=pos=1;}
 memcpy(out,path,anchor);
 while(pos<n){
  while(pos<n && path[pos]=='/')pos++;
  size_t start=pos;while(pos<n && path[pos]!='/')pos++;size_t count=pos-start;
  if(!count || (count==1 && path[start]=='.'))continue;
  if(count==2 && path[start]=='.' && path[start+1]=='.'){
   while(used>anchor && out[used-1]!='/')used--;
   if(used>anchor)used--;
  }else{
   if(used>anchor)out[used++]='/';
   memcpy(out+used,path+start,count);used+=count;
  }
 }
 out[used]=0;return 0;
}
int im2_fs_init(const char *data_root){
 if(!data_root || !*data_root)return error(22);
 char value[IM2_FS_PATH_MAX+1];if(normalized(data_root,value))return -1;
 pthread_mutex_lock(&lock);
 if(ready){int same=!strcmp(root,value);pthread_mutex_unlock(&lock);return same?0:error(22);}
 memcpy(root,value,strlen(value)+1);memcpy(cwd,value,strlen(value)+1);ready=1;
 pthread_mutex_unlock(&lock);return 0;
}
const char *im2_fs_path(const char *path,char *out,size_t capacity){
 if(!path || !out){error(14);return NULL;}
 size_t n=length(path);if(!n){error(2);return NULL;}if(n>IM2_FS_PATH_MAX){error(63);return NULL;}
 char base[IM2_FS_PATH_MAX+1],result[IM2_FS_PATH_MAX+1];
 pthread_mutex_lock(&lock);
 if(!ready){pthread_mutex_unlock(&lock);error(22);return NULL;}
 size_t root_length=strlen(root);
 int native=mount(path)!=0 || (root_length>1 && !strncmp(path,root,root_length) && (path[root_length]==0 || path[root_length]=='/'));
 memcpy(base,path[0]=='/'?root:cwd,strlen(path[0]=='/'?root:cwd)+1);
 pthread_mutex_unlock(&lock);
 if(native)memcpy(result,path,n+1);
 else{
  const char *relative=path;while(*relative=='/')relative++;
  size_t a=strlen(base),b=strlen(relative);
  int separator=a && base[a-1]!='/' && base[a-1]!=':';
  if(a+(size_t)separator+b>IM2_FS_PATH_MAX){error(63);return NULL;}
  memcpy(result,base,a);if(separator)result[a++]='/';memcpy(result+a,relative,b+1);
 }
 size_t size=strlen(result)+1;
 if(capacity<size){error(34);return NULL;}
 memcpy(out,result,size);return out;
}
static int stat_error(int value){
 if(value==ENAMETOOLONG)return 63;
 if(value==ELOOP)return 62;
 if(value==ENOTSUP)return 45;
 if(value==ENOENT || value==ENOTDIR || value==EACCES || value==EPERM || value==EIO || value==ENOMEM)return value;
 return 5;
}
int im2_fs_chdir(const char *path){
 char resolved[IM2_FS_PATH_MAX+1],value[IM2_FS_PATH_MAX+1];
 if(!im2_fs_path(path,resolved,sizeof(resolved)))return -1;
 struct stat st;if(stat(resolved,&st))return error(stat_error(errno));
 if(!S_ISDIR(st.st_mode))return error(20);
 if(normalized(resolved,value))return -1;
 pthread_mutex_lock(&lock);memcpy(cwd,value,strlen(value)+1);pthread_mutex_unlock(&lock);return 0;
}
char *im2_fs_getcwd(char *out,size_t capacity){
 char value[IM2_FS_PATH_MAX+1];
 pthread_mutex_lock(&lock);
 if(!ready){pthread_mutex_unlock(&lock);error(22);return NULL;}
 memcpy(value,cwd,strlen(cwd)+1);pthread_mutex_unlock(&lock);
 size_t size=strlen(value)+1;
 if(out && !capacity){error(22);return NULL;}
 if(capacity && capacity<size){error(34);return NULL;}
 if(!out){out=malloc(capacity?capacity:size);if(!out){error(12);return NULL;}}
 memcpy(out,value,size);return out;
}
