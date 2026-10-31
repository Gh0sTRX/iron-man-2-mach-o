// SPDX-License-Identifier: MIT
// Structural invariants exercise real SDK list/tree operations through imports.
#include <list>
#include <map>
#include <vector>
#include <algorithm>
#include <random>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
extern "C" {
#include "imports.h"
}
using L=std::__detail::_List_node_base;using T=std::_Rb_tree_node_base;
template<class F> static F fn(const char *name) {
 for(int i=0;i<legacy_container_imports_count;i++)if(!strcmp(legacy_container_imports[i].name,name)) {
  assert(legacy_container_imports[i].data==IM2_IMPORT_FUNCTION && legacy_container_imports[i].addr);
  return reinterpret_cast<F>(legacy_container_imports[i].addr);
 }
 assert(false);return nullptr;
}
struct Node { T base; uint32_t key,guard; };
static unsigned verify(T *p,T *parent,std::vector<uint32_t> &order) {
 if(!p)return 1;
 assert(p->_M_parent==parent && (p->_M_color==std::_S_red || p->_M_color==std::_S_black));
 if(p->_M_color==std::_S_red) {
  assert(!p->_M_left || p->_M_left->_M_color==std::_S_black);
  assert(!p->_M_right || p->_M_right->_M_color==std::_S_black);
 }
 unsigned a=verify(p->_M_left,p,order);auto n=reinterpret_cast<Node*>(p);
 assert(n->guard==0xa57ead01);order.push_back(n->key);
 unsigned b=verify(p->_M_right,p,order);assert(a==b);
 return a+(p->_M_color==std::_S_black);
}
static void verify_all(T &header,const std::vector<bool> &present) {
 std::vector<uint32_t> order,expected;
 for(unsigned i=0;i<present.size();i++)if(present[i])expected.push_back(i);
 if(!expected.empty())assert(header._M_parent && header._M_parent->_M_color==std::_S_black);
 else assert(!header._M_parent && header._M_left==&header && header._M_right==&header);
 verify(header._M_parent,&header,order);assert(order==expected);
 if(!expected.empty()) {
  assert(reinterpret_cast<Node*>(header._M_left)->key==expected.front());
  assert(reinterpret_cast<Node*>(header._M_right)->key==expected.back());
 }
}
int main() {
 auto hook=fn<void(*)(L*,L*)>("__ZNSt15_List_node_base4hookEPS_");
 auto unhook=fn<void(*)(L*)>("__ZNSt15_List_node_base6unhookEv");
 auto transfer=fn<void(*)(L*,L*,L*)>("__ZNSt15_List_node_base8transferEPS_S0_");
 L a,b,n[4];a._M_next=a._M_prev=&a;b._M_next=b._M_prev=&b;
 for(auto &v:n)hook(&v,&a);
 assert(a._M_next==n && a._M_prev==n+3);
 unhook(n+1);assert(n[0]._M_next==n+2 && n[2]._M_prev==n);
 transfer(&b,n+2,&a);assert(a._M_next==n && a._M_prev==n && n[0]._M_next==&a);
 assert(b._M_next==n+2 && b._M_prev==n+3 && n[2]._M_prev==&b && n[3]._M_next==&b);
 transfer(&a,&b,&b);assert(a._M_next==n && b._M_next==n+2); // empty range
 transfer(&a,b._M_next,&b);assert(b._M_next==&b && b._M_prev==&b);
 assert(n[0]._M_next==n+2 && n[2]._M_next==n+3 && n[3]._M_next==&a);
 // Move a range to its existing end: no corruption.
 transfer(&a,a._M_next,&a);assert(a._M_next==n && a._M_prev==n+3);
 auto insert=fn<void(*)(uint32_t,T*,T*,T*)>("__ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_");
 auto erase=fn<T*(*)(T*,T*)>("__ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_");
 auto next=fn<T*(*)(T*)>("__ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base");
 auto nextc=fn<const T*(*)(const T*)>("__ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base");
 auto prev=fn<T*(*)(T*)>("__ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base");
 const unsigned count=127;std::vector<Node> nodes(count);std::vector<unsigned> indices(count);
 for(unsigned i=0;i<count;i++){nodes[i].key=i;nodes[i].guard=0xa57ead01;indices[i]=i;}
 std::mt19937 rng(0x1a02);
 for(unsigned pass=0;pass<4;pass++) {
  T header;header._M_color=std::_S_red;header._M_parent=nullptr;header._M_left=header._M_right=&header;
  std::vector<bool> present(count,false);std::shuffle(indices.begin(),indices.end(),rng);
  for(unsigned k:indices) {
   T *parent=&header,*p=header._M_parent;bool left=true;
   while(p){parent=p;left=k<reinterpret_cast<Node*>(p)->key;p=left?p->_M_left:p->_M_right;}
   // Exercise unspecified high bits of the original one-byte bool argument.
   insert(0x100u|unsigned(left),&nodes[k].base,parent,&header);present[k]=true;verify_all(header,present);
  }
  T *p=header._M_left;
  for(unsigned k=0;k<count;k++){assert(p==&nodes[k].base && next(p)==nextc(p));p=next(p);}
  assert(p==&header);p=prev(&header);
  for(unsigned k=count;k>0;k--){assert(p==&nodes[k-1].base);if(k>1)p=prev(p);}
  // Different deletion orders force leaf, one-child, two-child and root erase.
  if(pass==0)std::sort(indices.begin(),indices.end());
  else if(pass==1)std::sort(indices.rbegin(),indices.rend());
  else std::shuffle(indices.begin(),indices.end(),rng);
  for(unsigned k:indices){assert(erase(&nodes[k].base,&header)==&nodes[k].base);present[k]=false;verify_all(header,present);}
  for(unsigned i=0;i<count;i++)assert(nodes[i].key==i && nodes[i].guard==0xa57ead01);
 }
 assert(legacy_container_imports_count==8);
 puts("PASS: legacy list splice/unhook, 508 tree insertions and erasures, black-height/red/root/parent invariants, mutable/const/reverse iteration, bool upper bits and payload preservation");
}
