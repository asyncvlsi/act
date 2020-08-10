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
 *  lispargs.h -- 
 *
 *   Macros for looking at various entries in a list.
 *
 *************************************************************************
 */
#define ARG1P(s)   (s)
#define ARG1(s)    CAR(s)

#define EARG2P(s)  (LTYPE(CDR(s)) != S_LIST || LLIST(CDR(s)))
#define ARG2P(s)   (LTYPE(CDR(s)) == S_LIST && LLIST(CDR(s)))
#define ARG2(s)    CAR(LLIST(CDR(s)))

#define ARG3P(s)   (LTYPE(CDR(LLIST(CDR(s)))) == S_LIST && LLIST(CDR(LLIST(CDR(s)))))
#define ARG3(s)    CAR(LLIST(CDR(LLIST(CDR(s)))))

#define ARG4P(s)   (LTYPE(CDR(LLIST(CDR(LLIST(CDR(s)))))) == S_LIST && LLIST(CDR(LLIST(CDR(LLIST(CDR(s)))))))
#define ARG4(s)    CAR(LLIST(CDR(LLIST(CDR(LLIST(CDR(s)))))))

/*
   what i'd really like is
      do { ... } while (0)
   because that forces the user to use ";" at the end . . . but
   Sun's cc thinks you should be warned about such statements -sigh-
*/
#define RETURN     { LispStackDisplay (); return NULL; }
#define RETURNPOP  { LispStackDisplay (); LispStackPop(); return NULL; }


#define NUMBER(t) ((t) == S_INT || (t) == S_FLOAT)
