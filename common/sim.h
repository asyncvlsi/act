/*************************************************************************
 *
 *  Simulation objects
 *
 *  Copyright (c) 1999, 2019 Rajit Manohar
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
#ifndef __ACTSIM_H__
#define __ACTSIM_H__

/*
 *  Sim:
 *
 *   All tasks in the simulation should be created by inheriting from
 *   the Sim class. Each Sim is tagged with a unique identifier
 *   (except for checkpoint and uncheckpoint, that are tagged with -1).
 *
 *   To create a task of type T, do the following:
 *
 *     class T : public SimTYPE { ... };
 *
 *   class T needs to override the following methods, since they are
 *   pure virtual functions:
 *
 *        
 *   IF YOU DON'T CARE ABOUT: printing, checkpointing, you can implement
 *   the following functions with `{ }' :)
 *    
 *        void Print (FILE *fp)
 *           Print information about the state of the object in
 *           human-readable format to fp.
 *
 *
 *        void DumpStats (FILE *fp)
 *           Dump statistics for a simulation object
 *
 */
#include <stdio.h>
#include <common/misc.h>
#include <common/mytime.h>
#include <common/hconfig.h>
#include <common/machine.h>

class Sim {
 public:
  Sim (int ready = 1);
  virtual ~Sim ();

  virtual void Print (FILE *fp) = 0; /* print to file */
  virtual void DumpStats (void) = 0; /* statistics */
  virtual const char *Name (void) = 0; /* name */
  
  static int CheckDeadlock (void); // check for deadlock
  static void DeadlockedDumpStats (void); // dumpstats for deadlocked processes

  virtual Time_t CurTime() = 0;

  static int Number;

  void UndoNumbering () { Sim::Number--; my_id = -1; }

  static Time_t Now();


  int myid () { return my_id; } /* your unique global id */

  friend class CheckPoint;
  friend class UnCheckPoint;
  friend class SimDES;

protected:
  int _ready;			// initially on the ready queue?
  
private:
  int my_id;
  static Sim *obj_list;		/* global list of Sim object things */
  Sim *next, *prev;		/* sim object links */

};


#endif /* __ACTSIM_H__ */
