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
 *  lisp.h -- 
 *
 *   This module defines things that are exported by the
 *   mini-scheme interpreter command line language to the rest
 *   of the world.
 *
 *************************************************************************
 */
#ifndef __LISP_H__
#define __LISP_H__

#ifdef __cplusplus
extern "C" {
#endif  

/*
  Interface to the Lisp interpreter from the textio module
*/

extern void LispInit(void);
extern void LispSetVariable(char *name, char *value);
extern void LispEvaluate(int argc, char **argv, int infile);

/* 
   User must provide these functions and variables
*/
extern int LispDispatch (int argc, char **argv, int echo_cmd, int infile);
/* return 0 for #f, 1 for #t, 2 for an int (returned by the
   following), 3 for a string */
extern int LispGetReturnInt (void);
extern char *LispGetReturnString (void);
extern double LispGetReturnFloat (void);

extern int LispInterruptExecution;


#ifdef __cplusplus
}
#endif  

#endif /* __LISP_H__ */

