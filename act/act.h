/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
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

 private:
  TypeFactory *tf;		/* type factory for the file */
  ActNamespace *gns;		/* global namespace */
};

#endif /* __ACT_H__ */
