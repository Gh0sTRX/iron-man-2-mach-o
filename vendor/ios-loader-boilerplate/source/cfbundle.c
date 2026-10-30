/* SPDX-License-Identifier: MIT. Partial CFBundle/CFURL bridge over existing owned Foundation objects. */
#include "foundation.h"
#include "url.h"
#include "imports.h"
#include <string.h>
static void *bundle_main(void){return im2_bundle_main();} /* Get rule: borrowed. */
static void *bundle_copy_url(void *bundle){
 const char *path=im2_bundle_path(bundle);
 return path?im2_url_create_file(path,0):NULL; /* Copy rule: +1, no autorelease pool required. */
}
static void *url_copy_path(void *url,int32_t style){
 const char *path=im2_url_local_file_path(url);
 return path && style==0?im2_string_create(path,0):NULL; /* POSIX style only. */
}
static uint32_t url_file_representation(void *url,uint32_t resolve_base,uint8_t *buffer,int32_t capacity){
 (void)resolve_base; /* Supported URLs are already absolute; no relative/base objects exist. */
 const char *path=im2_url_local_file_path(url);if(!path || !buffer || capacity<=0)return 0;
 size_t bytes=strlen(path);if(bytes>=(size_t)capacity)return 0;
 memcpy(buffer,path,bytes+1);return 1;
}
const import_entry cfbundle_imports[]={
 F(CFBundleGetMainBundle,bundle_main),
 F(CFBundleCopyBundleURL,bundle_copy_url),
 /* An iOS flat .app stores resources at its bundle root. No macOS Contents layout is inferred. */
 F(CFBundleCopyResourcesDirectoryURL,bundle_copy_url),
 F(CFURLCopyFileSystemPath,url_copy_path),
 F(CFURLGetFileSystemRepresentation,url_file_representation)
};
const int cfbundle_imports_count=sizeof(cfbundle_imports)/sizeof(*cfbundle_imports);
