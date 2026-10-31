// SPDX-License-Identifier: MIT. Reuse SDK's legacy libstdc++ stream ABI.
#define _GLIBCXX_USE_CXX11_ABI 0
#include <sstream>
#include <string>
#include <new>
#include <cstdint>
extern "C" {
#include "imports.h"
#include "log.h"
}
using Stream=std::ostringstream;
using Buffer=std::stringbuf;
using String=std::string;
#if defined(__arm__)
static_assert(sizeof(Stream)==180 && sizeof(Buffer)==40 && sizeof(std::ostream)==140 &&
              sizeof(std::ios)==136 && sizeof(std::ios_base)==112 && sizeof(String)==4 &&
              sizeof(std::ios_base::Init)==1,"Original ARM libstdc++ stream layout required");
static_assert(sizeof(long)==4 && std::ios_base::out==0x10,"Original signed long/openmode ABI");
#endif
#define ENTER try {
#define LEAVE } catch (...) {fatal_error("Legacy C++ stream exception cannot unwind iOS guest frames");}
static Stream *construct(Stream *p,uint32_t mode){ENTER return new(p) Stream(static_cast<std::ios_base::openmode>(mode));LEAVE}
static void destroy(Stream *p){ENTER p->~Stream();LEAVE}
static std::ostream *insert_int(std::ostream *p,int32_t n){ENTER return &(*p<<n);LEAVE}
static std::ostream *insert_long(std::ostream *p,int32_t n){ENTER return &(*p<<static_cast<long>(n));LEAVE}
static String *get_string(String *out,const Buffer *p){ENTER return new(out) String(p->str());LEAVE}
static uint32_t widen(const std::ios *p,uint32_t byte){ENTER return static_cast<unsigned char>(p->widen(static_cast<char>(byte&255)));LEAVE}
static void *init_construct(void *p){ENTER return new(p) std::ios_base::Init;LEAVE}
static void init_destroy(std::ios_base::Init *p){ENTER p->~Init();LEAVE}
const import_entry legacy_stream_imports[]={
 F(_ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEEC1ESt13_Ios_Openmode,construct),
 F(_ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEED1Ev,destroy),
 F(_ZNSolsEi,insert_int),F(_ZNSo9_M_insertIlEERSoT_,insert_long),
 F(_ZNKSt15basic_stringbufIcSt11char_traitsIcESaIcEE3strEv,get_string),
 F(_ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc,widen),
 F(_ZNSt8ios_base4InitC1Ev,init_construct),F(_ZNSt8ios_base4InitD1Ev,init_destroy),
};
const int legacy_stream_imports_count=sizeof(legacy_stream_imports)/sizeof(legacy_stream_imports[0]);
