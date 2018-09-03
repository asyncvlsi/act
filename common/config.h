/*************************************************************************
 *
 *  Copyright (c) 2009-2018 Rajit Manohar
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
void config_append_path (const char *s);

/* Add standard configuration path
   .  <- highest
   $ACT_HOME/lib/tool/ <- next
   $CAD_HOME/lib/tool/ <- next
*/
void config_std_path (const char *tool);

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
