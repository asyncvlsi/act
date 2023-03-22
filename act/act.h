/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#ifndef __ACT_H__
#define __ACT_H__

#include <act/types.h>
#include <act/lang.h>
#include <act/body.h>
#include <act/value.h>
#include <common/config.h>
#include <map>
#include <unordered_set>

/**
  @mainpage The ACT Library
 
This contains the documentation for the APIs provided by the core
ACT library, available via github at
https://github.com/asyncvlsi/act The top-level API is provided by
the Act class. This class is used to read in an ACT design file and
provides basic operations such as merging in additional files into
the design and expanding the design. After a file is read in, core
data structures that represent the entire design are created and
available for manipulation.  The core data structures represent all
aspects of the design in a hierarchical fashion.

The basic usage of the library is the following:

- Read in the ACT files to be processed.
- Expand the design.
- Access the expanded circuit definitions, potentially transforming them and/or generating output based on the specified design.
 
## Reading in files

In this step the ACT files are parsed, and the syntax is type-checked
to identify potential errors in the input. Files may be rejected at
this stage, in which case the library will exit with a fatal error
message along with information to help the user identify the location
and cause of the error.

To read in a design contained in the ACT file ``test.act``, use
```
Act *a = new Act ("test.act");
```
The returned data structure can be used to access the entire design
hierarchy.

All instances and user-defined types are associated with a _namespace_
(represented using the ActNamespace class). In the absence of a
namespace specifier, instances/types are part of the default global
namespace that is always present in the ACT data structures.

```
ActNamespace *ns = a->Global();
```

The global namespace can also be accessed using the static method
ActNamespace::Global() for convenience in case the Act pointer is
unavailable.

If an ACT file contains multiple namespaces, then a specific
namespaces can be accessed as follows:
```
ActNamespace *arith_ns = a->findNamespace ("arithmetic");
```

The names of user-defined types (processes, channels, data types) at this stage in the design correspond to the name in their ACT definitions. For example, the process
```
template<pint N>
defproc adder (bool a[N], b[N], out[N]) { ... }
```
can be found via its name `"adder"` within the namespace in which the
type is defined.

If this was defined in the global namespace and included in the
`test.act`, then the data structure for the process can be accessed
using
```
Process *p = a->findProcess ("adder");
```

If, instead, the adder was defined within the arithmetic namespace,
then it can be accessed using
```
Process *p = a->findProcess (arith_ns, "adder");
```


## Expanding the design

In this step, the design is expanded/elaborated. ACT permits designs
to be specified in a parameterized fashion. For example, a user could
design an N-bit adder. In this step, all parameters are expanded out
into their actual values, and the values are substituted throughout
the design. At this stage, all array dimensions are finalized, and so
any incompatibilities in array sizes/connections can also result in an
error being reported. Other errors that can be reported in this stage
include assertion failures, as well as type override errors.

At this stage, the expanded types can be found in the ACT data
structures. For example, if the design included an instance of type
`"adder<8>"`, then the string `"adder<8>"` will get added to the
namespace definition, and this corresponds to the definition of the
adder with the value `8` substituted for `N` throughout its
definition. For a type definition without any template parameters, the
expanded definitions will have empty angle brackets after them. For
example, an and gate called ``AND2X1`` without any parameters will be
expanded to `AND2X1<>`. Both the unexpanded definitions and expanded
definitions can be found in the namespace.

Integer template parameters are added to the type name for expanded
types as described above. Multiple parameters will be separated by
commas. Boolean template parameters are printed as either `t` or
`f` (for true and false), and real parameters appear as real
numbers. Arrays of parameters are specified using curly braces to
demarcate the array, followed by a comma-separated list of array
element values. This is used for both one-dimensional as well as
multi-dimensional arrays, since the number of dimensions and the shape
of the array is known.

In addition to substituting values of parameters, the expansion
process will also eliminate all conditional and loop constructs from
any definitions. This means that an expanded type will contain data
structures corresponding to the names of instances and their expanded
types, connection information, and expanded sub-language definitions
(if any).


## Name mangling

Expanded ACT type names can contain characters like `<`, `>`, and
`,` (among others). Instances can have names like `a[3].b[5].z`,
including the characters `[`, `]`, and `.`.  When exporting an
ACT design to another format for use by a third-party tool, names with
such characters in them can be potentially problematic. A good example
of this is when exporting a SPICE netlist---different versions of
SPICE have different syntactic restrictions. To handle this in a
disciplined manner, the ACT library has the notion of a //mangled//
name. A mangled name is generated by re-writing a user-specified list
of special characters with an underscore and a number/character
combination. This mapping is invertible, so a name can be unmangled as
well. The set of characters to be mangled is specified in the ACT
configuration option `act.mangle_string`.

Process names have a special case in terms of name mangling. If an
expanded process has no parameters, its mangled name is obtained
simply be omitting the trailing `<>`.


## Passes

Most of the work in manipulating a design operates on the expanded ACT
description.


 */

/**
 * @file act.h
 *       Contains top-level initialization/management functions for
 *       the ACT library
 */

class ActPass;
class Log;


/*
 *
 *  ACT can model a circuit at different levels of detail. The four
 *  levels are specified below.
 *
 *  The default level is the most detailed model available that can be
 *  provided by the user (PRS)
 *
 */

#define ACT_MODEL_CHP  0		// CHP language
#define ACT_MODEL_HSE  1		// HSE language
#define ACT_MODEL_PRS  2		// PRS language
#define ACT_MODEL_DEVICE 3              // PRS + sizing translated into netlist
#define ACT_MODEL_TOTAL 4

extern const char *act_model_names[];


/**
 *   The main Act class used to read in an ACT file and create basic
 *   data structures
 */
class Act {
 public:
  /**
   * Initialize the ACT library
   *
   *  @param argc is a pointer to argc (command-line processing)
   *  @param argv is a pointer to argv (command-line processing)
   *
   *  @return *argc and *argv are modified to reflect the command-line
   *  options left after ACT has extracted the ones it understands
   */
  static void Init (int *argc, char ***argv, const char *optional_conf);
  static void Init (int *argc, char ***argv, list_t *multi_conf = NULL);
  static int getOptions (int *argc, char ***argv);
  static void setOptionString (char *str);

  /**
   * Maximum depth of recursion for expanding types
   */
  static int max_recurse_depth;

  /**
   * Maximum number of iterations for general loops
   */
  static int max_loop_iterations;

#define WARNING_FLAG(x,y) \
  static int x ;
#include "warn.def"
  
  /**
   * Command-line arguments if -opt= is used
   */
  static list_t *cmdline_args;

  /**
   * Parser flags 
   */
  static int emit_depend;

  /**
   * Config debugging
   */
  static void config_info (const char *s);
  static void generic_msg (const char *s);


  /**
   * Create an act data structure for the specified input file
   *
   * @param s is the name of the file containing the top-level ACT. If
   * NULL, then the library is initialized without any ACT file being
   * read.
   */
  Act (const char *s = NULL);
  ~Act ();

  /** 
   * Merge in ACT file "s" into current ACT database
   *
   * @param s is the name of an ACT file
   */
  void Merge (const char *s);


  /**
   * Change global signal to use the port list
   *
   * @param s is the name of the global signal
   *    
   * @return true on success, false on error
   */
  bool LocalizeGlobal (const char *s);

  /**
   * Expand types
   */
  void Expand ();


  /**
   * Install string mangling functionality
   *
   * @param s is a string corresponding to the list of characters to
   * be mangled.
   */
  int mangle_set_char (unsigned char c);
  
  void mangle (char *s);

  int mangle_active() { return any_mangling; } /**< @return 1 if
						  mangling is active, 
						  0 otherwise */

  /** 
   * mangle string from src to dst.
   * @param src is the source string
   * @param dst is the destination string
   * @param sz is the space available in the destination string
   * @return 0 on success, -1 on error
   */  
  int mangle_string (const char *src, char *dst, int sz);
  
  /** 
   * unmangle string from src to dst.
   * @param src is the source string
   * @param dst is the destination string
   * @param sz is the space available in the destination string
   * @return 0 on success, -1 on error
   */  
  int unmangle_string (const char *src, char *dst, int sz);

  /**
   * Mangle fprintf functionality
   */
  void mfprintf (FILE *fp, const char *s, ...);

  /**
   * Unmangle fprintf functionality
   */
  void ufprintf (FILE *fp, const char *s, ...);

  /**
   * Mangle snprintf functionality
   */
  int msnprintf (char *fp, int sz, const char *s, ...);

  /**
   * Unmangle snprintf functionality
   */
  int usnprintf (char *fp, int sz, const char *s, ...);

  /**
   * Non-standard mangling for user-defined types.
   * @param omit_ns is 1 if you don't want to include the namespace in
   * the string
   */
  void msnprintfproc (char *fp, int sz, UserDef *, int omit_ns = 0);

  /**
   * Non-standard mangling for user-defined types
   * @param omit_ns is 1 if you don't want to include the namespace in
   * the string
   */
  void mfprintfproc (FILE *fp, UserDef *, int omit_ns = 0);

  /**
   * Unmangle string, assuming this is a process name and was mangled
   * with process mangling. Same arguments and return value as
   * unmangle_string.
   */
  int unmangle_stringproc (const char *src, char *dst, int sz);


  /* 
     API functions
  */

  /**
   * Find a process given a name
   * @param s is the name of the process
   * @param allow_expand if set to true, then if the process has angle
   * brackets then findProcess is allowed to expand the process
   * @return process pointer, or NULL if not found
   */
  Process *findProcess (const char *s, bool allow_expand = false);

  /**
   * Find a process within a namespace
   * @param s is the name of the process
   * @param ns is the ACT namespace
   * @return the process pointer if found, NULL otherwise
   */
  Process *findProcess (ActNamespace *ns, const char *s, bool allow_expand = false);

  /**
   * Find a user-defined type
   * @param s is the name of the type
   * @return the UserDef pointer if found, NULL otherwise
   */
  UserDef *findUserdef (const char *s);

  ActNamespace *findNamespace (const char *s);
  ActNamespace *findNamespace (ActNamespace *, const char *);
  ActNamespace *Global() { return gns; }

  /*
    Dump to a file 
  */
  void Print (FILE *fp);

  void pass_register (const char *name, ActPass *p);
  ActPass *pass_find (const char *name);
  void pass_unregister (const char *name);
  const char *pass_name (const char *name);

  /*
     Get a list of list of ValueIdx pointers that
     were generated during decomposition, if any
  */
  list_t *getDecomp (Process *p);

  /*
    Return a list of Process pointers that were generated during
    decomposition, if any
  */
  list_t *getDecompTypes ();

  /* 
    To mess with types after parsing
  */
  TypeFactory *getTypeFactory () { return tf; }


  /*
    Get modeling level of detail and refinement steps.
  */
  int getLevel ();		// get default level
  int getLevel (Process *p);	// get level for this type
  int getLevel (ActId *id);	// get level for this instance

  int getRefSteps() { return refine_steps; }
  void decRefSteps() { refine_steps--; }
  void incRefSteps() { refine_steps++; }

private:
  TypeFactory *tf;		/* type factory for the file */
  ActNamespace *gns;		/* global namespace */

  int mangle_characters[256];
  int inv_map[256];
  int any_mangling;
  int mangle_langle_idx;  /* index of '<' */
  int mangle_min_idx;     /* index of the min of , . { } */
  int mangle_mode;

  static Log *L;

  struct Hashtable *passes;	// any ActPass-es

  int refine_steps;		// number of refinement steps to
				// process

  int num_type_levels[ACT_MODEL_TOTAL];
  char **type_levels[ACT_MODEL_TOTAL];
  int num_inst_levels[ACT_MODEL_TOTAL];
  char **inst_levels[ACT_MODEL_TOTAL];

  int default_level;  // default level

  static char *_getopt_string;
  
  static int _process_act_arg (const char *argvp, int *tech_specified, char **conf);
};

class ActPass {
protected:
  int _finished;		// has the pass finished execution?
  int _sticky_visited;		// sticky visited flag
  Act *a;			// main act data structure
  list_t *deps;			// ActPass dependencies

  list_t *fwdeps;		// passes that depend on me
  
  const char *name;

  int _update_propagate;
  int _root_dirty;
  Process *_root;
  void *_global_info;
  
  
  virtual void _actual_update (Process *p);

public:
  ActPass (Act *_a, const char *name, int doroot = 0);
  // Create, initialize, and register pass
  // A pass sets "doroot" to 1 if any update to a process
  // propagates all the way back up to the root of the design.
  // We assume that the list of ports never change.
  
  virtual ~ActPass ();			       // release storage


  int AddDependency (const char *pass); // insert dependency on an
					// actpass

  /*-- re-compute information computed by this pass --*/
  void update (Process *p);

  int rundeps (Process *p = NULL);

  const char *getName();
  
  virtual int run(Process *p = NULL); // run pass on a process; NULL =
				      // top level
  
  int completed()  { return (_finished == 2) ? 1 : 0; }
  int pending()  { return (_finished == 1) ? 1 : 0; }
  void *getMap (Process *p);
  void *getGlobalInfo () { return _global_info; }
  Act *getAct () { return a; }
  ActPass *getPass (const char *name) { return a->pass_find (name); }

  Process *getRoot() { return _root; }

  /* -- negative modes are used internally; do not use! -- */
  virtual void run_recursive (Process *p = NULL, int mode = 0);

  void disableUpdate () { _update_propagate = 0; }
  void enableUpdate () { _update_propagate = 1; }


  void mkStickyVisited () { _sticky_visited = 1; }
  void clrStickyVisited() { _sticky_visited = 0; }

  static void refreshAll (Act *a, Process *p = NULL);

private:
  /* -- called before sub-tree -- */
  virtual void *pre_op (Process *p, int mode = 0);
  virtual void *pre_op (Channel *c, int mode = 0);
  virtual void *pre_op (Data *d, int mode = 0);

  /* -- called after sub-tree -- */
  virtual void *local_op (Process *p, int mode = 0);
  virtual void *local_op (Channel *c, int mode = 0);
  virtual void *local_op (Data *d, int mode = 0);

  /* - release storage (if any) -- */
  virtual void free_local (void *);

  int init (); // initialize or re-initialize
  void recursive_op (UserDef *p, int mode = 0);

  void init_map ();
  void free_map ();
  std::map<UserDef *, void *> *pmap;
  std::unordered_set<UserDef *> *visited_flag;
};

struct act_sh_passlib_info {
  char *lib;
  void *lib_ptr;
  int refs;
};

class Technology;
class ActDynamicPass;

struct act_sh_dispatch_table {
  void (*_init) (ActPass *ap);
  void (*_run) (ActPass *ap, Process *p);
  void (*_recursive) (ActPass *ap, Process *p, int mode);
  void *(*_proc) (ActPass *ap, Process *p, int mode);
  void *(*_chan) (ActPass *ap, Channel *c, int mode);
  void *(*_data) (ActPass *ap, Data *d, int mode);
  void (*_free) (ActPass *ap, void *v);
  void (*_done) (ActPass *ap);

  int (*_runcmd) (ActPass *ap, const char *name);
};

class ActDynamicPass : public ActPass {
public:
  ActDynamicPass (Act *_a, const char *name, const char *lib, const char *prefix);
  // load a dynamic pass from a shared object file
  
  ~ActDynamicPass ();		// release storage

  int run (Process *p = NULL);
  void run_recursive (Process *p = NULL, int mode = 0);

  void setParam (const char *name, void *v);
  void setParam (const char *name, int v);
  void setParam (const char *name, double v);
  int getIntParam (const char *name);
  void *getPtrParam (const char *name);
  double getRealParam (const char *name);
  bool hasParam (const char *name);

  int runcmd (const char *name);

  struct Hashtable *getConfig ();
  Technology *getTech () { return T; }

  bool loaded() { return _load_success; }
  
private:
  virtual void *local_op (Process *p, int mode = 0);
  virtual void *local_op (Channel *c, int mode = 0);
  virtual void *local_op (Data *d, int mode = 0);
  virtual void free_local (void *);

  bool _load_success;
  
  char *_libused;
  act_sh_dispatch_table _d;
  struct Hashtable *_params;
  struct Hashtable *_config_state;
  Technology *T;
  
  /* open shared object libraries */
  static list_t *_sh_libs;
};


/* this should be elsewhere */
Expr *const_expr (long);
Expr *const_expr_bool (int);
Expr *const_expr_real (double);


/*
   Should be called after Act::Init() to add global parameter
   definitions.

   MUST BE CALLED BEFORE an Act object is created!
*/
void act_add_global_pint (const char *name, int val);
void act_add_global_pbool (const char *name, int val);

#endif /* __ACT_H__ */
