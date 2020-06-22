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
#include "simdes.h"

/* globals for sim object */
int SimDES::initialized_sim = 0;

/* globals for events */
Heap *SimDES::all = NULL;
unsigned long SimDES::tm_offset[SIM_TIME_SIZE];
unsigned long SimDES::curtime = 0;
Event *Event::ev_queue = NULL;
SimDES *SimDES::curobj = NULL;

/* create and destroy */
SimDES::SimDES ()
{
  break_point = 0;

  if (!all) {
    /* first time I'm here */
    all = heap_new (32);
  }

  if (!initialized_sim) {
    initialized_sim = 1;
    for (int i=0; i < SIM_TIME_SIZE; i++) {
      SimDES::tm_offset[i] = 0;
    }
  }
}

/*
  Nothing to do here...
*/
SimDES::~SimDES ()
{ }

/*
 * Create a new event, insert into the queue
 */
Event::Event (SimDES *s, int event_type, int delay)
{
  /* check to see if the delay would cause "curtime" to roll over */
  while (((unsigned long)~0UL - SimDES::curtime) < (unsigned)delay) {
    /* let's walk through the heap to see if I can change the current
       time reference */
    int i;
    unsigned long tm;

    tm = heap_peek_minkey (SimDES::all);
    /* the earliest time of all pending events is now tm */

    if (tm == 0) {
      fatal_error ("The dynamic range of time in the pending event list is too large to represent.\n");
    }

    /* adjust curtime by tm, and add tm in to the tm_offset[] array */
    SimDES::curtime = SimDES::curtime - tm;

    if (SimDES::tm_offset[0] + tm < SimDES::tm_offset[0]) {
      /* time rolled over at 0, do the carries */
      for (i=1; i < SIM_TIME_SIZE; i++) {
	SimDES::tm_offset[i]++;
	if (SimDES::tm_offset[i] != 0) {
	  break;
	}
      }
    }
    SimDES::tm_offset[0] = SimDES::tm_offset[0] + tm;
      
    for (i=0; i < SimDES::all->sz; i++) {
      SimDES::all->key[i] -= tm;
    }
  }

  obj = s;
  ev_type = event_type;
  kill = 0;
  heap_insert (SimDES::all, SimDES::curtime + delay, this);
}

Event::~Event () { } 

/*
 * Return the low order bits of the current simulation time
 */
unsigned long long SimDES::CurTimeLo() 
{
  return tm_offset[0] + curtime;
}

#define IS_A_BREAKPOINT(ev)						\
  ((ev)->obj->break_point && ((ev)->obj->break_point == 1 ||		\
			      ((ev)->ev_type == (ev)->obj->bp_ev_type)))
/*
 * static method, run the entire simulation
 *
 *  Returns NULL if no more events, otherwise returns the event that
 *  caused the simulation to stop due to a break point.
 */
Event *SimDES::Run ()
{
  Event *ev;
  unsigned long tm;
  unsigned long long tm2;
  
  /* process all events in global time order */
  while ((ev = (Event *)heap_remove_min_key (all, &tm2))) {
    tm = tm2;
    /* current time needs to advance */
    curtime = tm;

    /* execute event */
    if (!ev->kill) {
      if (IS_A_BREAKPOINT (ev)) {
	/* put the event back */
	heap_insert (all, tm, ev);
	break;
      }
      curobj = ev->obj;
      ev->obj->Step (ev->ev_type);
    }
    delete ev;
  }
  return ev;
}

/*
 * static method, run the next n events in the simulation.
 * returns NULL on success, otherwise the event that caused the
 * simulation to stop (breakpoint)
 */
Event *SimDES::Advance (int n)
{
  Event *ev;
  unsigned long tm;
  unsigned long long tm2;

  /* process all events in global time order */
  while (n && (ev = (Event *)heap_remove_min_key (all, &tm2))) {
    tm = tm2;
    curtime = tm;
    /* current time needs to advance */
    if (!ev->kill) {
      if (IS_A_BREAKPOINT(ev)) {
	/* put the event back */
	heap_insert (all, tm, ev);
	break;
      }
      curobj = ev->obj;
      ev->obj->Step (ev->ev_type);
    }
    delete ev;
    n--;
  }
  if (n == 0) {
    return NULL;
  }
  else {
    return ev;
  }
}

/*
 * static method, run simulation until "delay" time elapses.
 * Returns NULL on success, otherwise returns the event that caused
 * the simulation to stop (breakpoint)
 */
Event *SimDES::AdvanceTime (int delay)
{
  Event *ev;
  unsigned long tm;

  /* process all events in global time order */
  do {
    if (heap_peek_min (all) == NULL) {
      /* nothing in the heap */
      return NULL;
    }
    tm = heap_peek_minkey (all);
    
    if (delay < (tm - curtime)) {
      /* I'm out of time, return */
      return NULL;
    }
    else {
      delay = delay - (tm - curtime);
    }

    ev = (Event *) heap_remove_min (all);

    /* current time needs to advance */
    curtime = tm;
    if (!ev->kill) {
      if (IS_A_BREAKPOINT(ev)) {
	heap_insert (all, tm, ev);
	break;
      }
      curobj = ev->obj;
      ev->obj->Step (ev->ev_type);
    }
    delete ev;
  } while (1);
  return ev;
}

void SimDES::Pause (int delay)
{
  /* create an event */
  new Event (this, SIM_EV_DELAY, delay);
}


/*
 * Conditions
 */
Condition::Condition () 
{
  /* initialize waiting object list */
  waiting_objects = list_new ();
}

Condition::~Condition () 
{
  list_free (waiting_objects);
}
    
/*
 * Add an object to the list of waiting objects for this condition
 */
void Condition::AddObject (SimDES *s)
{
  list_append (waiting_objects, s);
}

/* we're done, notify all objects */
void Condition::Wakeup (int delay)
{
  SimDES *s;

  while (!list_isempty (waiting_objects)) {
    s = (SimDES *)list_delete_tail (waiting_objects);
    new Event (s, SIM_EV_WAKEUP, delay);
  }
}

int Condition::isWaiting ()
{
  if (waiting_objects && !list_isempty (waiting_objects)) return 0;
  return 1;
}

/*
 * Condition corresponding to waiting for all of "slots"
 * sub-conditions to be true.
 */
WaitForAll::WaitForAll (int slots, int _delay)
{
  slot_state = bitset_new (slots); /* bitvector to keep track of
				      which slot is still not true */
  bitset_clear (slot_state);
  nslots = slots;
  num = slots;			/* counter to speed up empty test */
  delay = _delay;
}

void WaitForAll::ReInit ()
{
  bitset_clear (slot_state);
  num = nslots;
  Condition::ReInit ();
}


WaitForAll::~WaitForAll ()
{
  bitset_free (slot_state);
  slot_state = NULL;
}

/*
 * Slot "n" is now ready. Take the appropriate action.
 *
 * Returns 1 if the wait is satisfied, which says that the storage for
 * the wait can now be released.
 */
int WaitForAll::Notify (int n)
{
  if (!bitset_tst (slot_state, n)) {
    bitset_set (slot_state, n);
    num--;
    if (num == 0) {
      /* let all the waiting objects know we're ready to go */
      Wakeup (delay);
      return 1;
    }
  }
  return 0;
}

/*
 * Event queue management to not use malloc unless absolutely necessary.
 */
void *Event::operator new (size_t sz)
{
  void *x;
  Assert (sz == sizeof (Event), "What?");

  if (!Event::ev_queue) {
    Event::ev_queue = (Event *)malloc (sz);
    if (!Event::ev_queue) {
      fatal_error ("Out of memory!");
    }
    Event::ev_queue->next = NULL;
  }
  x = Event::ev_queue;
  Event::ev_queue = Event::ev_queue->next;
  return x;
}

void Event::operator delete (void *v)
{
  Event *e = (Event *)v;
  e->next = Event::ev_queue;
  Event::ev_queue = e;
}
