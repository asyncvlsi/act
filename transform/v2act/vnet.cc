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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vnet.h"
#include "v_parse.h"
#include "v_walk_X.h"

static
char *find_library (const char *s)
{
  char buf[10240];
  char *ret;
  FILE *tmp;

  ret = NULL;
  if (getenv ("CAD_HOME")) {
    sprintf (buf, "%s/lib/v2act/%s", getenv ("CAD_HOME"), s);
    tmp = fopen (buf, "r");
    if (tmp) {
      fclose (tmp);
      ret = buf;
    }
  }
  if (!ret && getenv ("ACT_HOME")) {
    sprintf (buf, "%s/lib/v2act/%s", getenv ("ACT_HOME"), s);
    tmp = fopen (buf, "r");
    if (tmp) {
      fclose (tmp);
      ret = buf;
    }
  }
  if (ret) {
    return Strdup (ret);
  }
  else {
    return NULL;
  }
}


VNet *verilog_read (const char *netlist, const char *actlib)
{
  char *s;
  v_Token *t;
  VNet *w;
  Act *a;
    
  s = find_library (actlib);
  if (!s) {
    a = new Act (actlib);
  }
  else {
    a = new Act (s);
    FREE (s);
  }

  t = v_parse (netlist);

  NEW (w, VNet);
  w->a = a;
  w->out = stdout;
  w->prefix = NULL;
  w->M = hash_new (32);
  w->hd = NULL;
  w->tl = NULL;
  w->missing = hash_new (8);

  v_walk_X (w, t);
  
  return w;
}
