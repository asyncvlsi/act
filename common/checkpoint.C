/*************************************************************************
 *
 *  Copyright (c) 1999 Cornell University
 *  Computer Systems Laboratory
 *  Ithaca, NY 14853
 *  All Rights Reserved
 *
 *  Permission to use, copy, modify, and distribute this software
 *  and its documentation for any purpose and without fee is hereby
 *  granted, provided that the above copyright notice appear in all
 *  copies. Cornell University makes no representations
 *  about the suitability of this software for any purpose. It is
 *  provided "as is" without express or implied warranty. Export of this
 *  software outside of the United States of America may require an
 *  export license.
 *
 *  $Id: checkpoint.cc,v 1.8 2007/07/24 19:40:42 rajit Exp $
 *
 *************************************************************************/
#include "checkpoint.h"
#include "sim.h"
#include "mem.h"

#include <time.h>

/* random initialization */
#if defined(__OMIT_DEFAULT_PARAMS__)
CheckPoint::CheckPoint (FILE *f, count_t tm, int ready, ChkType type)
  : SimObject (ready)
#else
CheckPoint::CheckPoint (FILE *f, count_t tm, int ready, ChkType type)
  : SimObject (ready)
#endif
{
  fp = f;
  chkpoint_time = tm;
  // set my id to -1 so that i don't save myself in the checkpoint
  UndoNumbering (); 
  chktype = type;
}

/*
 *  CheckPoint::MainLoop --
 *
 *     Pause until checkpoint time; then:
 *       * walk through object list, saving all objects.
 *       * save time queue
 *       * is that it??? :)
 */
void CheckPoint::MainLoop (void)
{
  SimObject *s;
  time_t t;
  char *date;
  int flag = 1;

  while (flag) {
    
    PAUSE (chkpoint_time);

    if (chktype == SaveCtxt) {
      fprintf (fp, "1\n");
    }
    else {
      // Note: this is incredibly bogus. don't even bother trying to use
      // this type of checkpoint :)
      fprintf (fp, "0\n");
    }

    // random info
    time (&t);
    date = ctime (&t);
    fprintf (fp, "@@@@ %s", date);

    // save each object in the object queue
    flag = 0;
    for (s = SimObject::obj_list; s; s = s->next) {
      if (s->my_id == -1) continue;
      
      flag = 1;
      // Note: this whole checkpointing business can't handle tasks that 
      // die in the middle or are dynamically generated (surprise,
      // surprise)
      fprintf (fp, "%u\n", s->my_id);

      // call process method to save its local state
      s->SaveState (fp);

#if defined (SYNCHRONOUS)
      // save state of the task structure
      if (chktype == SaveCtxt)
	task_write (fp, s->t, 1);
      else
	task_write (fp, s->t, 0);
#elif defined (ASYNCHRONOUS)
      if (chktype == SaveCtxt)
	thread_write (fp, s->t, 1);
      else
	thread_write (fp, s->t, 0);
#endif
    }
#if defined (SYNCHRONOUS)
    /* save etime queue state */
    SaveQueueState (fp, &etime);
    /* I'm done */
#endif
  }
}


/*
 *  Create empty hash, setup file pointer
 */
UnCheckPoint::UnCheckPoint(FILE *f, int ready) : SimObject (ready)
{
  fp = f; 
  UndoNumbering (); 
  H = ihash_new (32);
}

UnCheckPoint::~UnCheckPoint ()
{
  ihash_free (H);
}

/*
 *   UnCheckPoint::MainLoop --
 *
 *     Wander through object list, uncheckpointing...
 *
 */
void UnCheckPoint::MainLoop (void)
{
  SimObject *s;
  ihash_bucket_t *b;
  int i, tmp;
  char buf[128];
  
  PAUSE(1);			// everyone else has had a chance to go

  fgets (buf, 1024, fp);
  sscanf (buf, "%d", &i);
  fgets (buf, 1024, fp);
  
  printf ("RESTORING from CHECKPOINT [%s]\n", i ? "-raw-" : "-normal-");
  printf ("Checkpoint made: %s", buf+5);

  for (s = SimObject::obj_list; s; s = s->next) {
    if (s->my_id == -1) continue;
    Assert (!(b = ihash_lookup (H, s->my_id)), "Duplicate id's!");
    b = ihash_add (H, s->my_id);
    b->v = s;
#if defined (SYNCHRONOUS)
    s->t->tasklist = NULL;
#endif
  }

  for (s = SimObject::obj_list; s; s = s->next) {
    if (s->my_id == -1) continue;

    fscanf (fp, "%u", &tmp);
    if (tmp != s->my_id)
      printf ("WARNING: bogus! This is probably disastrous... enjoy! :)\n");

    s->RestoreState (fp, this);
#if defined(SYNCHRONOUS)
    task_read (fp, s->t, i);
#elif defined(ASYNCHRONOUS)
    thread_read (fp, s->t, i);
#endif
  }

#if defined(SYNCHRONOUS)
  /* restore etime, ectail queue */
  RestoreQueueState (fp, &etime, this);
#endif

  printf ("RESTORE complete, simulation time = %lu\n", 
	  (unsigned long)get_time ()/2);
  /* I'm done */
}


/*
 *  UnCheckPoint::ObjectPointer --
 *
 *     Return the simobject corresponding to the id
 *
 */
SimObject *UnCheckPoint::ObjectPointer (long id)
{
  ihash_bucket_t *b;

  Assert (b = ihash_lookup (H, id), "Inconsistent id; probably restoring from wrong checkpoint.");
  return (SimObject *)b->v;
}


#if defined (SYNCHRONOUS)
/*
 *   SaveQueueState --
 *
 *     Given a queue, dump it to file!
 *
 */
void SaveQueueState (FILE *fp, eventcount *ec)
{
  task *t;
  SimObject *s;
  int i;

  count_write (fp, ec->count);

  i = 0;
  for (t = ec->tasklist; t; t = t->tasklist) 
    i++;
  fprintf (fp, "%d\n", i);

  for (t = ec->tasklist; t; t = t->tasklist) {
    s = (SimObject *)t->cdata2;
    fprintf (fp, "%u\n", s->my_id);
  }
}


/*
 *   RestoreQueueState --
 *
 *     Read queue from file.
 *
 */
void RestoreQueueState (FILE *fp, eventcount *ec, UnCheckPoint *uc)
{
  task *t;
  SimObject *s;
  int i;
  unsigned long l;

  count_read (fp, (LL *)&ec->count);

  fscanf (fp, "%d", &i);
  t = NULL;
  ec->tasklist = NULL; // lose it--no one cares!
  if (i > 0) {
    i--;
    fscanf (fp, "%lu", &l);
    s = uc->ObjectPointer (l);
    t = s->t;
    Assert (t->cdata2 == s, "Un oh... bad stuff!");
    ec->tasklist = t;
    for (; i; i--) {
      fscanf (fp, "%lu", &l);
      s = uc->ObjectPointer (l);
      Assert (s->t->cdata2 == s, "yikes!");
      t->tasklist = s->t;
      t = s->t;
    }
    t->tasklist = NULL;
  }
}
#endif
