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

/**
 * @file act.h
 *       Contains top-level initialization/management functions for
 *       the ACT library
 */

class ActPass;

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
  static void Init (int *argc, char ***argv);


  /**
   * Maximum depth of recursion for expanding types
   */
  static int max_recurse_depth;

  /**
   * Maximum number of iterations for general loops
   */
  static int max_loop_iterations;

  /**
   * Warn on empty selection in main ACT language
   */
  static int warn_emptyselect;

  /**
   * Parser flags 
   */
  static int emit_depend;


  /**
   * Create an act data structure for the specified input file
   *
   * @param s is the name of the file containing the top-level ACT
   */
  Act (const char *s = NULL);
  ~Act ();

  /** 
   * Merge in ACT file "s" into current ACT database
   */
  void Merge (const char *s);


  /**
   * Expand types!
   */
  void Expand ();

  
  void mangle (char *s);	// install string mangling functions
  int mangle_active() { return any_mangling; }
  int mangle_string (const char *src, char *dst, int sz);
  int unmangle_string (const char *src, char *dst, int sz);
  void mfprintf (FILE *fp, const char *s, ...);
  void ufprintf (FILE *fp, const char *s, ...);
  int msnprintf (char *fp, int sz, const char *s, ...);
  int usnprintf (char *fp, int sz, const char *s, ...);
  void msnprintfproc (char *fp, int sz, UserDef *, int omit_ns = 0);
  void mfprintfproc (FILE *fp, UserDef *, int omit_ns = 0);


  /* 
     API functions
  */
  Process *findProcess (const char *s);
  Process *findProcess (ActNamespace *, const char *);
  UserDef *findUserdef (const char *s);

  ActNamespace *findNamespace (const char *s);
  ActNamespace *findNamespace (ActNamespace *, const char *);
  ActNamespace *Global() { return gns; }

  /*
    Dump to a file 
  */
  void Print (FILE *fp);

  void *aux_find (const char *phase);
  void aux_add (const char *phase, void *data);

  void pass_register (const char *name, ActPass *p);
  ActPass *pass_find (const char *name);

private:
  TypeFactory *tf;		/* type factory for the file */
  ActNamespace *gns;		/* global namespace */

  char mangle_characters[256];
  int inv_map[256];
  int any_mangling;
  int mangle_langle_idx;  /* index of '<' */
  int mangle_min_idx;     /* index of the min of , . { } */
  int mangle_mode;

  struct Hashtable *aux;	// any aux storage you want
  struct Hashtable *passes;	// any ActPass-es
};

class ActPass {
  int _finished;		// has the pass finished execution?
  Act *a;			// main act data structure
  list_t *deps;			// ActPass dependencies
  const char *name;
  
public:
  ActPass (Act *_a, const char *name); // create, initialize, and
				       // register pass
  
  ~ActPass ();			       // release storage


  int AddDependency (const char *pass); // insert dependency on an
					// actpass

  int rundeps (Process *p = NULL);

  const char *getName();
  
  virtual int run(Process *p = NULL); // run pass on a process; NULL =
				      // top level
  
  virtual int init (); // initialize or re-initialize
  
  int completed()  { return (_finished == 2) ? 1 : 0; }
  int pending()  { return (_finished == 1) ? 1 : 0; }
};

Expr *const_expr (int);

#endif /* __ACT_H__ */
