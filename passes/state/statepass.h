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
#include <act/passes.h>
#include "hash.h"
#include "bitset.h"
#include "array.h"


typedef struct {
  int nbools;			// # of Boolean bits
  int ibitwidth;		// cumulative integer bitwidth
  int ichanwidth;		// cumulative integer channel width

  int localbools;		// total number of Booleans needed for
				// local state: bits + elaborated int
				// + elaborated channel state

  int allbools;			// localbools + bools needed for instances

  struct iHashtable *map;	// map from connection pointer to
				// unique integer from 0 .. totbools-1
  
  struct iHashtable *imap;	// map from ValueIdx pointer to bool offset
  
} stateinfo_t;

class ActStatePass : public ActPass {
public:
  ActStatePass (Act *a);
  ~ActStatePass ();

  int run (Process *p = NULL);

  void Print (FILE *fp);

private:
  int init ();

  /* -- info map -- */
  std::map<Process *, stateinfo_t *> *umap;
  ActBooleanizePass *bp;

  void count_sub (Process *p);
  void count_local (Process *p, Scope *s);
};


#endif /* __ACT_PASS_STATE_H__ */
