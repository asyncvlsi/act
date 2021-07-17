/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#include <act/act.h>
#include <act/types.h>
#include <act/iter.h>
#include <string.h>
#include <common/misc.h>
#include <common/hash.h>

/*------------------------------------------------------------------------
 *
 *
 *   Macros
 *
 *
 *------------------------------------------------------------------------
 */
UserMacro::UserMacro (UserDef *u, const char *name)
{
  _nm = string_cache (name);
  nports = 0;
  port_t = NULL;
  port_n = NULL;
  c = NULL;
  
  parent = u;
}


UserMacro::~UserMacro ()
{
  if (port_t) {
    for (int i=0; i < nports; i++) {
      if (port_t[i]) {
	delete port_t[i];
      }
    }
    FREE (port_t);
  }
  if (port_n) {
    FREE (port_n);
  }
  act_chp_free (c);
}


void UserMacro::Print (FILE *fp)
{
  fprintf (fp, "  macro %s (", _nm);
  for (int i=0; i < nports; i++) {
    port_t[i]->Print (fp);
    fprintf (fp, " %s", port_n[i]);
    if (i != nports-1) {
      fprintf (fp, "; ");
    }
  }
  fprintf (fp, ") {\n");
  if (c) {
    fprintf (fp, "   ");
    chp_print (fp, c);
    fprintf (fp, "\n");
  }
  fprintf (fp, "  }\n");
}


int UserMacro::addPort (InstType *t, const char *id)
{
  int i;
  
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return 0;
    }
  }

  nports++;

  if (!port_n) {
    NEW (port_n, const  char *);
  }
  else {
    REALLOC (port_n, const char *, nports);
  }
  if (!port_t) {
    NEW (port_t, InstType *);
  }
  else {
    REALLOC (port_t, InstType *, nports);
  }

  port_n[nports-1] = string_cache (id);
  port_t[nports-1] = t;

  return 1;
}

UserMacro *UserMacro::Expand (UserDef *ux, ActNamespace *ns, Scope *s, int is_proc)
{
  UserMacro *ret = new UserMacro (ux, _nm);

  ret->nports = nports;
  if (nports > 0) {
    MALLOC (ret->port_t, InstType *, nports);
    MALLOC (ret->port_n, const char *, nports);
  }

  Scope *tsc = new Scope (s, 1);
  
  for (int i=0; i < nports; i++) {
    ret->port_t[i] = port_t[i]->Expand (ns, s);
    ret->port_n[i] = string_cache (port_n[i]);

    tsc->Add (ret->port_n[i], ret->port_t[i]);
  }

  for (int i=0; i < ux->getNumPorts(); i++) {
    tsc->Add (ux->getPortName (i), ux->getPortType (i));
  }

  if (is_proc) {
    chp_expand_macromode (2);
  }
  else {
    chp_expand_macromode (1);
  }
  ret->c = chp_expand (c, ns, tsc);
  chp_expand_macromode (0);

  delete tsc;

  return ret;
}


void UserMacro::setBody (struct act_chp_lang *chp)
{
  c = chp;
}


