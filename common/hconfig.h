/*************************************************************************
 *
 *  Another configuration file format (hierarchical)
 *
 *  Copyright (c) 1999-2016, 2019 Rajit Manohar
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
#ifndef __H_CONFIG_H__
#define __H_CONFIG_H__

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

#endif /* __H_CONFIG_H__ */
