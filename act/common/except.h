/*************************************************************************
 *
 *  Exception handling support in C
 *
 *  Copyright (c) 2009, 2019 Rajit Manohar
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

#if defined (__STDC__)
#if (__STDC_VERSION__ >= 201112L)
#define NORETURN_SPECIFIER _Noreturn
#else
#define NORETURN_SPECIFIER
#endif
#else 
#define NORETURN_SPECIFIER
#endif

typedef struct _except_ except_t;

void except_init (void);
   /* initialize exceptions package */

void except_done (void);

int except_type (void);

char *except_arg (void);

NORETURN_SPECIFIER void except_throw (int type, char *arg);

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
