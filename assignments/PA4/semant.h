#ifndef SEMANT_H_
#define SEMANT_H_

#include <assert.h>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cool-tree.h"
#include "tree.h"
#include "stringtab.h"
#include "symtab.h"
#include "list.h"

#define TRUE 1
#define FALSE 0

class ClassTable;
typedef ClassTable *ClassTableP;

using MethodTable = std::unordered_map<Symbol,
                                       std::unordered_map<Symbol, method_class const *>>;

// This is a structure that may be used to contain the semantic
// information such as the inheritance graph.  You may use it or not as
// you like: it is only here to provide a container for the supplied
// methods.

inline void halt();
inline void halt(ClassTable const *classtable);
inline void add_object(Symbol id, Symbol type, ObjectEnv &object_env, ClassTable const &class_tbl);
inline Symbol reference_object(Symbol id, ObjectEnv &object_env, ClassTable const &class_tbl);
inline method_class const *reference_method(Symbol C, Symbol f, ClassTable const &class_tbl);
inline void add_method(Symbol C, Symbol f, ClassTable &class_tbl, method_class const *m);

class ClassTable
{
  friend class tree_node;

private:
  mutable int semant_errors;
  void install_basic_classes();
  inline void add_edge(Symbol class_id, Symbol parent_id);
  ostream &error_stream;

  std::unordered_map<Symbol, std::vector<Symbol>> graph;

public:
  ClassTable(Classes);
  int errors() const { return semant_errors; }
  void check_hierarchy() const;
  void check_hierarchy(
      std::unordered_map<Symbol, method_class const *> methods,
      std::unordered_set<Symbol> members,
      Symbol class_node) const;
  ostream &semant_error() const;
  ostream &semant_error(Class_ c);
  ostream &semant_error(Symbol filename, tree_node *t);
  bool type_lte(Symbol base_t, Symbol super_t) const;
  bool type_lt(Symbol base_t, Symbol super_t) const;
  Symbol lca(std::unordered_set<Symbol> const &classes) const;
  Symbol lca(std::unordered_set<Symbol> &classes, Symbol class_node) const;

  void check_type();
  void check_type(ObjectEnv &object_env, Symbol class_node);

  MethodTable m_Table;
  std::unordered_map<Symbol, Class_> sym_class;
};

#endif
