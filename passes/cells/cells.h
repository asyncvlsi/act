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
#ifndef __ACT_PASS_CELL_H__
#define __ACT_PASS_CELL_H__

#include <map>
#include <act/act.h>
#include <act/iter.h>
#include <act/passes.h>
#include "hash.h"
#include "bitset.h"
#include "array.h"


struct act_varinfo {
  int nup, ndn;			// # of times in up and down guards
  int *depths;			// depth of each instance (nup + ndn)
  int cup, cdn;
				// size), 0..nup-1 followed by ndn
  unsigned char tree;		// 0 = not in tree, 1 in tree
};

struct act_prsinfo {
  Process *cell;		/* the cell; NULL until it is
				   created. */

  int nvars;			/* # of variables. includes @-labels */

  int nout;			/* # of outputs 
				   Variable convention:
				   0, 1, ..., nout-1 are outputs.
				   nout, ... nvars-1 are inputs
				*/
  int nat;                     /* # of labels */

  /* variable attributes */
  int *attr_map;
  A_DECL (struct act_varinfo, attrib);

  int tval;			 /* for tree<>; -1 = none, 0 = mgn,
				    otherwise tree  */

  /* XXX: need attributes from the production rules */
  
  A_DECL (act_prs_expr_t *, up); /* pull-up */
  A_DECL (act_prs_expr_t *, dn); /* pull-down */
      /* NOTE: all actid pointers are actually just simple integers */
      /* The # of these will be nout + any internal labels */

  int *match_perm;		// used to report match!
  
};


#endif /* __ACT_PASS_CELL_H__ */
