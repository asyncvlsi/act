/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
 *  lispCli.h -- 
 *
 *   Command-line interface wrapper  for interpreter
 *
 *************************************************************************
 */
#ifndef __LISPCLI_H__
#define __LISPCLI_H__

#ifdef __cplusplus
extern "C" {
#endif
  
/*
  Interface to command-line interface
*/
struct LispCliCommand {
  const char *name;
  const char *help;
  int (*f) (int argc, char **argv);
};

/*
  Initialize CLI module
*/
void LispCliInit (const char *elrc, const char *histrc, const char *prompt,
		  struct LispCliCommand *cmds, int cmd_len);

/* this version doesn't use editline */
void LispCliInitPlain (const char *prompt,
		       struct LispCliCommand *cmds, int cmd_len);

  /** Add more commands: returns cmd_len on success, otherwise it
   returns the id of the command that did not get added. 
   If "module" is NULL, the command names are taken from the name
   field of cmds; if module is not NULL, then "module:" is pre-pended
   to all the names of the commands.
  */
int LispCliAddCommands (const char *module, 
			  struct LispCliCommand *cmds, int cmd_len);

/*  change prompt */
void LispCliSetPrompt (const char *prompt);

/*
  Cleanup CLI module
*/
void LispCliEnd (void);

/*
  Run CLI using fp as the input
  
    Return 0 if it was interrupted, 1 if successfully completed.
*/
int LispCliRun (FILE *fp);

/* 
  Use to set int return value
*/
void LispSetReturnInt (int val);


#ifdef __cplusplus
}
#endif

#endif /* __LISPCLI_H__ */
