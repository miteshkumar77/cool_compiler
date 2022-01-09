#ifndef SEMANT_H_
#define SEMANT_H_

#include <assert.h>
#include <iostream>  
#include "cool-tree.h"
#include "stringtab.h"
#include "symtab.h"
#include "list.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define TRUE 1
#define FALSE 0

class ClassTable;
typedef ClassTable *ClassTableP;

using ObjectEnv = SymbolTable<Symbol, Symbol>; 
using MethodTable = std::unordered_map<Symbol,
            std::unordered_map<Symbol, method_class const*>>;

// This is a structure that may be used to contain the semantic
// information such as the inheritance graph.  You may use it or not as
// you like: it is only here to provide a container for the supplied
// methods.

class ClassTable {
private:
  mutable int semant_errors;
  void install_basic_classes();
  std::unordered_map<Symbol, std::vector<Symbol>> graph;
  std::unordered_map<Symbol, Class_> sym_class;
  ostream& error_stream;
  MethodTable m_Table;
  void build_method_table(std::unordered_map<Symbol, method_class const*>& methods,
    Symbol class_node);
  void type_check(ObjectEnv& object_env, Symbol class_node) const;
public:
  ClassTable(Classes);
  int errors() { return semant_errors; }
  bool type_compare_inclusive(Symbol base_t, Symbol super_t) const;
  bool type_compare_exclusive(Symbol base_t, Symbol super_t) const;
  bool is_valid_overload(method_class const& base_method,
      method_class const& super_method) const;
  void check_hierarchy(
        std::unordered_map<Symbol, method_class const*> methods,
        std::unordered_set<Symbol> members,
        Symbol class_node) const;
  void check_hierarchy() const;
  void build_method_table();
  void add_edge(Symbol class_id, Symbol parent_id);
  void type_check() const;
  ostream& semant_error() const;
  ostream& semant_error(Class_ c);
  ostream& semant_error(Symbol filename, tree_node *t);
};


#endif

