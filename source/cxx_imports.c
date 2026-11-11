// Generado por tools/gen_cxx.py: imports C++ del juego -> libstdc++ de VitaSDK.
// No editar a mano (volver a correr gen_cxx.py con el report.txt de ios_prep.py).
#include <string.h>

#include "imports.h"

extern char cxx_0 __asm__("_ZNKSs4findEPKcj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::find(char const*, unsigned long) const
extern char cxx_1 __asm__("_ZNKSs4findEPKcjj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::find(char const*, unsigned long, unsigned long) const
extern char cxx_2 __asm__("_ZNKSs6substrEjj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::substr(unsigned long, unsigned long) const
extern char cxx_3 __asm__("_ZNKSs7compareEPKc"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::compare(char const*) const
extern char cxx_4 __asm__("_ZNKSs7compareERKSs"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::compare(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&) const
extern char cxx_5 __asm__("_ZNKSt15basic_stringbufIcSt11char_traitsIcESaIcEE3strEv"); // std::basic_stringbuf<char, std::char_traits<char>, std::allocator<char> >::str() const
extern char cxx_6 __asm__("_ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc"); // std::basic_ios<char, std::char_traits<char> >::widen(char) const
extern char cxx_7 __asm__("_ZNSo9_M_insertIlEERSoT_"); // std::basic_ostream<char, std::char_traits<char> >& std::basic_ostream<char, std::char_traits<char> >::_M_insert<long>(long)
extern char cxx_8 __asm__("_ZNSolsEi"); // std::basic_ostream<char, std::char_traits<char> >::operator<<(int)
extern char cxx_9 __asm__("_ZNSs12_M_leak_hardEv"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::_M_leak_hard()
extern char cxx_10 __asm__("_ZNSs4_Rep10_M_destroyERKSaIcE"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::_Rep::_M_destroy(std::allocator<char> const&)
extern char cxx_11 __asm__("_ZNSs4_Rep20_S_empty_rep_storageE"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::_Rep::_S_empty_rep_storage
extern char cxx_12 __asm__("_ZNSs6appendEPKcj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::append(char const*, unsigned long)
extern char cxx_13 __asm__("_ZNSs6appendERKSs"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::append(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
extern char cxx_14 __asm__("_ZNSs6appendEjc"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::append(unsigned long, char)
extern char cxx_15 __asm__("_ZNSs6assignEPKcj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::assign(char const*, unsigned long)
extern char cxx_16 __asm__("_ZNSs6assignERKSs"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::assign(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
extern char cxx_17 __asm__("_ZNSs6insertEjRKSsjj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::insert(unsigned long, std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, unsigned long, unsigned long)
extern char cxx_18 __asm__("_ZNSs6resizeEjc"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::resize(unsigned long, char)
extern char cxx_19 __asm__("_ZNSs7reserveEj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::reserve(unsigned long)
extern char cxx_20 __asm__("_ZNSs9_M_mutateEjjj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::_M_mutate(unsigned long, unsigned long, unsigned long)
extern char cxx_21 __asm__("_ZNSs9push_backEc"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::push_back(char)
extern char cxx_22 __asm__("_ZNSsC1EPKcRKSaIcE"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string(char const*, std::allocator<char> const&)
extern char cxx_23 __asm__("_ZNSsC1EPKcjRKSaIcE"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string(char const*, unsigned long, std::allocator<char> const&)
extern char cxx_24 __asm__("_ZNSsC1ERKSs"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
extern char cxx_25 __asm__("_ZNSsD2Ev"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::~basic_string()
extern char cxx_26 __asm__("_ZNSsixEj"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::operator[](unsigned long)
extern char cxx_27 __asm__("_ZNSt15_List_node_base4hookEPS_"); // std::_List_node_base::hook(std::_List_node_base*)
extern char cxx_28 __asm__("_ZNSt15_List_node_base6unhookEv"); // std::_List_node_base::unhook()
extern char cxx_29 __asm__("_ZNSt15_List_node_base8transferEPS_S0_"); // std::_List_node_base::transfer(std::_List_node_base*, std::_List_node_base*)
extern char cxx_30 __asm__("_ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEEC1ESt13_Ios_Openmode"); // std::basic_ostringstream<char, std::char_traits<char>, std::allocator<char> >::basic_ostringstream(std::_Ios_Openmode)
extern char cxx_31 __asm__("_ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEED1Ev"); // std::basic_ostringstream<char, std::char_traits<char>, std::allocator<char> >::~basic_ostringstream()
extern char cxx_32 __asm__("_ZNSt8ios_base4InitC1Ev"); // std::ios_base::Init::Init()
extern char cxx_33 __asm__("_ZNSt8ios_base4InitD1Ev"); // std::ios_base::Init::~Init()
extern char cxx_34 __asm__("_ZSt17__throw_bad_allocv"); // std::__throw_bad_alloc()
extern char cxx_35 __asm__("_ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base"); // std::_Rb_tree_decrement(std::_Rb_tree_node_base*)
extern char cxx_36 __asm__("_ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base"); // std::_Rb_tree_increment(std::_Rb_tree_node_base const*)
extern char cxx_37 __asm__("_ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base"); // std::_Rb_tree_increment(std::_Rb_tree_node_base*)
extern char cxx_38 __asm__("_ZSt19__throw_logic_errorPKc"); // std::__throw_logic_error(char const*)
extern char cxx_39 __asm__("_ZSt20__throw_length_errorPKc"); // std::__throw_length_error(char const*)
extern char cxx_40 __asm__("_ZSt20__throw_out_of_rangePKc"); // std::__throw_out_of_range(char const*)
extern char cxx_41 __asm__("_ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_"); // std::_Rb_tree_rebalance_for_erase(std::_Rb_tree_node_base*, std::_Rb_tree_node_base&)
extern char cxx_42 __asm__("_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_"); // std::_Rb_tree_insert_and_rebalance(bool, std::_Rb_tree_node_base*, std::_Rb_tree_node_base*, std::_Rb_tree_node_base&)
extern char cxx_43 __asm__("_ZTVN10__cxxabiv117__class_type_infoE"); // vtable for __cxxabiv1::__class_type_info
extern char cxx_44 __asm__("_ZTVN10__cxxabiv120__si_class_type_infoE"); // vtable for __cxxabiv1::__si_class_type_info
extern char cxx_45 __asm__("_ZTVN10__cxxabiv121__vmi_class_type_infoE"); // vtable for __cxxabiv1::__vmi_class_type_info

const import_entry cxx_imports[] = {
    {"__ZNKSs4findEPKcm", (uintptr_t)&cxx_0, 0},
    {"__ZNKSs4findEPKcmm", (uintptr_t)&cxx_1, 0},
    {"__ZNKSs6substrEmm", (uintptr_t)&cxx_2, 0},
    {"__ZNKSs7compareEPKc", (uintptr_t)&cxx_3, 0},
    {"__ZNKSs7compareERKSs", (uintptr_t)&cxx_4, 0},
    {"__ZNKSt15basic_stringbufIcSt11char_traitsIcESaIcEE3strEv", (uintptr_t)&cxx_5, 0},
    {"__ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc", (uintptr_t)&cxx_6, 0},
    {"__ZNSo9_M_insertIlEERSoT_", (uintptr_t)&cxx_7, 0},
    {"__ZNSolsEi", (uintptr_t)&cxx_8, 0},
    {"__ZNSs12_M_leak_hardEv", (uintptr_t)&cxx_9, 0},
    {"__ZNSs4_Rep10_M_destroyERKSaIcE", (uintptr_t)&cxx_10, 0},
    {"__ZNSs4_Rep20_S_empty_rep_storageE", (uintptr_t)&cxx_11, 1},
    {"__ZNSs6appendEPKcm", (uintptr_t)&cxx_12, 0},
    {"__ZNSs6appendERKSs", (uintptr_t)&cxx_13, 0},
    {"__ZNSs6appendEmc", (uintptr_t)&cxx_14, 0},
    {"__ZNSs6assignEPKcm", (uintptr_t)&cxx_15, 0},
    {"__ZNSs6assignERKSs", (uintptr_t)&cxx_16, 0},
    {"__ZNSs6insertEmRKSsmm", (uintptr_t)&cxx_17, 0},
    {"__ZNSs6resizeEmc", (uintptr_t)&cxx_18, 0},
    {"__ZNSs7reserveEm", (uintptr_t)&cxx_19, 0},
    {"__ZNSs9_M_mutateEmmm", (uintptr_t)&cxx_20, 0},
    {"__ZNSs9push_backEc", (uintptr_t)&cxx_21, 0},
    {"__ZNSsC1EPKcRKSaIcE", (uintptr_t)&cxx_22, 0},
    {"__ZNSsC1EPKcmRKSaIcE", (uintptr_t)&cxx_23, 0},
    {"__ZNSsC1ERKSs", (uintptr_t)&cxx_24, 0},
    {"__ZNSsD2Ev", (uintptr_t)&cxx_25, 0},
    {"__ZNSsixEm", (uintptr_t)&cxx_26, 0},
    {"__ZNSt15_List_node_base4hookEPS_", (uintptr_t)&cxx_27, 0},
    {"__ZNSt15_List_node_base6unhookEv", (uintptr_t)&cxx_28, 0},
    {"__ZNSt15_List_node_base8transferEPS_S0_", (uintptr_t)&cxx_29, 0},
    {"__ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEEC1ESt13_Ios_Openmode", (uintptr_t)&cxx_30, 0},
    {"__ZNSt19basic_ostringstreamIcSt11char_traitsIcESaIcEED1Ev", (uintptr_t)&cxx_31, 0},
    {"__ZNSt8ios_base4InitC1Ev", (uintptr_t)&cxx_32, 0},
    {"__ZNSt8ios_base4InitD1Ev", (uintptr_t)&cxx_33, 0},
    {"__ZSt17__throw_bad_allocv", (uintptr_t)&cxx_34, 0},
    {"__ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base", (uintptr_t)&cxx_35, 0},
    {"__ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base", (uintptr_t)&cxx_36, 0},
    {"__ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base", (uintptr_t)&cxx_37, 0},
    {"__ZSt19__throw_logic_errorPKc", (uintptr_t)&cxx_38, 0},
    {"__ZSt20__throw_length_errorPKc", (uintptr_t)&cxx_39, 0},
    {"__ZSt20__throw_out_of_rangePKc", (uintptr_t)&cxx_40, 0},
    {"__ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_", (uintptr_t)&cxx_41, 0},
    {"__ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_", (uintptr_t)&cxx_42, 0},
    {"__ZTVN10__cxxabiv117__class_type_infoE", (uintptr_t)&cxx_43, 1},
    {"__ZTVN10__cxxabiv120__si_class_type_infoE", (uintptr_t)&cxx_44, 1},
    {"__ZTVN10__cxxabiv121__vmi_class_type_infoE", (uintptr_t)&cxx_45, 1},
};
TABLE_END(cxx)

// Lo que no tiene equivalente queda en el log como "unimpl".
const import_entry *cxx_import(const char *name) { (void)name; return NULL; }
