/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2019 Rajit Manohar
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
#ifndef __MY_PATH_H__
#define __MY_PATH_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 *  Returns a string corresponding to the path name
 *
 *  @param name is the raw name of the import file
 *  @return string corresponded to the fully expanded path name
 *
 */
char *act_path_open (char *name);

/**
 *  Determines if we are importing the specified file at the moment.
 *
 *  @param file is the name of the import file
 *  @return 1 if there is an import pending on the specified file, and
 *  0 otherwise
 */
int act_pending_import (char *file);

/**
 *  Error reporting: shows the history of imports
 *
 *  @param fp is the output file pointer to which the error message is
 *  displayed
 */
void act_print_import_stack (FILE *fp);

/**
 *  Indicates that the file import is completed, and pops the imported
 *  file of the current import stack
 *
 *  @param file is the name of the import
 */
void act_pop_import (char *file);

/**
 *  Adds the specified file to the import list
 *
 *  @param file is the name of the import file
 */
void act_push_import (char *file);

/**
 *  Determines if a file has already been imported. Used to avoid
 *  duplicate imports of the same import specifier
 *
 *  @param file is the specified import
 *  @return 1 if the specified import has been previously procsssed,
 *  0 otherwise
 */
int act_isimported (const char *file);

#ifdef __cplusplus
}
#endif

#endif /* __MY_PATH_H__ */
