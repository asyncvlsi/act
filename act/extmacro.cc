/*************************************************************************
 *
 *  Copyright (c) 2022 Rajit Manohar
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
#include <unistd.h>
#include <string.h>
#include <common/misc.h>
#include <act/extmacro.h>

ExternMacro::ExternMacro (Process *p)
{
  char buf[10240];

  ActNamespace::Act()->msnprintfproc (buf, 10240, p);

  _name = Strdup (buf);
  _lef = NULL;
  _spice = NULL;
  _verilog = NULL;
  llx = 0;
  lly = 0;
  urx = -1;
  ury = -1;

  /* at a minimum, you need LEF */

  snprintf (buf, 10240, "macros.%s.lef", _name);
  if (config_exists (buf)) {
    _lef = config_get_string (buf);
  }
  else {
    /* try and generate this! */
    /* see if we can dynamically generate a blob */
    char tmp[1024];
    ActNamespace::Act()->msnprintfproc (tmp, 1024, p->getUnexpanded());
    snprintf (buf, 10240, "macros.%s.gen", tmp);
    if (config_exists (buf)) {
      char *nm = config_get_string (buf);
      /* a generator exists! */
      ActNamespace::Act()->msnprintfproc (buf, 10240, p);
      snprintf (buf, 10240, "%s _tmp_.conf %s", nm, _name);
      int len = strlen (buf);
      /* template parameters */
      for (int i=0; i < p->getNumParams(); i++) {
	const char *nm = p->getPortName (-(i+1));
	ActId *tid = new ActId (nm);
	Expr *e = tid->Eval (p->getns(), p->CurScope(), 0, 0);
	if (e->type == E_INT) {
	  snprintf (buf + len, 10240 - len, " %lu", e->u.v);
	}
	else if (e->type == E_TRUE || e->type == E_FALSE) {
	  snprintf (buf + len, 10240 - len, " %c",
		    e->type == E_TRUE ? 't' : 'f');
	}
	else {
	  warning ("Only integer and Boolean template parameters supported for generators");
	}
	len += strlen (buf+len);
      }
      /* -- running generator -- */
      system (buf);
      config_read ("_tmp_.conf");
      unlink ("_tmp_.conf");

      snprintf (buf, 10240, "macros.%s.lef", _name);
      if (config_exists (buf)) {
	_lef = config_get_string (buf);
      }
      else {
	return;
      }
    }
    else {
      return;
    }
  }
}

ExternMacro::~ExternMacro ()
{
  FREE (_name);
}
