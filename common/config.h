/*************************************************************************
 *
 *  Functions for reading standard configuration file format.
 *
 *  Copyright (c) 2009-2019 Rajit Manohar
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
#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* append to config search path; the last call contains the path with
   the highest priority */
void config_append_path (const char *s);

/* Add standard configuration path
   .  <- highest
   $ACT_HOME/lib/tool/ <- next
   $CAD_HOME/lib/tool/ <- next
*/
void config_std_path (const char *tool);

/* What file name is actually being read? */
char *config_file_name (const char *name);

/*
  Add standard tech-specific config path
    . <- highest
    $ACT_HOME/lib/tech/ <- next
    $CAD_HOME/lib/tech/  <- next
*/
void config_stdtech_path (const char *tech);  

/* read configuration file */
void config_read (const char *s);

/* set prefix for the configuration */
void config_push_prefix (const char *s);
void config_pop_prefix (void);

/* read parameter from configuration file (or default value) */
int config_get_int (const char *s);
double config_get_real (const char *s);
char *config_get_string (const char *s);

/* table read functions */
int config_get_table_size (const char *s);
double *config_get_table_real (const char *s);
int *config_get_table_int (const char *s);
char **config_get_table_string (const char *s);

/* set default value */
void config_set_default_int (const char *s, int v);
void config_set_default_real (const char *s, double v);
void config_set_default_string (const char *s, const char *t);

/* set value: if this API is used, any file read in will not 
   override the value */
void config_set_int (const char *s, int v);
void config_set_real (const char *s, double v);
void config_set_string (const char *s, const char *t);

/* clear configuration tables */
void config_clear (void);

/* check if variable exists */
int config_exists (const char *s);

/* dump config table to file */
void config_dump (FILE *s);

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H__ */
