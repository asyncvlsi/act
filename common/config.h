/*************************************************************************
 *
 *  Copyright (c) 2009 Rajit Manohar
 *  All Rights Reserved
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
void config_append_path (char *s);

/* Add standard configuration path
   .  <- highest
   $ACT_HOME/lib/tool/ <- next
   $CAD_HOME/lib/tool/ <- next
*/
void config_std_path (char *tool);

/* read configuration file */
void config_read (char *s);

/* read parameter from configuration file (or default value) */
int config_get_int (char *s);
double config_get_real (char *s);
char *config_get_string (char *s);

/* table read functions */
int config_get_table_size (char *s);
double *config_get_table_real (char *s);
int *config_get_table_int (char *s);

/* set default value */
void config_set_default_int (char *s, int v);
void config_set_default_real (char *s, double v);
void config_set_default_string (char *s, char *t);

/* clear configuration tables */
void config_clear (void);

/* check if variable exists */
int config_exists (char *s);

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H__ */
