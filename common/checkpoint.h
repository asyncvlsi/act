/*************************************************************************
 *
 *  Checkpointing code/support for simulation library.
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
#ifndef __CHECKPOINT_H__
#define __CHECKPOINT_H__

#include "sim.h"
#include "hash.h"

/*
 *  Create a checkpoint at a particular time by creating
 *  a checkpoint task for the appropriate time.
 *
 *  NOTE: this only works when checkpointing *static* task/process
 *  structures. Otherwise we'd have to be able to dynamically create
 *  processes when restoring from a checkpoint.
 *
 *  Luckily, since hardware processes never terminate, this is fine for
 *  our purposes.
 *
 */
class CheckPoint : public SimObject {
#if defined(SYNCHRONOUS)
  count_t chkpoint_time;
#else
#error Checkpointing only works right now for sync
#endif
  FILE *fp;
 public:
  enum ChkType { SaveCtxt, NoSaveCtxt };
  // There are two types of checkpoints: one where the context is
  // saved (stack, pc, the whole damn thing). This one can be made
  // anywhere, and the program should be able to come up
  // properly. However, if you do this, then one can't restart from a
  // checkpointed state if the code changes. This is because the pc
  // and context structures are saved in the checkpoint.
  //
  // The second option is to not save the context. This only works for 
  // specialized types of tasks. Each task must be of the form:
  //
  //   while (1) { 
  //    await(foo,count);
  //    ...
  //   }
  //
  // Whenever UnCheckPoint is called, it re-initializes all eventcount 
  // structures and stuff like that, but it *DOES NOT CHANGE THE PC*
  // for any task. So, the PC needs to be right. The only way to
  // guarantee this is to make sure that all suspensions happen at a
  // fixed await().
  // 
  // This is not even half as useful as the other type of checkpoint,
  // but you can do cool things like save a checkpoint on one
  // architecture and then re-start on another one. :)

  CheckPoint (FILE *f, count_t tm, int ready, ChkType type = SaveCtxt );
  // save checkpoint to f, tm = time at which to checkpoint

  void Print (FILE *fp) { fp = NULL; } /* never called */
  void SaveState (FILE *fp) { fp = NULL; } /* no need to save state */
  void RestoreState (FILE *fp, UnCheckPoint *uc) { fp = NULL; uc = NULL; } 
                                /* no need to restore state */

  void MainLoop (void);		/* checkpoint loop */
  void DumpStats (void) { }

  ~CheckPoint () { };
private:
  ChkType chktype;
};


/*
 *  Restore from checkpoint.
 */
class UnCheckPoint : public SimObject {
  FILE *fp;
  struct iHashtable *H;
 public:
  UnCheckPoint (FILE *f, int ready);	// restore from f
  ~UnCheckPoint ();

  void Print (FILE *fp) { fp = NULL; }	/* never called */
  void SaveState (FILE *fp) { fp = NULL; } /* no need to save state */
  void RestoreState (FILE *fp, UnCheckPoint *uc) { fp = NULL; uc = NULL; } 
                                /* no need to restore state */
  void DumpStats (void) { }
  
  void MainLoop (void);		/* checkpoint loop */

  SimObject *ObjectPointer (long id); // translate taskid to pointer
};


#if defined(SYNCHRONOUS)
void SaveQueueState (FILE *fp, eventcount *ec);
				/* save queue state to file */

void RestoreQueueState (FILE *fp, eventcount *ec, UnCheckPoint *uc);
				/* restore state from file */
#elif defined (ASYNCHRONOUS)
void SaveChannelState (FILE *fp, ch_t *c);
				/* save queue state to file */

void RestoreChannelState (FILE *fp, ch_t *c, UnCheckPoint *uc);
				/* restore state from file */
#else

#error ick

#endif

#endif /* __CHECKPOINT_H__ */
