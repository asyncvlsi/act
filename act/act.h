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
  Act (const char *s);
  ~Act ();


  /**
   * Expand types!
   */
  void Expand ();

  ActNamespace *Global() { return gns; }

 private:
  TypeFactory *tf;		/* type factory for the file */
  ActNamespace *gns;		/* global namespace */
};

/* install string mangling */
void act_mangle (char *s);

/* mangle/unmangle string from src to dst, sz = size of dst 
   Returns -1 on error, 0 on success 
*/
int act_mangle_string (char *src, char *dst, int sz);
int act_unmangle_string (char *src, char *dst, int sz);

/* mangle fprintf/snprintf */
void act_fprintf (FILE *fp, char *s, ...);
int act_snprintf (char *fp, int len, char *s, ...);

/* unmangle fprintf/snprintf */
void act_ufprintf (FILE *fp, char *s, ...);
int act_usnprintf (char *fp, int len, char *s, ...);

#endif /* __ACT_H__ */
