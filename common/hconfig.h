/*-*-mode:c++-*-**********************************************************
 *
 *  Copyright (c) 1999-2017 Rajit Manohar
 *
 *************************************************************************/
#ifndef __CONFIG_H__
#define __CONFIG_H__

#define DEFAULT_CONFIG_FILE     "sim.conf"

void ReadConfigFile (const char *name = DEFAULT_CONFIG_FILE); 
// read variables from configuration file

char *ParamGetString (const char *name); 
// read string parameter, returns NULL if non-existent

int ParamGetInt (const char *name);
// read int parameter, return 0 if non-existent

float ParamGetFloat (const char *name); 
// read floating-point parameter, return 0 if non-existent

#define PARAM_YES "Yes"
#define PARAM_NO "No"
int ParamGetBool (const char *name);
// read boolean parameter, return 1 if TRUE, 0 if FALSE. return 0 if non-existent

typedef unsigned long long int LL;
LL ParamGetLL (const char *name);
// read LL parameter, return 0 if non-existent

void RegisterDefault (const char *name, int val);
void RegisterDefault (const char *name, LL val);
void RegisterDefault (const char *name, float val);
void RegisterDefault (const char *name, const char *s);
void OverrideConfig (const char *name, const char *value);

#endif /* __CONFIG_H__ */
