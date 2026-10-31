// SPDX-License-Identifier: MIT
// iOS libstdc++ COW string bridge. No iOS SjLj exception may cross this boundary.
#define _GLIBCXX_USE_CXX11_ABI 0
#include <string>
#include <new>
#include <cstdint>
#include <limits>
extern "C" {
#include "imports.h"
#include "log.h"
}
using String = std::string;
#if defined(__arm__)
static_assert(sizeof(String) == 4 && sizeof(std::size_t) == 4,
              "Guest requires a one-word ARM32 COW string");
#endif
static_assert(_GLIBCXX_USE_CXX11_ABI == 0, "Modern string ABI is incompatible");
// Host tests use 64-bit size_t; guest npos is always UINT32_MAX.
static std::size_t count(uint32_t n) { return n == UINT32_MAX ? String::npos : n; }
#define ENTER try {
#define LEAVE } catch (...) { fatal_error("Legacy C++ string operation failed; guest exception unwinding unsupported"); }
static String *construct(String *out, const char *p, const void *) {
    ENTER return new(out) String(p); LEAVE
}
static String *construct_n(String *out, const char *p, uint32_t n, const void *) {
    ENTER return new(out) String(p, n); LEAVE
}
static String *copy(String *out, const String *s) {
    ENTER return new(out) String(*s); LEAVE
}
static void destroy(String *s) { s->~String(); }
static String *append_n(String *s, const char *p, uint32_t n) {
    ENTER s->append(p,n); return s; LEAVE
}
static String *append_string(String *s, const String *other) {
    ENTER s->append(*other); return s; LEAVE
}
static String *append_fill(String *s, uint32_t n, int c) {
    ENTER s->append(n,static_cast<char>(c)); return s; LEAVE
}
static String *assign_n(String *s, const char *p, uint32_t n) {
    ENTER s->assign(p,n); return s; LEAVE
}
static String *assign_string(String *s, const String *other) {
    ENTER s->assign(*other); return s; LEAVE
}
static String *insert(String *s, uint32_t at, const String *other, uint32_t start, uint32_t n) {
    ENTER s->insert(at,*other,start,count(n)); return s; LEAVE
}
static void resize(String *s, uint32_t n, int c) {
    ENTER s->resize(n,static_cast<char>(c)); LEAVE
}
static void reserve(String *s, uint32_t n) { ENTER s->reserve(n); LEAVE }
static void push(String *s, int c) { ENTER s->push_back(static_cast<char>(c)); LEAVE }
static char *index(String *s, uint32_t at) {
    ENTER if(at > s->size()) fatal_error("Legacy string index exceeds size");
    return &(*s)[at]; LEAVE
}
static uint32_t find(const String *s, const char *p, uint32_t at) {
    ENTER return static_cast<uint32_t>(s->find(p,at)); LEAVE
}
static uint32_t find_n(const String *s, const char *p, uint32_t at, uint32_t n) {
    ENTER return static_cast<uint32_t>(s->find(p,at,n)); LEAVE
}
// Nontrivial C++ return: original ARM caller supplies result storage before this.
static String *substr(String *out, const String *s, uint32_t at, uint32_t n) {
    ENTER return new(out) String(s->substr(at,count(n))); LEAVE
}
static int compare(const String *s, const char *p) { ENTER return s->compare(p); LEAVE }
static int compare_string(const String *s, const String *other) { ENTER return s->compare(*other); LEAVE }
// These functions operate on the same COW header as guest inline refcount code.
extern "C" unsigned char native_empty_rep[] asm("_ZNSs4_Rep20_S_empty_rep_storageE");
extern "C" void native_leak(String *) asm("_ZNSs12_M_leak_hardEv");
extern "C" void native_destroy_rep(void *, const void *) asm("_ZNSs4_Rep10_M_destroyERKSaIcE");
#if defined(__arm__)
extern "C" void native_mutate(String *,std::size_t,std::size_t,std::size_t) asm("_ZNSs9_M_mutateEjjj");
#else
extern "C" void native_mutate(String *,std::size_t,std::size_t,std::size_t) asm("_ZNSs9_M_mutateEmmm");
#endif
static void leak(String *s) { ENTER native_leak(s); LEAVE }
static void mutate(String *s,uint32_t at,uint32_t removed,uint32_t added) {
    ENTER if(at>s->size() || removed>s->size()-at) fatal_error("Legacy string mutation exceeds size");
    native_mutate(s,at,removed,added); LEAVE
}
static void destroy_rep(void *p, const void *allocator) { native_destroy_rep(p,allocator); }
// Original inline string destructors call this with the refcount at data-4.
static int32_t exchange_add(volatile int32_t *p, int32_t delta) {
    return __atomic_fetch_add(p,delta,__ATOMIC_ACQ_REL);
}
// Declaration in imports.h gives these const definitions external C linkage.
const import_entry legacy_string_imports[] = {
 F(_ZN9__gnu_cxx18__exchange_and_addEPVii,exchange_add),
 F(_ZNSsC1EPKcRKSaIcE,construct), F(_ZNSsC1EPKcmRKSaIcE,construct_n),
 F(_ZNSsC1ERKSs,copy), F(_ZNSsD2Ev,destroy),
 F(_ZNSs6appendEPKcm,append_n), F(_ZNSs6appendERKSs,append_string), F(_ZNSs6appendEmc,append_fill),
 F(_ZNSs6assignEPKcm,assign_n), F(_ZNSs6assignERKSs,assign_string),
 F(_ZNSs6insertEmRKSsmm,insert), F(_ZNSs6resizeEmc,resize), F(_ZNSs7reserveEm,reserve),
 F(_ZNSs9push_backEc,push), F(_ZNSsixEm,index),
 F(_ZNKSs4findEPKcm,find), F(_ZNKSs4findEPKcmm,find_n), F(_ZNKSs6substrEmm,substr),
 F(_ZNKSs7compareEPKc,compare), F(_ZNKSs7compareERKSs,compare_string),
 F(_ZNSs12_M_leak_hardEv,leak), F(_ZNSs9_M_mutateEmmm,mutate),
 F(_ZNSs4_Rep10_M_destroyERKSaIcE,destroy_rep), D(_ZNSs4_Rep20_S_empty_rep_storageE,native_empty_rep),
};
const int legacy_string_imports_count = sizeof(legacy_string_imports)/sizeof(legacy_string_imports[0]);
