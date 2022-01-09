

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <symtab.h>
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
    arg         = idtable.add_string("arg");
    arg2        = idtable.add_string("arg2");
    Bool        = idtable.add_string("Bool");
    concat      = idtable.add_string("concat");
    cool_abort  = idtable.add_string("abort");
    copy        = idtable.add_string("copy");
    Int         = idtable.add_string("Int");
    in_int      = idtable.add_string("in_int");
    in_string   = idtable.add_string("in_string");
    IO          = idtable.add_string("IO");
    length      = idtable.add_string("length");
    Main        = idtable.add_string("Main");
    main_meth   = idtable.add_string("main");
    //   _no_class is a symbol that can't be the name of any 
    //   user-defined class.
    No_class    = idtable.add_string("_no_class");
    No_type     = idtable.add_string("_no_type");
    Object      = idtable.add_string("Object");
    out_int     = idtable.add_string("out_int");
    out_string  = idtable.add_string("out_string");
    prim_slot   = idtable.add_string("_prim_slot");
    self        = idtable.add_string("self");
    SELF_TYPE   = idtable.add_string("SELF_TYPE");
    Str         = idtable.add_string("String");
    str_field   = idtable.add_string("_str_field");
    substr      = idtable.add_string("substr");
    type_name   = idtable.add_string("type_name");
    val         = idtable.add_string("_val");
}

void ClassTable::build_method_table() {
    std::unordered_map<Symbol, method_class const*> methods;
    build_method_table(methods, Object);
}

void ClassTable::build_method_table(std::unordered_map<Symbol,
        method_class const*>& methods, Symbol class_node) {
    
    std::unordered_map<Symbol, method_class const*> methods_save = methods;
    
    Features features = sym_class.at(class_node)->get_features();

    for (int i = 0; i < features->len(); ++i) {
        Feature f = features->nth(i);
        method_class const* m = dynamic_cast<method_class const*>(f);
        if (m)
            methods[m->get_name()] = m;
    }

    m_Table[class_node] = methods;
    for (Symbol child_class : graph[class_node])
        build_method_table(methods, child_class);
    methods_save.swap(methods);
}


void reachable(std::unordered_map<Symbol, std::vector<Symbol>> const& graph, 
        std::unordered_set<Symbol>& visited, Symbol node) {
    if (visited.count(node)) return;
    visited.insert(node);
    for (Symbol child : graph.at(node)) {
        reachable(graph, visited, child);
    }
}

void ClassTable::check_hierarchy(
        std::unordered_map<Symbol, method_class const*> methods,
        std::unordered_set<Symbol> members,
        Symbol class_node) const {
    
    Features features = sym_class.at(class_node)->get_features();
    std::unordered_map<Symbol, method_class const*> methods_save = methods;
    std::unordered_set<Symbol> members_save = members;

    for (int i = 0; i < features->len(); ++i) {
        Feature f = features->nth(i);
        attr_class const* a = dynamic_cast<attr_class const*>(f);
        if (a) {
            if (members.count(a->get_name())) {
                semant_error() << "Redefinition of attribute " <<
                    a->get_name() << " in sub class " << class_node << std::endl;
            } else {
                members.insert(a->get_name());
            }
            break;
        }

        method_class const* m = dynamic_cast<method_class const*>(f);
        if (m) {
            if (methods.count(m->get_name())) {
                method_class const* m_ref = methods.at(m->get_name());
                if (m->get_return_type() != m_ref->get_return_type()) {
                    semant_error() << "Invalid method overload return type" << std::endl;
                    continue;
                }
                Formals formals = m->get_formals();
                Formals formals_ref = m_ref->get_formals();
                if (formals->len() != formals_ref->len()) {
                    semant_error() << "Invalid method overload argument count" << std::endl;
                    continue;
                }

                for (int j = 0; j < formals->len(); ++j) {
                    Formal formal = formals->nth(j);
                    Formal formal_ref = formals_ref->nth(j);
                    if (formal->get_type_decl() != formal_ref->get_type_decl()) {
                        semant_error() << "Invalid method overload argument type" << std::endl;
                        break;
                    }
                }
            } else {
                methods[m->get_name()] = m;
            }
        }
    }

    for (Symbol child : graph.at(class_node)) {
        check_hierarchy(methods, members, child);
    }
    methods.swap(methods_save);
    members.swap(members_save);
}

void ClassTable::check_hierarchy() const {
    std::unordered_map<Symbol, method_class const*> methods;
    std::unordered_set<Symbol> members;
    check_hierarchy(methods, members, Object);
}


ClassTable::ClassTable(Classes def_classes) : semant_errors(0) , error_stream(cerr) {

    install_basic_classes();

    for (int i = 0; i < def_classes->len(); ++i) {
        Class_ cur_class = def_classes->nth(i);
        if (sym_class.count(cur_class->get_name())) {
            semant_error() << "Redefinition of class " << cur_class->get_name() << std::endl;
            return;
        }
        sym_class[cur_class->get_name()] = cur_class;
        add_edge(cur_class->get_name(), cur_class->get_parent_name());
    }

    {
        std::unordered_set<Symbol> visited;
        reachable(graph, visited, Object);
        if (visited.size() != sym_class.size()) {
            semant_error() << "cyclic inheritance found" << std::endl;
        }
    }
}

bool ClassTable::type_compare_inclusive(Symbol base_t, Symbol super_t) const {
    if (super_t == Object)
        return true;
    while(base_t != super_t && base_t != Object)
        base_t = sym_class.at(base_t)->get_parent_name();
    return base_t == super_t;
}

bool ClassTable::type_compare_exclusive(Symbol base_t, Symbol super_t) const {
    return base_t != super_t &&
            type_compare_inclusive(base_t, super_t);
}

void ClassTable::add_edge(Symbol class_id, Symbol parent_id) {
    graph[parent_id].push_back(class_id);
    if (!graph.count(class_id)) graph[class_id] = std::vector<Symbol>();
}

void ClassTable::type_check() const {
    ObjectEnv object_env;
    type_check(object_env, Object);
}

void ClassTable::type_check(ObjectEnv& object_env, Symbol class_node) const {
    
}

void ClassTable::install_basic_classes() {

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
	class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())),filename);

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

ostream& ClassTable::semant_error(Class_ c)
{                                                             
    return semant_error(c->get_filename(),c);
}    

ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
{
    error_stream << filename << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream& ClassTable::semant_error() const
{                                                 
    semant_errors++;                            
    return error_stream;
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
    classtable->check_hierarchy();
    if (classtable->errors())
        goto printerrors;
    classtable->build_method_table();
    /* some semantic analysis code may go here */

    printerrors:
        if (classtable->errors()) {
        cerr << "Compilation halted due to static semantic errors." << endl;
        exit(1);
    }
}


