

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "semant.h"
#include "utilities.h"

extern int semant_debug;
extern char *curr_filename;

//////////////////////////////////////////////////////////////////////
//
// Symbols
//
// For convenience, a large number of symbols are predefined here.
// These symbols include the primitive type and method names, as well
// as fixed names used by the runtime system.
//
//////////////////////////////////////////////////////////////////////
static Symbol
    arg,
    arg2,
    Bool,
    concat,
    cool_abort,
    copy,
    Int,
    in_int,
    in_string,
    IO,
    length,
    Main,
    main_meth,
    No_class,
    No_type,
    Object,
    out_int,
    out_string,
    prim_slot,
    self,
    SELF_TYPE,
    Str,
    str_field,
    substr,
    type_name,
    val;
//
// Initializing the predefined symbols.
//
static void initialize_constants(void)
{
    arg = idtable.add_string("arg");
    arg2 = idtable.add_string("arg2");
    Bool = idtable.add_string("Bool");
    concat = idtable.add_string("concat");
    cool_abort = idtable.add_string("abort");
    copy = idtable.add_string("copy");
    Int = idtable.add_string("Int");
    in_int = idtable.add_string("in_int");
    in_string = idtable.add_string("in_string");
    IO = idtable.add_string("IO");
    length = idtable.add_string("length");
    Main = idtable.add_string("Main");
    main_meth = idtable.add_string("main");
    //   _no_class is a symbol that can't be the name of any
    //   user-defined class.
    No_class = idtable.add_string("_no_class");
    No_type = idtable.add_string("_no_type");
    Object = idtable.add_string("Object");
    out_int = idtable.add_string("out_int");
    out_string = idtable.add_string("out_string");
    prim_slot = idtable.add_string("_prim_slot");
    self = idtable.add_string("self");
    SELF_TYPE = idtable.add_string("SELF_TYPE");
    Str = idtable.add_string("String");
    str_field = idtable.add_string("_str_field");
    substr = idtable.add_string("substr");
    type_name = idtable.add_string("type_name");
    val = idtable.add_string("_val");
}

inline bool is_dfl_class(Symbol class_name) { return class_name == Object || class_name == IO || class_name == Int || class_name == Bool || class_name == Str; }

void reachable(std::unordered_map<Symbol, std::vector<Symbol>> const &graph,
               std::unordered_set<Symbol> &visited, Symbol node)
{
    if (visited.count(node))
        return;
    visited.insert(node);
    for (Symbol child : graph.at(node))
    {
        reachable(graph, visited, child);
    }
}

ClassTable::ClassTable(Classes def_classes) : semant_errors(0), error_stream(cerr)
{
    install_basic_classes();

    for (int i = 0; i < def_classes->len(); ++i)
    {
        Class_ cur_class = def_classes->nth(i);
        if (sym_class.count(cur_class->get_name()))
        {
            semant_error(cur_class) << "Redefinition of class " << cur_class->get_name() << std::endl;
            return;
        }
        sym_class[cur_class->get_name()] = cur_class;
        add_edge(cur_class->get_name(), cur_class->get_parent_name());
    }

    {
        std::unordered_set<Symbol> visited;
        reachable(graph, visited, Object);
        if (visited.size() != graph.size())
        {
            semant_error() << "Cyclic inheritance found" << std::endl;
        }
    }
}

void ClassTable::check_hierarchy() const
{
    std::unordered_map<Symbol, method_class const *> methods;
    std::unordered_set<Symbol> members;
    check_hierarchy(methods, members, Object);
}
void ClassTable::check_hierarchy(
    std::unordered_map<Symbol, method_class const *> methods,
    std::unordered_set<Symbol> members,
    Symbol class_node) const
{
    // for back_tracking
    auto methods_save = methods;
    auto members_save = members;

    Features features = sym_class.at(class_node)->get_features();
    for (int i = 0; i < features->len(); ++i)
    {
        Feature f = features->nth(i);
        if (attr_class const *a = dynamic_cast<attr_class const *>(f); a)
        {
            if (members.count(a->get_name()))
            {
                semant_error(class_node) << "Redefinition of attribute " << a->get_name() << " in class " << class_node << std::endl;
            }
            else
            {
                members.insert(a->get_name());
            }
        }
        else if (method_class const *m = dynamic_cast<method_class const *>(f); m)
        {
            if (methods.count(m->get_name()))
            {
                method_class const *m_ref = methods.at(m->get_name());
                if (m->get_return_type() != m_ref->get_return_type())
                {
                    semant_error(class_node) << "Invalid method overload return type" << std::endl;
                    continue;
                }
                Formals formals = m->get_formals();
                Formals formals_ref = m_ref->get_formals();
                if (formals->len() != formals_ref->len())
                {
                    semant_error(class_node) << "Invalid method overload argument count" << std::endl;
                    continue;
                }

                for (int j = 0; j < formals->len(); ++j)
                {
                    Formal formal = formals->nth(j);
                    Formal formal_ref = formals_ref->nth(j);
                    if (formal->get_type_decl() != formal_ref->get_type_decl())
                    {
                        semant_error(class_node) << "Invalid method overload argument type" << std::endl;
                        break;
                    }
                }
            }
            else
            {
                methods[m->get_name()] = m;
            }
        }
    }

    for (Symbol child : graph.at(class_node))
    {
        check_hierarchy(methods, members, child);
    }
    methods.swap(methods_save);
    members.swap(members_save);
}

bool ClassTable::type_lte(Symbol sub_t, Symbol super_t) const
{
    if (sub_t == super_t)
        return true;
    else if (sub_t == Object)
        return false;
    else
        return type_lte(sym_class.at(sub_t)->get_parent_name(), super_t);
}

bool ClassTable::type_lt(Symbol sub_t, Symbol super_t) const
{
    return sub_t != super_t && type_lte(sub_t, super_t);
}

Symbol ClassTable::lca(std::unordered_set<Symbol> const &classes) const
{
    auto classes_save = classes;
    return lca(classes_save, Object);
}

Symbol ClassTable::lca(std::unordered_set<Symbol> &classes, Symbol class_node) const
{
    if (auto it = classes.find(class_node); it != classes.end())
    {
        classes.erase(it);
    }

    Symbol lca_ = nullptr;
    for (auto child : graph.at(class_node))
    {
        lca_ = lca(classes, class_node);
        if (lca_)
            return lca_;
    }
    return classes.empty() ? class_node : nullptr;
}

void assign_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    auto T = reference_object(class_node, name, object_env, class_tbl);
    expr->check_type(class_node, object_env, class_tbl);
    auto Tp = expr->get_type();
    if (class_tbl.type_lte(Tp, T))
    {
        set_type(expr->get_type());
    }
    else
    {
        class_tbl.semant_error(class_node) << "Invalid type for assignment." << std::endl;
        halt();
    }
}

void static_dispatch_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    expr->check_type(class_node, object_env, class_tbl);
    auto T_0 = expr->get_type();
    std::vector<Symbol> T1n;
    T1n.reserve(actual->len());
    for (int i = 0; i < actual->len(); ++i)
    {
        auto e_i = actual->nth(i);
        e_i->check_type(class_node, object_env, class_tbl);
        T1n.push_back(e_i->get_type());
    }

    if (!class_tbl.type_lte(T_0, type_name))
    {
        class_tbl.semant_error(class_node) << "Invalid type name for static dispatch" << std::endl;
        halt();
    }

    auto Mp = reference_method(class_node, type_name, name, class_tbl);
    for (int i = 0; i < Mp->get_formals()->len(); ++i)
    {
        if (!class_tbl.type_lte(T1n[i], Mp->get_formals()->nth(i)->get_type_decl()))
        {
            class_tbl.semant_error(class_node) << "Incompatible argument type for static dispatch" << std::endl;
            halt();
        }
    }
    auto Tpn1 = Mp->get_formals()->nth((int)T1n.size() - 1)->get_type_decl();
    set_type(Tpn1 == SELF_TYPE ? T_0 : Tpn1);
}

void dispatch_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    expr->check_type(class_node, object_env, class_tbl);
    auto T_0 = expr->get_type();
    std::vector<Symbol> T;
    T.reserve(actual->len());
    for (int i = 0; i < actual->len(); ++i)
    {
        auto e_i = actual->nth(i);
        e_i->check_type(class_node, object_env, class_tbl);
        T.push_back(e_i->get_type());
    }
    auto T_0p = T_0 == SELF_TYPE ? class_node : T_0;
    // method_class const *m_ptr = class_tbl.m_Table.at(T_0p).at(name);
    auto m_ptr = reference_method(class_node, T_0p, name, class_tbl);
    if (m_ptr->get_formals()->len() != (T.size())) // This '+ 1' might be wrong
    {
        class_tbl.semant_error(class_node) << "Invalid argument count for dispatch." << std::endl;
        halt();
    }

    for (int i = 0; i < T.size(); ++i)
    {
        if (!class_tbl.type_lt(T[i], m_ptr->get_formals()->nth(i)->get_type_decl()))
        {
            class_tbl.semant_error(class_node) << "Incompatible argument type for dispatch." << std::endl;
            halt();
        }
    }
    auto T_n1p = m_ptr->get_formals()->len() ? m_ptr->get_formals()->nth(T.size())->get_type_decl(): ;
    auto T_n1 = (T_n1p == SELF_TYPE ? T_0 : T_n1p);
    set_type(T_n1);
}

void cond_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    pred->check_type(class_node, object_env, class_tbl);
    if (pred->get_type() != Bool)
    {
        class_tbl.semant_error(class_node) << "Predicate type must be Bool" << std::endl;
        halt();
    }
    then_exp->check_type(class_node, object_env, class_tbl);
    else_exp->check_type(class_node, object_env, class_tbl);

    auto T2 = then_exp->get_type();
    auto T3 = else_exp->get_type();
    type = class_tbl.lca({T2, T3});
}

void loop_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    pred->check_type(class_node, object_env, class_tbl);
    if (pred->get_type() != Bool)
    {
        class_tbl.semant_error(class_node) << "Loop predicate was not of type Boolean" << std::endl;
        halt();
    }
    body->check_type(class_node, object_env, class_tbl);
    type = Object;
}

void branch_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    object_env.enterscope();
    object_env.addid(name, type_decl);
    expr->check_type(class_node, object_env, class_tbl);
    object_env.exitscope();
}

void typcase_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    std::unordered_set<Symbol> decl_types;
    std::unordered_set<Symbol> output_types;
    expr->check_type(class_node, object_env, class_tbl);
    for (int i = 0; i < cases->len(); ++i)
    {
        cases->nth(i)->check_type(class_node, object_env, class_tbl);
        auto Ti = cases->nth(i)->get_type_decl();
        if (decl_types.count(Ti))
        {
            class_tbl.semant_error(class_node) << "Non-unique type declaration in typcase." << std::endl;
            halt();
        }
        decl_types.insert(Ti);
        output_types.insert(cases->nth(i)->get_expr()->get_type());
    }
    set_type(class_tbl.lca(output_types));
}

void block_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    for (int i = 0; i < body->len(); ++i)
    {
        body->nth(i)->check_type(class_node, object_env, class_tbl);
    }
    type = body->nth(body->len() - 1)->get_type();
}

void let_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{

    if (init)
    {
        auto T_0p{type_decl == SELF_TYPE ? class_node : type_decl};
        init->check_type(class_node, object_env, class_tbl);
        auto T_1{init->get_type()};
        if (!class_tbl.type_lte(T_1, T_0p))
        {
            class_tbl.semant_error(class_node) << "Incompatible types for let initialization" << std::endl;
            halt();
        }
        object_env.enterscope();
        add_object(class_node, identifier, T_0p, object_env, class_tbl);
        body->check_type(class_node, object_env, class_tbl);
        auto T_2 = body->get_type();
        set_type(T_2);
        object_env.exitscope();
    }
    else
    {
        auto T_0p{type_decl == SELF_TYPE ? class_node : type_decl};
        object_env.enterscope();
        add_object(class_node, identifier, T_0p, object_env, class_tbl);
        body->check_type(class_node, object_env, class_tbl);
        auto T_1 = body->get_type();
        set_type(T_1);
        object_env.exitscope();
    }
}

void arith_type_check(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl,
                      Expression e1, Expression e2)
{
    e1->check_type(class_node, object_env, class_tbl);
    e2->check_type(class_node, object_env, class_tbl);
    if (e1->get_type() != Int)
    {
        class_tbl.semant_error(class_node) << "Type of left arith argument not Int." << std::endl;
    }

    if (e2->get_type() != Int)
    {
        class_tbl.semant_error(class_node) << "Type of right arith argument not Int." << std::endl;
    }
}

void plus_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    arith_type_check(class_node, object_env, class_tbl, e1, e2);
    set_type(Int);
}

void sub_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    arith_type_check(class_node, object_env, class_tbl, e1, e2);
    set_type(Int);
}

void mul_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    arith_type_check(class_node, object_env, class_tbl, e1, e2);
    set_type(Int);
}

void divide_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    arith_type_check(class_node, object_env, class_tbl, e1, e2);
    set_type(Int);
}

void neg_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    e1->check_type(class_node, object_env, class_tbl);
    if (e1->get_type() != Int)
    {
        class_tbl.semant_error(class_node) << "Argument for Negation not Int." << std::endl;
    }
    set_type(Int);
}

void compare_type_check(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl, Expression e1, Expression e2)
{
    e1->check_type(class_node, object_env, class_tbl);
    e2->check_type(class_node, object_env, class_tbl);
    if (e1->get_type() != Int)
    {
        class_tbl.semant_error(class_node) << "Left argument for Comparison not Int." << std::endl;
    }
    if (e2->get_type() != Int)
    {
        class_tbl.semant_error(class_node) << "Right argument for Comparison not Int." << std::endl;
    }
}

void lt_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    compare_type_check(class_node, object_env, class_tbl, e1, e2);
    set_type(Bool);
}

void eq_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    e1->check_type(class_node, object_env, class_tbl);
    e2->check_type(class_node, object_env, class_tbl);
    auto T1 = e1->get_type();
    auto T2 = e2->get_type();
    if (((T1 == Int || T1 == Str || T1 == Bool) ||
         (T2 == Int || T2 == Str || T2 == Bool)) &&
        T1 != T2)
    {
        class_tbl.semant_error(class_node) << "Objects with type Int, String, or Bool can only be compared with others of the same type." << std::endl;
    }
    set_type(Bool);
}

void leq_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    compare_type_check(class_node, object_env, class_tbl, e1, e2);
    set_type(Bool);
}

void comp_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    e1->check_type(class_node, object_env, class_tbl);
    if (e1->get_type() != Bool)
    {
        class_tbl.semant_error(class_node) << "Complement argument must be of type Bool" << std::endl;
    }
    set_type(Bool);
}

void int_const_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    set_type(Int);
}

void bool_const_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    set_type(Bool);
}

void string_const_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    set_type(Str);
}
void new__class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    set_type(type_name == SELF_TYPE ? class_node : type_name);
}

void isvoid_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    e1->check_type(class_node, object_env, class_tbl);
    set_type(Bool);
}

void no_expr_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    set_type(No_type);
}

void object_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    set_type(reference_object(class_node, name, object_env, class_tbl));
}

void attr_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    if (this->name == self)
    {
        class_tbl.semant_error(class_tbl.sym_class.at(class_node)) << "Attribute named self not allowed." << std::endl;
        halt();
    }
    auto T_0 = reference_object(class_node, name, object_env, class_tbl);
    if (T_0 != get_type_decl())
    {
        class_tbl.semant_error(class_tbl.sym_class.at(class_node)) << "Attr-No-Init type mismatch" << std::endl;
    }
    if (!is_none_type(init))
    {
        object_env.enterscope();
        add_object(class_node, self, class_node, object_env, class_tbl);
        this->init->check_type(class_node, object_env, class_tbl);
        auto T_1 = init->get_type();
        object_env.exitscope();
        if (!class_tbl.type_lte(T_1, T_0))
        {
            class_tbl.semant_error(class_tbl.sym_class.at(class_node)) << "Attr-Init incompatible expression and declared type." << std::endl;
        }
    }
}

void method_class::check_type(Symbol class_node, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    if (is_dfl_class(class_node))
        return;
    auto M = reference_method(class_node, class_node, name, class_tbl);
    auto T_0 = M->get_formals()->len() ? M->get_formals()->nth(M->get_formals()->len() - 1)->get_type_decl() : Object;
    object_env.enterscope();
    add_object(class_node, self, class_node, object_env, class_tbl);
    for (int i = 0; i + 1 < M->get_formals()->len(); ++i)
    {
        auto arg = M->get_formals()->nth(i);
        add_object(class_node, arg->get_name(), arg->get_type_decl(), object_env, class_tbl);
    }
    expr->check_type(class_node, object_env, class_tbl);
    auto T_0p = expr->get_type();
    object_env.exitscope();
    auto T_c = (T_0 == SELF_TYPE ? class_node : T_0);

    if (!class_tbl.type_lte(T_0p, T_c))
    {
        class_tbl.semant_error(class_tbl.sym_class.at(class_node)) << "Incompatible declared method return type and actual expression deduced type." << std::endl;
    }
}

inline void ClassTable::add_edge(Symbol class_id, Symbol parent_id)
{
    graph[parent_id].push_back(class_id);
    if (!graph.count(class_id))
        graph[class_id] = std::vector<Symbol>();
}

void ClassTable::install_basic_classes()
{

    // The tree package uses these globals to annotate the classes built below.
    // curr_lineno  = 0;
    Symbol filename = stringtable.add_string("<basic class>");

    // The following demonstrates how to create dummy parse trees to
    // refer to basic Cool classes.  There's no need for method
    // bodies -- these are already built into the runtime system.

    // IMPORTANT: The results of the following expressions are
    // stored in local variables.  You will want to do something
    // with those variables at the end of this method to make this
    // code meaningful.

    //
    // The Object class has no parent class. Its methods are
    //        abort() : Object    aborts the program
    //        type_name() : Str   returns a string representation of class name
    //        copy() : SELF_TYPE  returns a copy of the object
    //
    // There is no need for method bodies in the basic classes---these
    // are already built in to the runtime system.

    Class_ Object_class =
        class_(Object,
               No_class,
               append_Features(
                   append_Features(
                       single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
                       single_Features(method(type_name, nil_Formals(), Str, no_expr()))),
                   single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
               filename);

    //
    // The IO class inherits from Object. Its methods are
    //        out_string(Str) : SELF_TYPE       writes a string to the output
    //        out_int(Int) : SELF_TYPE            "    an int    "  "     "
    //        in_string() : Str                 reads a string from the input
    //        in_int() : Int                      "   an int     "  "     "
    //
    Class_ IO_class =
        class_(IO,
               Object,
               append_Features(
                   append_Features(
                       append_Features(
                           single_Features(method(out_string, single_Formals(formal(arg, Str)),
                                                  SELF_TYPE, no_expr())),
                           single_Features(method(out_int, single_Formals(formal(arg, Int)),
                                                  SELF_TYPE, no_expr()))),
                       single_Features(method(in_string, nil_Formals(), Str, no_expr()))),
                   single_Features(method(in_int, nil_Formals(), Int, no_expr()))),
               filename);

    //
    // The Int class has no methods and only a single attribute, the
    // "val" for the integer.
    //
    Class_ Int_class =
        class_(Int,
               Object,
               single_Features(attr(val, prim_slot, no_expr())),
               filename);

    //
    // Bool also has only the "val" slot.
    //
    Class_ Bool_class =
        class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())), filename);

    //
    // The class Str has a number of slots and operations:
    //       val                                  the length of the string
    //       str_field                            the string itself
    //       length() : Int                       returns length of the string
    //       concat(arg: Str) : Str               performs string concatenation
    //       substr(arg: Int, arg2: Int): Str     substring selection
    //
    Class_ Str_class =
        class_(Str,
               Object,
               append_Features(
                   append_Features(
                       append_Features(
                           append_Features(
                               single_Features(attr(val, Int, no_expr())),
                               single_Features(attr(str_field, prim_slot, no_expr()))),
                           single_Features(method(length, nil_Formals(), Int, no_expr()))),
                       single_Features(method(concat,
                                              single_Formals(formal(arg, Str)),
                                              Str,
                                              no_expr()))),
                   single_Features(method(substr,
                                          append_Formals(single_Formals(formal(arg, Int)),
                                                         single_Formals(formal(arg2, Int))),
                                          Str,
                                          no_expr()))),
               filename);

    sym_class[Object] = Object_class;
    sym_class[IO] = IO_class;
    add_edge(IO, IO_class->get_parent_name());
    sym_class[Int] = Int_class;
    add_edge(Int, Int_class->get_parent_name());
    sym_class[Bool] = Bool_class;
    add_edge(Bool, Bool_class->get_parent_name());
    sym_class[Str] = Str_class;
    add_edge(Str, Str_class->get_parent_name());
}

////////////////////////////////////////////////////////////////////
//
// semant_error is an overloaded function for reporting errors
// during semantic analysis.  There are three versions:
//
//    ostream& ClassTable::semant_error()
//
//    ostream& ClassTable::semant_error(Class_ c)
//       print line number and filename for `c'
//
//    ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
//       print a line number and filename
//
///////////////////////////////////////////////////////////////////

ostream &ClassTable::semant_error(Class_ c) const
{
    return semant_error(c->get_filename(), c);
}

ostream &ClassTable::semant_error(Symbol filename, tree_node *t) const
{
    error_stream << filename << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream &ClassTable::semant_error() const
{
    semant_errors++;
    return error_stream;
}

ostream &ClassTable::semant_error(Symbol c) const
{
    return semant_error(sym_class.at(c));
}

inline void add_object(Symbol class_node, Symbol id, Symbol type, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    if (object_env.probe(id))
    {
        class_tbl.semant_error(class_node) << "Illegal redefinition of variable." << std::endl;
        halt();
    }
    object_env.addid(id, type);
}

inline Symbol reference_object(Symbol class_node, Symbol id, ObjectEnv &object_env, ClassTable const &class_tbl)
{
    auto T = object_env.lookup(id);
    if (!T)
    {
        class_tbl.semant_error(class_node) << "Reference to undefined object identifier." << std::endl;
        halt();
    }
    return T;
}

inline method_class const *reference_method(Symbol class_node, Symbol C, Symbol f, ClassTable const &class_tbl)
{
    if (class_tbl.m_Table.count(C) &&
        class_tbl.m_Table.at(C).count(f))
    {
        return class_tbl.m_Table.at(C).at(f);
    }
    if (C == Object)
    {
        class_tbl.semant_error(class_node) << "Reference to undefined method." << std::endl;

        halt();
        return nullptr;
    }
    return reference_method(class_node, class_tbl.sym_class.at(C)->get_parent_name(), f, class_tbl);
}

inline void add_method(Symbol class_node, Symbol C, Symbol f, ClassTable &class_tbl, method_class const *m)
{
    if (class_tbl.m_Table.count(C) &&
        class_tbl.m_Table.at(C).count(f))
    {
        class_tbl.semant_error(class_node) << "Redefinition of method in same class." << std::endl;

        halt();
    }
    class_tbl.m_Table[C][f] = m;
}

inline void halt()
{
    cerr << "Compilation halted due to static semantic errors." << std::endl;
    exit(1);
}

inline void halt(ClassTable const *classtable)
{
    if (classtable->errors())
    {
        cerr << "Compilation halted due to static semantic errors." << std::endl;
        exit(1);
    }
}

void ClassTable::check_type()
{
    ObjectEnv object_env;
    check_type(object_env, Object);
}

void ClassTable::check_type(ObjectEnv &object_env, Symbol class_node)
{
    object_env.enterscope();
    Features features = sym_class.at(class_node)->get_features();
    for (int i = 0; i < features->len(); ++i)
    {
        if (attr_class *a = dynamic_cast<attr_class *>(features->nth(i)); a)
        {
            add_object(class_node, a->get_name(), a->get_type_decl(), object_env, *this);
        }
    }

    for (int i = 0; i < features->len(); ++i)
    {
        Feature f = features->nth(i);
        if (method_class const *m = dynamic_cast<method_class const *>(f); m)
        {
            add_method(class_node, class_node, m->get_name(), *this, m);
        }
    }

    for (int i = 0; i < features->len(); ++i)
    {
        Feature f = features->nth(i);
        f->check_type(class_node, object_env, *this);
    }

    for (Symbol child : graph.at(class_node))
    {
        check_type(object_env, child);
    }
    object_env.exitscope();
}
/*   This is the entry point to the semantic checker.

     Your checker should do the following two things:

     1) Check that the program is semantically correct
     2) Decorate the abstract syntax tree with type information
        by setting the `type' field in each Expression node.
        (see `tree.h')

     You are free to first do 1), make sure you catch all semantic
     errors. Part 2) can be done in a second stage, when you want
     to build mycoolc.
 */

void program_class::semant()
{
    initialize_constants();

    /* ClassTable constructor may do some semantic analysis */
    ClassTable *classtable = new ClassTable(classes);
    halt(classtable);
    classtable->check_hierarchy();
    halt(classtable);
    classtable->check_type();
    halt(classtable);

    /* some semantic analysis code may go here */
}
