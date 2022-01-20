/*************************************************************************
 *
 *  Threaded simulation objects
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
#ifndef __ACTSIMTHREAD_H__
#define __ACTSIMTHREAD_H__

/*
 *  SimCreateTask (SimThread *s, char *name, int stacksize) --
 *
 *    Creates a new task for execution. "s" is the task, "name" is the task
 *    name, and "stacksize" is an optional parameter that specifies the stack
 *    size for the task.
 *
 *  SimObject:
 *
 *   All tasks in the simulation should be created by inheriting from
 *   the SimObject class. Each SimObject is tagged with a unique identifier
 *   (except for checkpoint and uncheckpoint, that are tagged with -1).
 *
 *   To create a task of type T, do the following:
 *
 *     class T : public SimThread { ... };
 *
 *   class T needs to override the following methods, since they are
 *   pure virtual functions:
 *
 *        
 *        void MainLoop (void)
 *
 *           The main task loop. This is what is invoked when the task
 *           begins execution.
 *
 *
 *   IF YOU DON'T CARE ABOUT: printing, checkpointing, you can implement
 *   the following functions with `{ }' :)
 *    
 *        void Print (FILE *fp)
 *
 *           Print information about the state of the object in
 *           human-readable format to fp.
 *
 *
 *        void SaveState (FILE *fp)
 *
 *           Checkpoint the state of the object to file.
 *
 *
 *         void RestoreState (FILE *fp, UnCheckPoint *uc)
 *
 *           Restore the state of all local variables from the file. The
 *           "uc" object is useful in that it knows about the mapping from
 *           task ids to simulation object pointers.
 *
 *   checkpoint.h has helper functions that allow one to checkpoint
 *   eventcount structures.
 *
 *
 *
 */
#include <stdio.h>
#include <common/sim.h>
#include <common/thread.h>
#include <common/channel.h>

#include <common/misc.h>
#include <common/hconfig.h>
#include <common/log.h>
#include <common/machine.h>

class UnCheckPoint;	// forward

class SimThread : public Sim {
 public:
  SimThread (int ready = 1);
  SimThread ();
  virtual ~SimThread ();

  virtual void MainLoop (void) = 0; /* main loop for you */

  static void Thread_Stub (void);

  /* checkpointing support */
  void SaveState (FILE *fp);
   void RestoreState (FILE *fp, UnCheckPoint *uc);

  Time_t CurTime() { return CurrentTime(); }

  friend class CheckPoint;
  friend class UnCheckPoint;
  friend void SimCreateTask (SimThread *, const char *, int);
  friend void SaveChannelState (FILE *, ch_t *);
  friend void RestoreChannelState (FILE *, ch_t *, UnCheckPoint *);

  const char *Name ();

  lthread_t *SimCore (void) { return t; }
private:
  void UndoNumbering(void);	/* for id correspondence stuff.  DON'T
				 * CALL THIS UNLESS YOU KNOW WHAT YOU
				 * ARE DOING.  Checkpointing code
				 * depends on this */

  lthread_t *t;
};

void SimCreateTask (SimThread *s, const char *name, int stksz = DEFAULT_STACK_SIZE);

#define STANDARD_SIM_TEMPLATE   			\
   void MainLoop(void); 				\
   void Print (FILE *fp);				\
   void SaveState (FILE *fp);				\
   void RestoreState (FILE *fp, UnCheckPoint *uc);	\
   void DumpStats (void) { }

#define DUMMY_SIM_TEMPLATE   				\
   void MainLoop(void); 				\
   void Print (FILE *fp) { }				\
   void SaveState (FILE *fp) { }			\
   void RestoreState (FILE *fp, UnCheckPoint *uc) { }	\
   void DumpStats (void) { }

#define PAUSE(x) Delay(x)
#define SIM_TIME CurrentTime()

#endif /* __ACTSIMTHREAD_H__ */
