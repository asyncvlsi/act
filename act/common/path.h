/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2011, 2019, 2021 Rajit Manohar
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
#ifndef __ACT_COMMON_PATH_H__
#define __ACT_COMMON_PATH_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct path_info path_info_t;

/**
 *
 *  Returns an initialized path information structure
 *
 *  @return opaque object used to manage file paths
 *
 */
path_info_t *path_init (void);


/**
 *
 *  Adds a list of colon-separated paths to the path info object
 *
 */
void path_add (path_info_t *, const char *);


/**
 *
 *  Removes a list of colon-separated paths from the path info object
 *
 */
void path_remove (path_info_t *, const char *);

/**
 *
 *  Clear search path
 *
 */
void path_clear (path_info_t *);

/**
 *
 * Release all storage
 *
 */
void path_free (path_info_t *);
  
/**
 *
 *  Returns a string corresponding to the path name
 *
 *  @param name is the raw name of the import file
 *  @return string corresponded to the fully expanded path name
 *
 */
char *path_open (path_info_t *, const char *name, const char *ext);

  /**
   * Returns a string corresponding to the path name, NULL if it
   * couldn't be found. Skips the current working directory.
   * 
   * @param name is the raw name of the import file
   * @return string corresponds to the fully expanded path name
   */
char *path_open_skipcwd (path_info_t *, const char *name, const char *ext);

#ifdef __cplusplus
}
#endif

#endif /* __ACT_COMMON_PATH_H__ */
