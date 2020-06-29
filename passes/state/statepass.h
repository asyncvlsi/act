/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Manohar
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
#ifndef __ACT_PASS_STATE_H__
#define __ACT_PASS_STATE_H__

#include <map>
#include <act/act.h>
#include <act/iter.h>
#include <act/passes/booleanize.h>
#include "hash.h"
#include "array.h"


/*
 * State information: assumed that ports are allocated in the parent
 * process, so no booleans associated with ports are included in the
 * local/all bools count.
 */

typedef struct {
  int nportbools;		// # of port bools [not omitted]

  int localbools;		// total number of Booleans needed for
				// local state: bits + elaborated int
				// + elaborated channel state

  /*-- 
    Boolean variables are numbered
      0 ... nportbools-1 ... (nportbools + localbools - 1)
  --*/
  
  bitset_t *multi;		// bitset of local bools that are
				// locally multi-driver. The size of
				// this bitset is # of localbools + #
				// of port bools. 0..nportbools-1 are
				// the ports, and the rest are
				// numbered as per localbool numbering
				// with nportbools as the offset.

  int ismulti;			// 1 if this process contributes to a
				// multi-driver scenario

  int allbools;			// total booleans needed for this
				// process (minus its ports)

  struct iHashtable *map;	// map from connection pointer to
				// unique integer from 0 .. totbools-1
                                // chp connection pointers also map to
				// unique id
  
  
  struct iHashtable *imap;	// map from ValueIdx pointer to bool
				// offset
  /*--
    Analogous quantities for CHP level of abstraction
    --*/
  
  int nportchp;			// # of chp variables in the port [not
				// omitted]

  int localchp;			// local chp variables

  bitset_t *chpmulti;		// 1 if multi-driver at the CHP level

  act_booleanized_var_t **chpv;	// map to var info from variable ID
				// for chp: size if nportchp +
				// localchp

  int chp_ismulti;		// multidriver through CHP

  struct iHashtable *chpmap;	// connection * to unique integer
				// (bool/int/chan)

} stateinfo_t;

class ActStatePass : public ActPass {
public:
  ActStatePass (Act *a);
  ~ActStatePass ();

  int run (Process *p = NULL);

  void Print (FILE *fp, Process *p = NULL);

  stateinfo_t *getStateInfo (Process *p);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  stateinfo_t *countLocalState (Process *p);
  void printLocal (FILE *fp, Process *p);
  int _black_box_mode;
  
  ActBooleanizePass *bp;
  FILE *_fp;
};


#endif /* __ACT_PASS_STATE_H__ */
