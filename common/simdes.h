/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#ifndef __ACTSIMDES_H__
#define __ACTSIMDES_H__

/*
 *  SimDES:
 *
 *   All tasks in the simulation should be created by inheriting from
 *   the SimDES class. Each Sim is tagged with a unique identifier
 *
 *   To create a task of type T, do the following:
 *
 *     class T : public SimDES { ... };
 *
 *   class T needs to override the following methods, since they are
 *   pure virtual functions:
 *
 *        int Init ()
 *
 *           This is what is invoked when the task begins
 *           execution. It should return the time of the first event
 *           for this object, or -1 if there isn't any.
 *
 *        void Step (int)
 *
 *           Invoked whenever the object operations are to be
 *           processed.
 *
 *
 */
#include <stdio.h>
#include "misc.h"
#include "heap.h"
#include "list.h"
#include "bitset.h"
#include "sim.h"

class SimDES;

/*
 * Events: used to make forward progress in the simulation
 *
 *  Events have
 *       - event_type: the type of the event (also called ev_type below)
 *       - delay (relative to the current time)
 *       - object: the object whose event of type "event_type" has to
 *                 be executed
 *
 *  There are three reserved event types:
 *      EV_INIT : the initial event, used if the object begins with an
 *       action at the beginning of the simulation
 *      EV_WAKEUP : event that occurs when the object gets to wake up after a
 *       wait condition
 *      EV_DELAY : event that occurs when the object wakes up after an
 *       internal delay (Pause).
 * 
 */
#define SIM_EV_INIT   0
#define SIM_EV_WAKEUP 1
#define SIM_EV_DELAY  2
#define SIM_EV_REST   3
#define SIM_EV_MAX    31

class Event {
public:
  Event (SimDES *s, int event_type, int delay);
  ~Event ();

  /*
   * Used to make the simulator drop an event without executing
   * it. Used to revoke a pending event.
   */
  void Remove () { kill = 1; }

  void *operator new (size_t sz);
  void operator delete (void *v);

private:
  unsigned int kill:1;		// set to 1 to make this an event that
				// is discarded
  unsigned int ev_type:6;	// ditto

  SimDES *obj;		// information about the event (see above)

  /* allocated event queue */
  static Event *ev_queue;
  Event *next;               // queue of events

  friend class SimDES;
};

/*
 *  Sim: used to model entities in the simulation. Details above.
 */

#define SIM_TIME_SIZE 2

class SimDES {
 public:
  SimDES ();		    // Inherit from this class. The
			    // constructor should create the initial
			    // event with type SIM_EV_INIT, if any
  
  virtual ~SimDES ();

  /*
   *  The main function for the simulation. Step() is used to make
   *  forward progress. See above. All state changes should happen in Step().
   */
  virtual void Step (int ev_type) = 0;

  void Pause (int delay); 	// pause yourself by the specified
				// delay---after executing this
				// function return immediately.

  virtual const char *Name() { return "-anon-"; } // name for object

  /* set and clear break-points on the object */
  void SetBp () { break_point = 1; }
  void SetBp (int ev_type) { bp_ev_type = ev_type; break_point = 2; }
  void ClrBp () { break_point = 0; }

  /*-- simulation management --*/

  static Event *Run();		// run the simulation

  static Event *Advance(int n = 1); // run n events
  static Event *AdvanceTime (int delay); // run all events upto
				       // specified delay in the future

  /*
    The current time is represented in the simulation by an array of
    SIM_TIME_SIZE 64-bit values.
    
    Treating the tm_offset[] array as a monolothic integer, the
    current time is actually

      (tm_offset[]) + curtime
  */
  static unsigned long CurTimeLo (); // low order bits of the current time
  static SimDES *CurObj () { return curobj; }

protected:
  unsigned int break_point:2;	// set a breakpoint on this object
  unsigned int bp_ev_type:6; // event type for breakpoint, if required

private:
  static SimDES *curobj;	// current object being stepped

  /*-- object management --*/
  static int initialized_sim;   // global check

  /*-- simulation runtime --*/

  /*
    The current time is actually (tm_offset + curtime)
    When curtime becomes too large to represent, we walk through the
    heap and modify all times in the heap, and update tm_offset.
  */

  static unsigned long tm_offset[SIM_TIME_SIZE];
  /* if heap times get large, this will get used as the
     offset into the current time
  */

  static unsigned long curtime;	// current time
  static Heap *all;		// all events

  friend class Event;
};

class Condition {
public:
  Condition ();
  virtual ~Condition ();

  /*  
      return 1 if the condition should be released
      return 0 otherwise
   */
  virtual int Notify (int slot) = 0;	// generic notifier

  void AddObject (SimDES *s);	// add me to the list of things
				// that have to be invoked when
				// the wait completes

  void ReInit () {
    Assert (list_isempty (waiting_objects), "ReInit() with waiting objects?");
  }

  int isWaiting ();		// return 1 if there are waiting
				// objects, 0 otherwise

protected:
  void Wakeup (int delay = 0);

private:
  list_t *waiting_objects;
};

class WaitForAll : public Condition {
public:
  WaitForAll (int slots, int delay = 0);
  ~WaitForAll ();

  int Notify  (int slot);	// slot is complete
  void ReInit ();		// re-initialize

private:
  int delay;
  int num;
  int nslots;
  bitset_t *slot_state;
};

#define STANDARD_SIM_TEMPLATE			\
   void Print (FILE *fp) { }			\
   int Init ();					\
   void Step (int);				\
   void DumpStats (void) { }


#endif /* __ACTSIMDES_H__ */
