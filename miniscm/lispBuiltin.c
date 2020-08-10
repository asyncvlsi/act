/*************************************************************************
 *
 *  Copyright (c) 1996, 2020 Rajit Manohar
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
/*************************************************************************
 *
 *  lispBuiltin.c --
 *
 *   Forces builtin symbol
 *
 *************************************************************************
 */
#include <stdio.h>

#include "lisp.h"
#include "lispInt.h"
#include "lispargs.h"

static LispObj *_internal_list;


/*-----------------------------------------------------------------------------
 *
 *  LispEvalBuiltin --
 *
 *      Force its argument to be a user-defined builtin function
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

LispObj *
LispEvalBuiltin (char *name, Sexp *s, Sexp *f)
{
  LispObj *l;
  if (!ARG1P(s) || LTYPE(ARG1(s)) != S_SYM || ARG2P(s)) {
    fprintf (stderr, "Usage: (%s symbol)\n",name);
    RETURN;
  }
  l = LispCopyObj(ARG1(s));
  LTYPE(l) = S_MAGIC_BUILTIN;
  return l;
}

