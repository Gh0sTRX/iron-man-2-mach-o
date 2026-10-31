// SPDX-License-Identifier: MIT
#define _GLIBCXX_USE_CXX11_ABI 0
#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <sys/wait.h>
#include <unistd.h>
extern "C" {
#include "imports.h"
void fatal_error(const char *, ...) { _Exit(73); }
}
using S=std::string;
template<class T> static T fn(const char *n) {
 for(int i=0;i<legacy_string_imports_count;i++)
  if(!std::strcmp(n,legacy_string_imports[i].name)) {
   assert(legacy_string_imports[i].data==0);return reinterpret_cast<T>(legacy_string_imports[i].addr);
  }
 std::abort();
}
#define FN(type,name) fn<type>("__" name)
int main() {
 using Ctor=S*(*)(S*,const char*,const void*);using CtorN=S*(*)(S*,const char*,uint32_t,const void*);
 using Copy=S*(*)(S*,const S*);using Destroy=void(*)(S*);
 auto ctor=FN(Ctor,"ZNSsC1EPKcRKSaIcE");auto ctorn=FN(CtorN,"ZNSsC1EPKcmRKSaIcE");
 auto copy=FN(Copy,"ZNSsC1ERKSs");auto destroy=FN(Destroy,"ZNSsD2Ev");
 alignas(S) unsigned char a[sizeof(S)],b[sizeof(S)],c[sizeof(S)];
 S *one=ctor(reinterpret_cast<S*>(a),"Iron Man",nullptr);
 S *two=copy(reinterpret_cast<S*>(b),one);
 assert(one->c_str()==two->c_str()); // old COW sharing, then writable detach
 auto index=FN(char*(*)(S*,uint32_t),"ZNSsixEm");
 *index(two,0)='i';assert(*one=="Iron Man" && *two=="iron Man" && one->c_str()!=two->c_str());
 auto append=FN(S*(*)(S*,const char*,uint32_t),"ZNSs6appendEPKcm");
 assert(append(two," 2",2)==two && *two=="iron Man 2");
 auto assign=FN(S*(*)(S*,const char*,uint32_t),"ZNSs6assignEPKcm");
 const char bytes[]={'a',0,'b',char(0xc3),char(0xa9)};
 S *three=ctorn(reinterpret_cast<S*>(c),bytes,sizeof(bytes),nullptr);
 assert(three->size()==5 && !memcmp(three->data(),bytes,5));
 assign(two,three->data(),5);assert(*two==*three);
 auto appstr=FN(S*(*)(S*,const S*),"ZNSs6appendERKSs");appstr(two,two);assert(two->size()==10);
 auto appfill=FN(S*(*)(S*,uint32_t,int),"ZNSs6appendEmc");appfill(two,3,'x');assert(two->size()==13);
 auto asstr=FN(S*(*)(S*,const S*),"ZNSs6assignERKSs");asstr(two,one);asstr(two,two);assert(*two==*one);
 auto insert=FN(S*(*)(S*,uint32_t,const S*,uint32_t,uint32_t),"ZNSs6insertEmRKSsmm");
 insert(two,4,one,4,UINT32_MAX);assert(*two=="Iron Man Man");
 auto resize=FN(void(*)(S*,uint32_t,int),"ZNSs6resizeEmc");resize(two,4,'?');assert(*two=="Iron");resize(two,6,'!');assert(*two=="Iron!!");
 auto reserve=FN(void(*)(S*,uint32_t),"ZNSs7reserveEm");reserve(two,256);assert(two->capacity()>=256);
 auto push=FN(void(*)(S*,int),"ZNSs9push_backEc");push(two,'2');assert(*two=="Iron!!2");
 auto find=FN(uint32_t(*)(const S*,const char*,uint32_t),"ZNKSs4findEPKcm");
 auto findn=FN(uint32_t(*)(const S*,const char*,uint32_t,uint32_t),"ZNKSs4findEPKcmm");
 assert(find(one,"Man",0)==5 && find(one,"X",0)==UINT32_MAX && find(one,"Iron",1)==UINT32_MAX);
 assert(findn(three,bytes+1,0,2)==1);
 auto cmp=FN(int(*)(const S*,const char*),"ZNKSs7compareEPKc");
 auto cmps=FN(int(*)(const S*,const S*),"ZNKSs7compareERKSs");assert(cmp(one,"Iron Man")==0 && cmps(one,one)==0 && cmp(one,"Z")<0);
 destroy(three);
 auto sub=FN(S*(*)(S*,const S*,uint32_t,uint32_t),"ZNKSs6substrEmm");
 three=sub(reinterpret_cast<S*>(c),one,5,UINT32_MAX);assert(*three=="Man");destroy(three);
 auto leak=FN(void(*)(S*),"ZNSs12_M_leak_hardEv");asstr(two,one);leak(two);assert(one->c_str()!=two->c_str());
 auto mutate=FN(void(*)(S*,uint32_t,uint32_t,uint32_t),"ZNSs9_M_mutateEmmm");
 mutate(two,4,4,1);*index(two,4)='2';assert(*two=="Iron2");
 // Real COW rep destruction using the base before its character data.
 auto repdestroy=FN(void(*)(void*,const void*),"ZNSs4_Rep10_M_destroyERKSaIcE");
 three=ctor(reinterpret_cast<S*>(c),"allocated nonempty representation",nullptr);
 const char *data=three->c_str();repdestroy(const_cast<char*>(data)-2*sizeof(size_t)-sizeof(void*),nullptr);
 // Deliberately do not run the object's destructor after manual rep destruction.
 const import_entry *empty=nullptr;
 for(int i=0;i<legacy_string_imports_count;i++)if(legacy_string_imports[i].data==1)empty=&legacy_string_imports[i];
 assert(empty && !strcmp(empty->name,"__ZNSs4_Rep20_S_empty_rep_storageE"));
 three=ctor(reinterpret_cast<S*>(c),"",nullptr);
 assert(reinterpret_cast<uintptr_t>(three->c_str())==empty->addr+2*sizeof(size_t)+sizeof(void*));destroy(three);
 // Out-of-range native exceptions must terminate inside the bridge, not escape.
 pid_t pid=fork();assert(pid>=0);if(!pid){sub(reinterpret_cast<S*>(c),one,100,1);_Exit(1);}
 int status;assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==73);
 destroy(two);destroy(one);
 auto atomic=FN(int32_t(*)(volatile int32_t*,int32_t),"ZN9__gnu_cxx18__exchange_and_addEPVii");
 volatile int32_t ref=1;assert(atomic(&ref,-1)==1 && ref==0);assert(atomic(&ref,-1)==0 && ref==-1);
 assert(legacy_string_imports_count==24);
 puts("PASS: 24 legacy C++ bindings with atomic refcounts, COW detach, binary/UTF-8 bytes, hidden result, npos, mutations, empty data symbol and exception containment");
}
