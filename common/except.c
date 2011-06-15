/*************************************************************************
 *
 *  Copyright (c) 2009 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <setjmp.h>
#include "except.h"
#include "misc.h"


static int cur_type = -1;
static char *cur_msg = NULL;

static except_t *root = NULL;
static except_t *freelist = NULL;


static except_t *alloc_except (void)
{
  except_t *e;
  if (freelist) {
    e = freelist;
    freelist = freelist->next;
  }
  else {
    NEW (e, except_t);
  }
  e->next = NULL;
  e->msg = NULL;
  e->type = -1;
  
  return e;
}

static void free_except (except_t *e)
{
  e->next = freelist;
  freelist = e;
  if (e->msg) FREE (e->msg);
}


void _except_setup (void)
{
  except_t *e = alloc_except ();
  e->next = root;
  root = e;
}

except_t *_except_env (void)
{
  Assert (root, "-- exn stack error --");
  return root;
}

void except_error (void)
{
  Assert (root, "-- exn stack error --");
  if (cur_msg) {
    FREE (cur_msg);
  }
  cur_msg = root->msg;
  cur_type = root->type;
  root->msg = NULL;
  except_done ();
}


#if 0
/*------------------------------------------------------------------------
 *
 *  except_try --
 *
 *   Allocate a new exception stack reference
 *
 *------------------------------------------------------------------------
 */
int
except_try (void)
{
  except_t *e = alloc_except ();

  e->next = root;
  root = e;

  if (setjmp (e->env) == 0) {
    return 1;
  }
  else {
    Assert (root, "-- exn stack error --");
    if (cur_msg) {
      FREE (cur_msg);
    }
    cur_msg = root->msg;
    cur_type = root->type;
    root->msg = NULL;
    except_done ();
    return 0;
  }    
}
#endif

/*------------------------------------------------------------------------
 *
 *  except_done --
 *
 *   Pop exception stack
 *
 *------------------------------------------------------------------------
 */
void
except_done (void)
{
  except_t *e; 
  Assert (root, " -- exn stack error -- ");

  e = root;
  root = root->next;
  free_except (e);
}

/*------------------------------------------------------------------------
 *
 *  except_type --
 *
 *   Returns current exception type
 *
 *------------------------------------------------------------------------
 */
int
except_type (void)
{
  return cur_type;
}

/*------------------------------------------------------------------------
 *
 *  except_arg --
 *
 *   Returns exception argument
 *
 *------------------------------------------------------------------------
 */
char *
except_arg (void)
{
  return cur_msg;
}


/*------------------------------------------------------------------------
 *
 *  except_throw --
 *
 *   Throw 
 *
 *------------------------------------------------------------------------
 */
void
except_throw (int type, char *arg)
{
  if (!root) {
    fprintf (stderr, "Exception: number=%d, message %s\n", type,
	     arg ? arg : "-none-");
    fatal_error ("Uncaught exception");
  }

  root->type = type;
  if (arg) {
    root->msg = Strdup (arg);
  }
  else {
    root->msg = NULL;
  }
  longjmp (root->env, 1);
}
