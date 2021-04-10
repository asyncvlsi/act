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
#include <common/hash.h>
#include <common/array.h>


/*
 * State information: assumed that ports are allocated in the parent
 * process, so no booleans associated with ports are included in the
 * local/all bools count.
 */

class state_counts {
  int bools;			// HSE/PRS bools
  
  int xbools; // extra bools for chp, if any
  int chans;
  int ints;

public:
  void addInt (int v = 1) { ints += v; }
  int numInts () { return ints; }
  
  void addBool (int v = 1) { bools += v; }
  int numBools () { return bools; }
  
  void addChan (int v = 1) { chans += v; }
  int numChans() { return chans; }
  
  void addCHPBool (int v = 1) { xbools += v; }
  int numCHPBools () { return xbools; }
  int numAllBools() { return bools + xbools; }

  int numCHPVars() { return xbools + ints + chans; }
  int numAllVars() { return bools + xbools + ints + chans; }

  void addVar (state_counts &s, int sz = 1) {
    bools += sz*s.bools;
    xbools += sz*s.xbools;
    chans += sz*s.chans;
    ints += sz*s.ints;
  }
  
  state_counts() { bools = 0; xbools = 0; chans = 0; ints = 0; }
};

typedef struct {
  act_boolean_netlist_t *bnl;	// the basis for this calculation

  /* 
   *  Usage of state for:
   *    1. Just the used ports
   *    2. Just the local state (not including sub-processes)
   *    3. Local state plus sub-process state
   */
  state_counts ports, local, all;

  /*-- 
    Boolean variables are numbered
      0 ... nportbools-1 ... (nportbools + localbools - 1)
  --*/
  
  bitset_t *multi;		// bitset of local bools that are
				// locally multi-driver. The size of
				// this bitset is # of local.bools + #
				// of port.bools. 0..ports.bools-1 are
				// the ports, and the rest are
				// numbered as per local.bool numbering
				// with nportbools as the offset.

  int ismulti;			// 1 if this process contributes to a
				// multi-driver scenario

  struct pHashtable *map;	// map from connection pointer to
				// unique integer from 0 .. localbools-1
                                // for local booleans.
                                // For port bools, negative numbers
				// numbered from -nportbools to -1.
                                //
                                // Similar for port integers, and port
                                // channels.
                                // for chp extra booleans, the port
                                // numbers are more negative, and the
                                // local state is more positive.

  /* The port numbering is pos - total #ports.

     But using the offset methods below reverses the port
     numbering. This is so that the functions don't need to know the
     number of ports.
     
     It is this reversed numbering that is used in all the tools when
     converting negative idx to their actual location in an array.
  */

  
  int chp_ismulti;		// multidriver through CHP

  struct pHashtable *inst;	// used for instance offsets
  
} stateinfo_t;

class ActStatePass : public ActPass {
public:
  ActStatePass (Act *a, int inst_offset = 0);
  ~ActStatePass ();

  int run (Process *p = NULL);

  void Print (FILE *fp, Process *p = NULL);

  stateinfo_t *getStateInfo (Process *p);
  act_boolean_netlist_t *getBNL (Process *p) { return bp->getBNL (p); }

  /* type: 0 = bool, 1 = int, 2 = chan-in, 3 = chan-not-in */
  
  /* return 0 on error, 1 on success */
  int getTypeOffset (stateinfo_t *si, act_connection *c,
		     int *offset, int *type, int *width);

  bool connExists (stateinfo_t *si, act_connection *c);
  
  int getTypeOffset (Process *p, act_connection *c,
		     int *offset, int *type, int *width) {
    return getTypeOffset (getStateInfo (p), c, offset, type, width);
  }

  act_connection *getConnFromOffset (stateinfo_t *si, int offset, int type,
				     int *dy);
  act_connection *getConnFromOffset (Process *p, int offset, int type, int *dy) {
    return getConnFromOffset (getStateInfo (p), offset, type, dy);
  }

  state_counts getGlobals() { return _globals; }

  stateinfo_t *rootStateInfo () { return _root_si; }

  int isGlobalOffset (int off) {
    if (off >= 0) { return 0; }
    off = -off;
    if (off & 1) { return 0; }
    return 1;
  }

  int isPortOffset (int off) {
    if (off >= 0) { return 0; }
    off = -off;
    if (off & 1) { return 1; }
    return 0;
  }
    
  int portIdx (int off) {
    Assert (isPortOffset (off), "What?");
    off = -off;
    return (off + 1)/2 - 1;
  }

  int globalIdx (int off) {
    Assert (isGlobalOffset (off), "What?");
    off = -off;
    return off/2 - 1;
  }

  int instOffsets () { return _inst_offsets; }

  int globalBoolOffset (ActId *id);

private:
  void *local_op (Process *p, int mode = 0);
  void free_local (void *);

  stateinfo_t *countLocalState (Process *p);
  void printLocal (FILE *fp, Process *p);
  int _black_box_mode;
  int _inst_offsets;

  stateinfo_t *_root_si;	// top-level state info
  state_counts _globals;
  
  ActBooleanizePass *bp;
  FILE *_fp;
};





#endif /* __ACT_PASS_STATE_H__ */
