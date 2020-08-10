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

/*
  Interface to the Lisp interpreter from the textio module
*/

extern void LispInit(void);
extern void LispSetVariable(char *name, char *value);
extern void LispEvaluate(int argc, char **argv, int infile);

/* 
   User must provide this function and this variable
*/
extern int LispDispatch (int argc, char **argv, int echo_cmd, int infile);
extern int LispInterruptExecution;

#endif /* __LISP_H__ */

