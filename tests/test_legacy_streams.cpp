// SPDX-License-Identifier: MIT. Real SDK stream operations through import table.
#define _GLIBCXX_USE_CXX11_ABI 0
#include <sstream>
#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
extern "C" {
#include "imports.h"
void fatal_error(const char *,...){std::abort();}
}
using O=std::ostringstream;using B=std::stringbuf;using S=std::string;
template<class F> static F method(const char *name){
 for(int i=0;i<legacy_stream_imports_count;i++)if(!strcmp(name,legacy_stream_imports[i].name)){
  assert(legacy_stream_imports[i].data==0 && legacy_stream_imports[i].addr);return reinterpret_cast<F>(legacy_stream_imports[i].addr);
 }std::abort();
}
int main(){
 auto ctor=method<O*(*)(O*,uint32_t)>("__ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEEC1ESt13_Ios_Openmode");
 auto dtor=method<void(*)(O*)>("__ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEED1Ev");
 auto putint=method<std::ostream*(*)(std::ostream*,int32_t)>("__ZNSolsEi");
 auto putlong=method<std::ostream*(*)(std::ostream*,int32_t)>("__ZNSo9_M_insertIlEERSoT_");
 auto get=method<S*(*)(S*,const B*)>("__ZNKSt15basic_stringbufIcSt11char_traitsIcESaIcEE3strEv");
 auto widen=method<uint32_t(*)(const std::ios*,uint32_t)>("__ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc");
 auto init=method<void*(*)(void*)>("__ZNSt8ios_base4InitC1Ev");
 auto done=method<void(*)(std::ios_base::Init*)>("__ZNSt8ios_base4InitD1Ev");
 alignas(std::ios_base::Init) unsigned char initial[sizeof(std::ios_base::Init)];assert(init(initial)==initial);
 for(int32_t n:{0,1234,-1234,INT32_MIN,INT32_MAX}){
  alignas(O) unsigned char storage[sizeof(O)+2*alignof(O)];memset(storage,0xa5,sizeof(storage));
  auto stream=ctor(reinterpret_cast<O*>(storage+alignof(O)),0x10);assert(stream);
  assert(putint(stream,n)==stream);assert(widen(stream,'\n')=='\n');
  alignas(S) unsigned char output[sizeof(S)];S *s=get(reinterpret_cast<S*>(output),stream->rdbuf());
  assert(*s==std::to_string(n));s->~S();
  stream->str("");stream->clear();assert(putlong(stream,n)==stream);
  s=get(reinterpret_cast<S*>(output),stream->rdbuf());assert(*s==std::to_string(n));s->~S();
  dtor(stream);
  for(size_t i=0;i<alignof(O);i++){assert(storage[i]==0xa5);assert(storage[alignof(O)+sizeof(O)+i]==0xa5);}
 }
 done(reinterpret_cast<std::ios_base::Init*>(initial));assert(legacy_stream_imports_count==8);
 puts("PASS: legacy stream/Init lifecycle, signed32 int/long formatting, stringbuf hidden result, widen and object bounds");
}
