/*************************************************************************
 *
 *  Copyright (c) 2009 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __EXCEPT_H__
#define __EXCEPT_H__

/*
  Faking exceptions in C
*/


#include <stdio.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _except_ except_t;

void except_init (void);
   /* initialize exceptions package */

void except_done (void);

int except_type (void);

char *except_arg (void);

void except_throw (int type, char *arg);

void except_error (void);

void _except_setup (void);
except_t *_except_env (void);


/*
  try {
  ...
  }
  catch {
  case ...
  }

Turns into:

  if (except_try ()) {



    except_done ();
  }
  else {
    except_error ();
    switch (except_type ()) {
     case T1:
     case T2:
     ...
    }
  }


  throw exception 

turns into:

  except_throw (type, arg);

  ========================================================================

  With macros, this is:

  TRY {
    ...

  } CATCH {
     EXCEPT_SWITCH {
     case T1:
     ...
     }
  }

*/

struct _except_ {
  int type;
  char *msg;
  jmp_buf env;
  struct _except_ *next;
};


#define TRY					\
  _except_setup ();				\
  if (setjmp (_except_env()->env) == 0) {

#define CATCH except_done(); } else

#define EXCEPT_SWITCH except_error(); switch (except_type())

#define THROW(x)  except_throw ((x), NULL)
#define THROW2(x,y) except_throw ((x), (y))

#define DEFAULT_CASE  default: except_throw (except_type (), except_arg ()); break

/* common exceptions */

#define EXC_NULL_EXCEPTION   1

#ifdef __cplusplus
}
#endif

#endif /* __EXCEPT_H__ */
