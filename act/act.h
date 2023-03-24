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

1. Read in the ACT files to be processed.
2. Expand the design.
3. Access and potentially transform the expanded circuit definitions.

Before any of these steps, the Act library should be initialized via
the call:

```
Act::Init (&argc, &argv);
```

Here ``argc``/``argv`` are the standard C command-line arguments. The
common ACT command-line arguments are extracted by this process.
 
## Step 1: Reading in files

In this step the ACT files are parsed, and the syntax is type-checked
to identify potential errors in the input. Files may be rejected at
this stage, in which case the library will exit with a fatal error
message along with information to help the user identify the location
and cause of the error.

**Example:** To read in a design contained in the ACT file ``test.act``, use
```
Act *a = new Act ("test.act");
```
The returned data structure can be used to access the entire design
hierarchy. If additional ACT files need to be read in (beyond those
imported directly from the original ACT file), then additional files
can be merged into the Act data structure using
```
a->Merge ("extra.act");
```

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

template<pint N> defproc adder (bool a[N], b[N], out[N]) { ... }

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


## Step 2: Expanding the design

In this step, the design is expanded/elaborated. ACT permits designs
to be specified in a parameterized fashion. For example, a user could
design an N-bit adder. In this step, all parameters are expanded out
into their actual values, and the values are substituted throughout
the design. At this stage, all array dimensions are finalized, and so
any incompatibilities in array sizes/connections can also result in an
error being reported. Other errors that can be reported in this stage
include assertion failures, as well as type override errors.

To expand the Act data structure, use:
```

Act *a = new Act("test.act");

a->Expand();

```


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

Reading in expanded processes operates in the same way as earlier:

```
Process *p = a->findProcess ("adder<8>");
```

This will return an expanded process (or NULL if not found).

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


## Step 3: Access and manipulate designs

Most of the work in manipulating a design operates on the expanded ACT
description. This starts with accessing the data structure for a
namespace or user-defined type. While ``findProcess()`` can be used to
find a user-defined process, other user-defined types (channel and
data types) can be accessed using
```
UserDef *u = a->findUserdef ("datatypename");
```

The data structures for user-defined types have APIs that can be used
to access/manipulate the design.

A commonly used pattern is to apply some analysis/transformation to
every process/user-defined type in the user design, starting from some
top-level process name. Act has a built-in notion of an Act _pass_
(supported by the ActPass class) that includes traversal methods/etc.


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

The standard name mangling prefix character is an underscore. Name
mangling operates character-by-character as follows:

1. An underscore is replaced by two underscores.
2. A character that is not in the mangle string is a pass-through, so
it is not modified.
3. If a character is at position k in the name mangling string, it is
replaced with an underscore followed by k. The position character is 0
to 9 for positions 0 to 9, followed by a-z. Up to 26 characters can be
mangled.

Name mangling can at most double the length of the string. Various
APIs to mangle and unmangle names are part of the Act class, such as
Act::mfprintf() and Act::msnprintf().

Process names have a special case in terms of name mangling. If an
expanded process has no parameters, its mangled name is obtained
simply be omitting the trailing `<>`.

 */

/**
 * @file act.h
 *       Contains top-level initialization/management functions for
 *       the ACT library.
 *
 *  ACT can model a circuit at different levels of detail. The four
 *  levels are specified below.
 *
 *  The default level is the most detailed model available that can be
 *  provided by the user (PRS)
 *
 */
#define ACT_MODEL_CHP  0		///< Modeling level is the CHP language
#define ACT_MODEL_HSE  1	        ///< Modeling level is the HSE language
#define ACT_MODEL_PRS  2		///< Modeling level is the PRS language
#define ACT_MODEL_DEVICE 3              ///< Modeling level is device: PRS + sizing translated into netlist
#define ACT_MODEL_TOTAL 4               ///< The total number of modeling levels (used for error checking)


/**
 * the string name for each model level, indexed by the
 * ACT_MODEL_... macros
 */
extern const char *act_model_names[]; 

class ActPass;
class Log;

/**
 *   @class Act
 *
 *   @brief The main Act class used to read in an ACT file and create basic
 *   data structures. All design information can be accessed through this
 *   data structure.
 *
 */
class Act {
 public:
  /**
   * 
   * Initialize the ACT library. 
   *  *argc and *argv are modified to reflect the command-line
   *  options left after ACT has extracted the ones it understands
   *
   *  @param argc is a pointer to argc (command-line processing)
   *  @param argv is a pointer to argv (command-line processing)
   *  @param optional_conf is a file name for an optional
   *  configuration file that must be loaded as part of the
   *  initialization. A configation name can be of the form
   *  `prefix:file.conf`. Here the names in the configuration file are
   *  assumed to be enclosed in a `begin prefix`/`end` group.
   *
   */
  static void Init (int *argc, char ***argv, const char *optional_conf);

  /**
   * Another initialization method, supporting multiple configuration
   * files.
   *  *argc and *argv are modified to reflect the command-line
   *  options left after ACT has extracted the ones it understands
   *  
   *  @param argc is a pointer to argc (command-line processing)
   *  @param argv is a pointer to argv (command-line processing)
   *  @param multi_conf is a list of strings, each corresponding to a
   *  configuration file name (same as the simple Init method). All
   *  the configuration files are loaded as part of the initialization.
   * 
   */
  static void Init (int *argc, char ***argv, list_t *multi_conf = NULL);

  /**
   * When the ACT library is used as part of a scripting language, it
   * is sometimes useful for the script to initiate argument
   * processing. This function is used to process command-line
   * arguments, and create auxillary data structures to record the
   * options specified by the user. The option string (ala getopt())
   * is set by the setOptionString() method.
   *
   *  @param argc is a pointer to argc (command-line processing)
   *  @param argv is a pointer to argv (command-line processing)
   *
   *  @return 1 if this succeeded, 0 if there was an error during
   *  command-line argument processing.
   */
  static int getOptions (int *argc, char ***argv);

  /**
   * This function works together with getOptions() and is used to
   * specify the option string for command-line argument processing.
   *  
   * @param str is the getopt string for arguments
   *  
   */
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
   * Command-line arguments if -opt= is used. Used to record the
   * options from setOptionString().
   */
  static list_t *cmdline_args;

  /**
   * Parser flags. If this is set, then the names of .act files read
   * in during parsing are printed out. This is used by the `adepend`
   * tool, which can be useful when writing Makefiles.
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
   * Change global signal to use the port list throughout the design.
   *
   * @param s is the name of the global signal
   *    
   * @return true on success, false on error
   */
  bool LocalizeGlobal (const char *s);

  /**
   * Expand types. This expands the entire design, updating the data
   * structures stored in the Act class.
   */
  void Expand ();


  /**
   * Install string mangling functionality
   *
   * @param s is a string corresponding to the list of characters to
   * be mangled.
   */
  void mangle (char *s);

  /**
   * Specify the character used as the prefix for the name mangling
   * procedure.
   *
   * @param c is the character used as the name mangling character.
   */
  int mangle_set_char (unsigned char c);

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
   * Mangle fprintf functionality. This provides an fprintf() API,
   * except the output is mangled.
   *
   * @param fp output file pointer
   * @param s  format string
   *
   */
  void mfprintf (FILE *fp, const char *s, ...);

  /**
   * Unmangle fprintf functionality. This provides an fprintf() API,
   * except the output is unmangled.
   *
   * @param fp output file pointer
   * @param s  format string
   *
   */
  void ufprintf (FILE *fp, const char *s, ...);

  /**
   * Mangle snprintf functionality. This provides an snprintf() API,
   * except the output is mangled.
   *
   * @param fp output string buffer
   * @param sz size of the output buffer
   * @param s  format string
   *
   * @return snprintf() result
   */
  int msnprintf (char *fp, int sz, const char *s, ...);

  /**
   * Unmangle snprintf functionality. This provides an snprintf() API,
   * except the output is mangled.
   *
   * @param fp output string buffer
   * @param sz size of the output buffer
   * @param s  format string
   *
   * @return snprintf() result
   */
  int usnprintf (char *fp, int sz, const char *s, ...);

  /**
   * Non-standard mangling for user-defined types.
   *
   * @param fp is the output buffer
   * @param sz is the size of the output buffer
   * @param u is the user-defined type
   * @param omit_ns is 1 if you don't want to include the namespace in
   * the string
   */
  void msnprintfproc (char *fp, int sz, UserDef *u, int omit_ns = 0);

  /**
   * Non-standard mangling for user-defined types
   * @param fp is the output file
   * @param u is the user-defined type
   * @param omit_ns is 1 if you don't want to include the namespace in
   * the string
   */
  void mfprintfproc (FILE *fp, UserDef *u, int omit_ns = 0);

  /**
   * Unmangle string, assuming this is a process name and was mangled
   * with process mangling. Same arguments and return value as
   * unmangle_string().
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
   * @param allow_expand if set to true, then if the process has angle
   * brackets then findProcess is allowed to expand the process
   * @return the process pointer if found, NULL otherwise
   */
  Process *findProcess (ActNamespace *ns, const char *s, bool allow_expand = false);

  /**
   * Find a user-defined type
   * @param s is the name of the type
   * @return the UserDef pointer if found, NULL otherwise
   */
  UserDef *findUserdef (const char *s);

  /**
   * Find a namespace.
   * @param s is the name of the namespace
   * @return the ActNamespace pointer if found, NULL otherwise
   */
  ActNamespace *findNamespace (const char *s);
  
  /**
   * Find a namespace nested within another namespace
   * @param ns is the parent namespace
   * @param s is the name of the nested namespace
   * @return the ActNamespace pointer if found, NULL otherwise
   */
  ActNamespace *findNamespace (ActNamespace *ns, const char *s);

  
  /**
   * @return the ActNamespace pointer for the global namespace
   */
  ActNamespace *Global() { return gns; }

  /**
   * Prints the entire ACT data structure to a file.
   *
   * @param fp is the file pointer to which the output is written.
   */
  void Print (FILE *fp);

  /**
   * Register an ActPass with the ACT library
   *
   * @param name is the name of the pass
   * @param p is the ActPass to be registered
   */
  void pass_register (const char *name, ActPass *p);

  /**
   * Search for a pass by its registered name.
   *
   * @param name is the name of the pass
   * @return the ActPass that is registered by the specified name,
   * NULL if the pass does not exist.
   */
  ActPass *pass_find (const char *name);
  
  /**
   * Remove a previosly registered ActPass from the ACT library
   *
   * @param name is the name of the pass
   */
  void pass_unregister (const char *name);
  
  /**
   * @return an immutable string that corresponds to the pass name,
   * NULL if the pass does not exist
   *
   * @param name is the name of the pass of interest
   */
  const char *pass_name (const char *name);

  /**
   * Certain ACT passes can re-write the ACT and introduce decomposed
   * processes. Currently the chpmem and chparb passes have this
   * property, as they extract parts of the CHP and introduce new
   * processes/instances. This method returns a list of list of
   * ValueIdx pointers generated by the decomposition passes.
   *
   * @param p is the Process to query for any decomposed components
   * @return a list of list of ValueIdx pointers. The list could be
   * NULL if nothing exists.
   */
  list_t *getDecomp (Process *p);

  /**
   * @return a list of Process pointers that were generated during
   * decomposition, if any. See the getDecomp() method for more details.
  */
  list_t *getDecompTypes ();

  /**
   * Returns the type factory used to manipulate/create types.
   * @return the TypeFactory used by the ACT library.
  */
  TypeFactory *getTypeFactory () { return tf; }

  /*
    Get modeling level of detail and refinement steps.
  */
  int getLevel ();		///< get default level
  int getLevel (Process *p);	///< get level for this type
  int getLevel (ActId *id);	///< get level for this instance

  int getRefSteps() { return refine_steps; } ///< return the remaining
					     ///refinement steps
  void decRefSteps() { refine_steps--; } ///< decrement the number of
					 ///remaining refinement steps
  
  void incRefSteps() { refine_steps++; } ///< increment the number of
					 ///remaining refinement steps

private:
  TypeFactory *tf;		///< type factory for the Act instance
  ActNamespace *gns;		///< the global namespace pointer

  
  int mangle_characters[256];   ///< which characters do we mangle?
  int inv_map[256];             ///< map used for inverse lookup
  int any_mangling;            ///< 1 if there is any name mangling, 0 otherwise

#if 0
  int mangle_langle_idx;  /* index of '<' */
  int mangle_min_idx;     /* index of the min of , . { } */
  int mangle_mode;
#endif  

  static Log *L;  ///< Used for log messages

  struct Hashtable *passes;	///< Table of any ActPass-es

  int refine_steps;		///< number of refinement steps to
				/// process

  int num_type_levels[ACT_MODEL_TOTAL]; ///< read from configuration
					///file, corresponds to the
					///size of the type table for
					///each modeling level
  
  char **type_levels[ACT_MODEL_TOTAL];  ///< read from configuration
					///file, the type string table
					///for each modeling level
  
  int num_inst_levels[ACT_MODEL_TOTAL]; ///< read from configuration
					///file, corresponds to the
					///size of the instance table
					///for each modeling level
  
  char **inst_levels[ACT_MODEL_TOTAL]; ///< read from configuration
					///file, corresponds to the
					///instance string table
					///for each modeling level

  int default_level;  ///< the default modeling level

  static char *_getopt_string; ///< the string used for getopt

  /**
   * Internal function used to actually process an argument, used to
   * avoid replicating code.
   */
  static int _process_act_arg (const char *argvp, int *tech_specified, char **conf);
};

/**
 *   @class ActPass
 *
 *   @brief The main ActPass class used to implement an ACT
 *   analysis/synthesis pass. All the core tools use this framework to
 *   operate on the ACT data structures. Passes are called once the
 *   design has been expanded.
 *
 *   The basic operation of a pass can modify the internals of a
 *   user-defined type, and/or compute some auxillary information that
 *   is saved away in a data structure that is indexed by the
 *   user-defined type pointer. This data can be accessed after a pass
 *   has been run.
 *
 *   A pass operates recursively on the design/type hierarchy. If a
 *   user-defined type instantiates another user-defined type, then
 *   the types for each instance are processed before the parent
 *   type. Hence, information computed about the instances are
 *   available to the parent. Each user-defined type that is part of
 *   the design hierarchy is visited exactly once during the design
 *   traversal.
 *
 *   A pass can also dpeend on information computed by another
 *   pass. These dependencies are explicitly specified in the pass
 *   constructor using the AddDependency() call. This tracks both
 *   forward and backward dependencies---i.e. other passes that depend
 *   on the ActPass, and other passes that the ActPass depend on. A
 *   cyclic dependency is detected and reported as an error.
 *
 *   When a pass is run via ActPass::run() , any of its dependencies
 *   are automatically executed (if they have not already been
 *   run). Additionally, it is possible for a pass to run any passes
 *   that depend on what has been re-computed via the
 *   ActPass::update() call.
 *
 */
class ActPass {
protected:
  int _finished;		///< has the pass finished execution?
  
  int _sticky_visited;		///< sticky visited flag. Normally
				///when a pass is run, it keeps track
				///of types that have been
				///visited. When the run is complete,
				///all the visited flags are
				///cleared. Setting this flag forces
				///the pass to preserve the visited
				///flags across multiple runs. This is
				///useful when used in conjunction
				///with run_recursive(). In particular
				///it is used when multiple
				///run_recursive() calls are made
				///rather than a single call at the
				///root of the design hierarchy.
  
  Act *a;			///< main act data structure to which
				///the pass is linked
  
  list_t *deps;			///< ActPass dependencies: these are
				///passes that must be run prior to
				///the pass being executed.

  list_t *fwdeps;		///< passes that depend on me. These
				///are passes whose operation might be
				///influenced by the computation from
				///this pass.
  
  const char *name;		///< the name for the pass

  int _update_propagate;	///< flag used to suppress updates from
				///propagating. The default is 1,
				///which means any updates will
				///automatically propagate.
  
  int _root_dirty;		///< if this flag is set, then even if
				///an update propagates from a certain
				///process in the design hierarchy, it
				///influences the entire design so all
				///data computed for all components of
				///the design must be re-computed.
  
  Process *_root;		///< the root of the design
  
  void *_global_info;		///< this space is for rent. It is
				///used by the decomposition passes to
				///save away type information.
  

  /**
   * This performs the actual pass re-computation for a particular process 
   *
   * @param p is the process whose pass data is to be recomputed
   */
  virtual void _actual_update (Process *p);

  /**
   * This function sould only be called in the constructor. It
   * specifies that execution of the pass depends on
   * data/transformations from another ActPass. This dependency is
   * added to the dependency-tracking mechanism built-into the ActPass
   * structures.
   *
   * @param pass is the name of the other ActPass this one depends on
   * @return should always return 1.
   */
  int AddDependency (const char *pass);

public:
  /**
   * Create, initialize, and register pass
   * A pass sets "doroot" to 1 if any update to a process
   * propagates all the way back up to the root of the design.
   *
   * @param _a the Act data structure where this pass should be added
   * @param name the name of the pass
   * @param doroot should be set to 1 if any updates must propagate
   * from the root of the entire design, rather than from the updated
   * process.
   */
  ActPass (Act *_a, const char *name, int doroot = 0);
  
  virtual ~ActPass ();			       ///< release storage


  /**
   * Propagate the updates from the current pass to all passes that
   * have previously been run and that are dependencies of this
   * pass (either direct or indirect). This function runs all downstream
   * dependencies in topological order. Passes with updates suppressed
   * are omitted from the update operation.
   *
   * @param p is the process in the design that was updated.
   */
  void update (Process *p);

  /**
   * Run all passes that this one depends on.
   *
   * @param p is the process that is the root of the design where
   * dependencies are computed.
   * @return should always return 1. If there is an error due to a
   * cyclic dependency, then the library will exit.
   */
  int rundeps (Process *p = NULL);

  /**
   * Used to determine which pass this might be.
   * @return the name of the pass
   */
  const char *getName();

  /**
   * Run the entire pass with the specified top-level process name
   * @param p is the tyoe of the top-level process; NULL means that
   * the top-level of the design is the global namespace and not a
   * specific process.
   * @return 1 on success, 0 if any dependencies failed.
   */
  virtual int run(Process *p = NULL); // run pass on a process; NULL =
				      // top level


  /**
   * @return 1 if a pass has completed execution, 0 otherwise
   */
  int completed()  { return (_finished == 2) ? 1 : 0; }

  /**
   * @return 1 if a pass is currently being executed, 0 otherwise
   */
  int pending()  { return (_finished == 1) ? 1 : 0; }

  /**
   * A pass computes a per-process data structure, and this method 
   * used to access it.
   *
   * @param u is the user-defined type whose information is to be queried
   * @return the data structure pointer associated with the process
   */
  void *getMap (UserDef *u);

  /**
   * @return the global info field for this pass
   */
  void *getGlobalInfo () { return _global_info; }

  /**
   * @return the Act pointer that this pass is attached to
   */
  Act *getAct () { return a; }

  /**
   * @return the pass that has the specified name
   */
  ActPass *getPass (const char *name) { return a->pass_find (name); }

  /**
   * @return the design root that this pass was invoked with.
   */
  Process *getRoot() { return _root; }


  /**
   * This is the core recursive call that is used to traverse the
   * design hierarchy. It is invoked with the root of the design
   * hierarchy, and operates recursively visiting each type once.
   * Note that this does not invoke any dependencies of this pass;
   * that functionality is provided by ActPass::run() only.
   *
   * The mode parameter is passed along so that different actions can
   * be implemented in a single pass, selected by the mode. When
   * ActPass::run() is called, it eventually calls this function with
   * mode set to 0. Negative modes are used internally by the library,
   * so they should not be used.
   *
   * @param p is the process for the recursive traversal
   * @param mode is a parameter passed to the leaf call.
   */
  virtual void run_recursive (Process *p = NULL, int mode = 0);

  /**
   * Disables update propagation (see ActPass::update()).
   */
  void disableUpdate () { _update_propagate = 0; }

  /**
   * Enables update propagation (see ActPass::update()).
   */
  void enableUpdate () { _update_propagate = 1; }

  /**
   * Set the sticky visited flag (see ActPass::_sticky_visited)
   */
  void mkStickyVisited () { _sticky_visited = 1; }

  /**
   * Clear the sticky visited flag (see ActPass::_sticky_visited)
   */
  void clrStickyVisited() { _sticky_visited = 0; }

  /**
   * This forces a complete recomputation of all passes that have been
   * run so far.
   *
   * @param a is the Act design pointer
   * @param p is the process from which the recomputation is triggered.
   */
  static void refreshAll (Act *a, Process *p = NULL);

private:
  /**
   * This function is called exactly once per process that is
   * reachable from the root of the pass. It is called before any
   * types instantiated within the process are visited.
   *
   * @param p is the process type
   * @param mode is the mode flag passed to this call.
   * @return the data structure that is to be associated with this
   * process
   */
  virtual void *pre_op (Process *p, int mode = 0);

  /**
   * Same as pre_op() for processes, but for channels.
   *
   * @param c is the channel type
   * @param mode is the mode flag passed to this call.
   * @return the data structure that is to be associated with this
   * channel
   */
  virtual void *pre_op (Channel *c, int mode = 0);

  /**
   * Same as pre_op() for processes, but for data types.
   *
   * @param d is the data type
   * @param mode is the mode flag passed to this call.
   * @return the data structure that is to be associated with this
   * data type
   */
  virtual void *pre_op (Data *d, int mode = 0);

  /**
   * This function is called exactly once per process that is
   * reachable from the root of the pass. It is called after all the
   * types instantiated within the process are visited.
   *
   * @param p is the process type
   * @param mode is the mode flag passed to this call.
   * @return the data structure that is to be associated with this
   * process
   */
  virtual void *local_op (Process *p, int mode = 0);

  
  /**
   * Same as local_op() for processes, except for channels.
   *
   * @param c is the channel type
   * @param mode is the mode flag passed to this call.
   * @return the data structure that is to be associated with this
   * channel
   */
  virtual void *local_op (Channel *c, int mode = 0);
  
  /**
   * Same as local_op() for processes, except for data types
   *
   * @param d is the data type
   * @param mode is the mode flag passed to this call.
   * @return the data structure that is to be associated with this
   * data type
   */
  virtual void *local_op (Data *d, int mode = 0);

  /**
   * Free the data structure allocated by calls to pre_op()/local_op().
   */
  virtual void free_local (void *);

  int init (); ///< initialize or re-initialize

  void recursive_op (UserDef *p, int mode = 0); ///< does the actual
						///work for run_recursive()

  void init_map ();		///< initialize/re-initialize the map from
				///user-defined types to the
				///associated data structure
  
  void free_map ();		///< free memory associated with the
				///user-defined type to data map
  
  std::map<UserDef *, void *> *pmap; ///< the map from user-defined
				     ///types to the data structure
				     ///returned by
				     ///local_op()/pre_op().
  
  std::unordered_set<UserDef *> *visited_flag; ///< used to keep track
					       ///of what types have
					       ///been visited so far.
};

class Technology;
class ActDynamicPass;

/**
 *  @class ActDynamicPass
 *
 *  @brief This is an ActPass that is dynamically loaded in at
 *  run-time via a shared object library. A dynamic pass can be used
 *  to extend the Act framework at run-time without having to
 *  re-compile/statically link in new functionality. Apart from
 *  loading in new passes in at runtime via C++, the interact
 *  command-line interface to the Act system also supports loading in
 *  new passes at runtime.
 *
 *  An example of how to write a dynamic pass and load it into
 *  interact and run it is provided in the skeleton github repository
 *  available at https://github.com/asyncvlsi/actpass
 */
class ActDynamicPass : public ActPass {

  /**
   * @struct act_sh_passlib_info
   * @brief This holds information about the shared object file
   */
  struct act_sh_passlib_info {
    char *lib; ///< this is the file name of the shared object library
    void *lib_ptr; ///< this is the open library pointer, if any
    int refs;  ///< this is the number of active references to the open
	       ///library pointer.
  };

  /**
   * @struct act_sh_dispatch_table
   * @brief These are the C function pointers extracted from the
   * shared library used to execute the various methods in the
   * ActDynamicPass
   */
  struct act_sh_dispatch_table {

    /**
     * Called to initialize the pass from the constructor of the
     * dynamic pass. Dependencies could be inserted here, for
     * example.
     * @param ap is the ActPass corresponding to the ActDynamicPass
     */
    void (*_init) (ActPass *ap);


    /** 
     * Called after the pass has been run via ActPass::run(), in case
     * any additional work should be done.
     * 
     * @param ap is the dynamic pass
     * @param p is the process type passed to the ActPass::run() method.
     */
    void (*_run) (ActPass *ap, Process *p);
    
    /**
     * Called after ActPass::run_recursive() is finished.
     * @param ap is the dynamic pass
     * @param p is the process type passed to corresponding ActPass method
     * @param mode is the mode for the corresponding ActPass method
     */
    void (*_recursive) (ActPass *ap, Process *p, int mode);

    /**
     * This is the ActPass::local_op() for a process
     * @param ap is the ActPass
     * @param p is the process for local_op()
     * @param mode is the mode for local_op()
     * @return the data structure generated by local_op()
     */
    void *(*_proc) (ActPass *ap, Process *p, int mode);
    
    /**
     * This is the ActPass::local_op() for a channel
     * @param ap is the ActPass
     * @param c is the channel for local_op()
     * @param mode is the mode for local_op()
     * @return the data structure generated by local_op()
     */
    void *(*_chan) (ActPass *ap, Channel *c, int mode);
    
    /**
     * This is the ActPass::local_op() for a data type
     * @param ap is the ActPass
     * @param d is the data type for local_op()
     * @param mode is the mode for local_op()
     * @return the data structure generated by local_op()
     */
    void *(*_data) (ActPass *ap, Data *d, int mode);

    /**
     * This is called to free an allocated data structure
     * @param ap is the ActPass
     * @param v is the data structure to be free'd
     */
    void (*_free) (ActPass *ap, void *v);

    /*
     * This called prior to the pass being destroyed
     * @param ap is the pass
     */
    void (*_done) (ActPass *ap);

    /*
     * A pass-specific function that can be defined to run arbitrary
     * commands associated with a pass.
     * @param ap is the pass
     * @param name is the name of the command to run
     * @return return value of the command (-1 is reserved to mean the
     * command does not exist).
     */
    int (*_runcmd) (ActPass *ap, const char *name);
  };

  
public:
  /**
   * Load a dynamic pass from a shared object file.
   *
   * @param _a is the Act design where the pass should be added
   * @param name is the name of the ActPass
   * @param lib is the shared object library name
   * @param prefix is the string prefix for the functions in the
   * shared object library associated with the pass. Function names
   * will be prefix_<name>
   */
  ActDynamicPass (Act *_a, const char *name, const char *lib, const char *prefix);
  
  ~ActDynamicPass ();		///< release storage

  /**
   * Implementation of ActPass::run() for a dynamic pass
   */
  int run (Process *p = NULL);

  /**
   * Implementation of ActPass::run_recursive() for a dynamic pass 
   */
  void run_recursive (Process *p = NULL, int mode = 0);

  /**
   * To pass extra parameters to/from a dynamic pass, a table of
   * parameters is stored with the dynamic pass. This function is used
   * to set a pointer parameter
   *
   * @param name the name of the parameter
   * @param v is the pointer
   */
  void setParam (const char *name, void *v);
  
  /**
   * To pass extra parameters to/from a dynamic pass, a table of
   * parameters is stored with the dynamic pass. This function is used
   * to set an integer parameter
   *
   * @param name the name of the parameter
   * @param v is the integer for the parameter
   */
  void setParam (const char *name, int v);

  /**
   * To pass extra parameters to/from a dynamic pass, a table of
   * parameters is stored with the dynamic pass. This function is used
   * to set a real valued parameter
   *
   * @param name the name of the parameter
   * @param v is the real number for the parameter
   */
  void setParam (const char *name, double v);

  /**
   * @param name is the name of the parameter to be queried
   * @return the integer value associated with the parameter, -1 on a
   * failure
   */
  int getIntParam (const char *name);
  
  /**
   * @param name is the name of the parameter to be queried
   * @return the pointer associated with the parameter, NULL on a
   * failure
   */
  void *getPtrParam (const char *name);
  
  /**
   * @param name is the name of the parameter to be queried
   * @return the real number associated with the parameter, -1 on a
   * failure
   */
  double getRealParam (const char *name);

  /**
   * @param name is the name of the pameter to be queried
   * @return true if the parameter is defined, false otherwise.
   */
  bool hasParam (const char *name);

  /**
   * This is used to run the specified command. Parameters to the
   * command are typically passed using the setParam() methods.
   * @return returns the result from the runcmd function, -1 if the
   * function does not exist.
   */
  int runcmd (const char *name);

  /**
   * For internal use only. Used to get the hash table that holds all
   * the config parameters.
   * @return the configuration parameter hash table.
   */
  struct Hashtable *getConfig ();

  /**
   * For internal use only. Used to get the Technology class
   * @return the technology class
   */
  Technology *getTech () { return T; }

  /**
   * @return true if the pass was successfully loaded, false otherwise
   */
  bool loaded() { return _load_success; }

  /**
   * This provides access to the protected ActPass::AddDependency()
   * method since this call may be required from the dynamically
   * loaded functions
   *
   * @param name is the name of the pass for dependency tracking.
   * @return pass-through for ActPass::AddDependency()
   */
  int addDependency (const char *name) { return AddDependency (name); }
  
private:

  /**
   * Implementation of local_op() for dynamic pass.
   */
  virtual void *local_op (Process *, int = 0);
  /**
   * Implementation of local_op() for dynamic pass.
   */
  virtual void *local_op (Channel *, int  = 0);
  /**
   * Implementation of local_op() for dynamic pass.
   */
  virtual void *local_op (Data *, int  = 0);
  /**
   * Implementation of free_local() for dynamic pass.
   */
  virtual void free_local (void *);

  bool _load_success; ///< flag set when the pass is loaded
  
  char *_libused;  ///< library used by this pass
  
  act_sh_dispatch_table _d;  ///< dispatch table for this pass
  
  struct Hashtable *_params;  ///< hash table holding parameters for
			      ///the pass
  
  struct Hashtable *_config_state; ///< hash table holding the 
				   ///config parameter state
  
  Technology *T;		///< the technology pointer for the library
  
  /**
   * A list of shared object libraries stored as a list of 
   * act_sh_passlib_info pointers. THis is shared across all dynamic
   * passes.
   */
  static list_t *_sh_libs;
};


/* this should be elsewhere */

/**
 * Create an Expr pointer corresponding to a constant integer
 * expression. Constant expressions are cached globally, and should
 * never be explicitly free'd.
 *
 * @param val is the constant value for the expression
 * @return an Expr corresponding to the constant value
 */
Expr *const_expr (long val);

/**
 * Create an Expr pointer corresponding to a constant Boolean value
 * (true or false). Constant expressions are cached globally, and should
 * never be explicitly free'd.
 *
 * @param v is the constant value for the expression (1 for true, 0
 * for false)
 * @return an Expr corresponding to the constant Boolean value
 */
Expr *const_expr_bool (int v);

/**
 * Create an Expr pointer corresponding to a constant real number.
 * Constant expressions are cached globally, and should
 * never be explicitly free'd.
 *
 * @param v is the constant value for the expression.
 * @return an Expr corresponding to the constant real number.
 */
Expr *const_expr_real (double v);


/**
 * For internal use only. This function is used to add a constant
 * parameter definition before the Act library is used to read in a
 * file.  It should be called after Act::Init() to add global
 * parameter definitions.  IT MUST BE CALLED BEFORE an Act object is
 * created!
 *
 * @param name is the name of the pint parameter
 * @param val is the value of the parameter
 */
void act_add_global_pint (const char *name, int val);


/**
 * For internal use only. This function is used to add a constant
 * parameter definition before the Act library is used to read in a
 * file.  It should be called after Act::Init() to add global
 * parameter definitions.  IT MUST BE CALLED BEFORE an Act object is
 * created!
 *
 * @param name is the name of the pbool parameter
 * @param val is the value of the parameter
 */
void act_add_global_pbool (const char *name, int val);

#endif /* __ACT_H__ */
