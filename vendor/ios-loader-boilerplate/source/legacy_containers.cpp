// SPDX-License-Identifier: MIT
// Old iOS libstdc++ node operations, using the existing SDK implementation.
#include <list>
#include <map>
#include <cstddef>
#include <cstdint>
extern "C" {
#include "imports.h"
}
using List = std::__detail::_List_node_base;
using Tree = std::_Rb_tree_node_base;
#if defined(__arm__)
static_assert(sizeof(List)==8 && offsetof(List,_M_next)==0 && offsetof(List,_M_prev)==4,
              "Original ARM list node has two pointers before payload at +8");
static_assert(sizeof(Tree)==16 && offsetof(Tree,_M_color)==0 && offsetof(Tree,_M_parent)==4 &&
              offsetof(Tree,_M_left)==8 && offsetof(Tree,_M_right)==12,
              "Original ARM tree node has four words before payload at +16");
static_assert(std::_S_red==0 && std::_S_black==1,"Original node color encoding");
#endif
static void list_hook(List *node,List *before) { node->_M_hook(before); }
static void list_unhook(List *node) { node->_M_unhook(); }
static void list_transfer(List *before,List *first,List *last) {
    // The old callers skip empty ranges; preserve that invariant at the boundary.
    if(first!=last) before->_M_transfer(first,last);
}
static Tree *tree_next(Tree *node) { return std::_Rb_tree_increment(node); }
// Some SDK headers omit the const overload while the compatibility symbol is
// retained in libstdc++. Its ABI is one pointer in, one pointer out.
extern "C" const Tree *native_tree_next_const(const Tree *)
    asm("_ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base");
static const Tree *tree_next_const(const Tree *node) { return native_tree_next_const(node); }
static Tree *tree_prev(Tree *node) { return std::_Rb_tree_decrement(node); }
static Tree *tree_erase(Tree *node,Tree *header) {
    // Caller owns payload destruction/deallocation and size bookkeeping.
    return std::_Rb_tree_rebalance_for_erase(node,*header);
}
static void tree_insert(uint32_t left,Tree *node,Tree *parent,Tree *header) {
    std::_Rb_tree_insert_and_rebalance((left&0xff)!=0,node,parent,*header);
}
const import_entry legacy_container_imports[] = {
 F(_ZNSt15_List_node_base4hookEPS_,list_hook),
 F(_ZNSt15_List_node_base6unhookEv,list_unhook),
 F(_ZNSt15_List_node_base8transferEPS_S0_,list_transfer),
 F(_ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base,tree_next),
 F(_ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base,tree_next_const),
 F(_ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base,tree_prev),
 F(_ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_,tree_erase),
 F(_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_,tree_insert),
};
const int legacy_container_imports_count=sizeof(legacy_container_imports)/sizeof(legacy_container_imports[0]);
