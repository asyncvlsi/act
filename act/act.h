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


  /* 
     API functions
  */
  Process *findProcess (const char *s);
  Process *findProcess (ActNamespace *, const char *);
  ActNamespace *findNamespace (const char *s);
  ActNamespace *findNamespace (ActNamespace *, const char *);
  ActNamespace *Global() { return gns; }

  /*
    Dump to a file 
  */
  void Print (FILE *fp);

  void *aux_find (const char *phase);
  void aux_add (const char *phase, void *data);

private:
  TypeFactory *tf;		/* type factory for the file */
  ActNamespace *gns;		/* global namespace */

  char mangle_characters[256];
  int inv_map[256];
  int any_mangling;

  struct Hashtable *aux;	// any aux storage you want
  
};

#endif /* __ACT_H__ */
