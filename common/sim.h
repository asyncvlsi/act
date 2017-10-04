/*-*-mode:c++-*-**********************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
#ifndef __SIM_H__
#define __SIM_H__

/*
 *  SimCreateTask (SimObject *s, char *name, int stacksize) --
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
 *     class T : public SimObject { ... };
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

#if defined (ASYNCHRONOUS)
#include "thread.h"
#include "channel.h"
#elif defined (SYNCHRONOUS)
#include "tasking.h"
#else
#error Yikes
#endif

#include "misc.h"
#include "hconfig.h"
#include "log.h"
#include "machine.h"

class UnCheckPoint;	// forward

class SimObject {
 public:
  SimObject (int ready);
  SimObject ();
  virtual ~SimObject ();

  int myid () { return my_id; } /* your unique global id */

  virtual void Print (FILE *fp) = 0; /* print to file */

  virtual void SaveState (FILE *fp) = 0; /* save state to file */

  virtual void RestoreState (FILE *fp, UnCheckPoint *uc) = 0; 
                                     /* read state from file */

  virtual void MainLoop (void) = 0; /* main loop for you */

  virtual void DumpStats (void) = 0; /* statistics */

  static int SimCheckDeadlock (void); // check for deadlock
  static void SimDeadlockedDumpStats (void); // dumpstats for deadlocked processes

  static void Thread_Stub (void);
  static int Number;

  friend class CheckPoint;
  friend class UnCheckPoint;
  friend void SimCreateTask (SimObject *, const char *, int);
#if defined(SYNCHRONOUS)
  friend void SaveQueueState (FILE *, eventcount *);
  friend void RestoreQueueState (FILE *, eventcount *, UnCheckPoint *);
#elif defined(ASYNCHRONOUS)
  friend void SaveChannelState (FILE *, ch_t *);
  friend void RestoreChannelState (FILE *, ch_t *, UnCheckPoint *);
#endif

#if defined(SYNCHRONOUS)
  task *SimCore (void) { return t; }
#elif defined (ASYNCHRONOUS)
  lthread_t *SimCore (void) { return t; }
#endif
private:
  int my_id;
  static SimObject *obj_list;	/* global list of SimObject things */
  void UndoNumbering(void);	/* for id correspondence stuff.  DON'T
				 * CALL THIS UNLESS YOU KNOW WHAT YOU
				 * ARE DOING.  Checkpointing code
				 * depends on this */
  SimObject *next, *prev;	/* simobject links */

  int _ready;			// initially on the ready queue?

#if defined (ASYNCHRONOUS)
  lthread_t *t;
#elif defined (SYNCHRONOUS)
  task *t;
#endif
};

void SimCreateTask (SimObject *s, const char *name, int stksz = DEFAULT_STACK_SIZE);

#define STANDARD_SIM_TEMPLATE   			\
   void MainLoop(void); 				\
   void Print (FILE *fp);				\
   void SaveState (FILE *fp);				\
   void RestoreState (FILE *fp, UnCheckPoint *uc);	\
   void DumpStats (void) { }

#define STANDARD_STATS_TEMPLATE   			\
   void MainLoop(void); 				\
   void Print (FILE *fp);				\
   void SaveState (FILE *fp);				\
   void RestoreState (FILE *fp, UnCheckPoint *uc);	\
   void DumpStats (void)

#define DUMMY_SIM_TEMPLATE   				\
   void MainLoop(void); 				\
   void Print (FILE *fp) { }				\
   void SaveState (FILE *fp) { }			\
   void RestoreState (FILE *fp, UnCheckPoint *uc) { }	\
   void DumpStats (void) { }

#define DUMMY_STATS_TEMPLATE				\
   void MainLoop(void); 				\
   void DumpStats (void);				\
   void Print (FILE *fp) { }				\
   void SaveState (FILE *fp) { }			\
   void RestoreState (FILE *fp, UnCheckPoint *uc) { }

#if defined (ASYNCHRONOUS)

#define PAUSE(x) Delay(x)
#define SIM_TIME CurrentTime()

#elif defined (SYNCHRONOUS)

#define PAUSE(x) epause((x)<<1)
#define SIM_TIME  ((get_time()) >> 1)
#define AWAIT_P_PHI0   do { if (etime.count & 0x1) epause(1); } while (0)
#define AWAIT_P_PHI1   do { if (!(etime.count & 0x1)) epause(1); } while (0)

#define IN_P_PHI0      (!(etime.count & 0x1))
#define IN_P_PHI1      (etime.count & 0x1)

#endif


#endif /* __SIM_H__ */
