/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __ACT_H__
#define __ACT_H__

#include <act/types.h>
#include <act/lang.h>

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
   * Create an act data structure for the specified input file
   *
   * @param s is the name of the file containing the top-level ACT
   */
  Act (const char *s = NULL);
  ~Act ();


  /**
   * Expand types!
   */
  void Expand ();

  
  void mangle (char *s);	// install string mangling functions
  int mangle_active() { return any_mangling; }
  int mangle_string (char *src, char *dst, int sz);
  int unmangle_string (char *src, char *dst, int sz);
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
