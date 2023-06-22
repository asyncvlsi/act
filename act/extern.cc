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
#include <string.h>
#include <dlfcn.h>
#include <common/array.h>
#include <common/misc.h>
#include <common/config.h>
#include <act/act.h>
#include <act/types.h>

struct extern_libs {
  char *name;			/* name of library */
  char *path;			/* path to library */
  void *dl;			/* open dl */
};

struct ExtLibs {
  char *prefix;
  A_DECL (struct extern_libs, files);
};

struct ExtLibs *act_read_extern_table (const char *prefix)
{
  int i, n;
  char buf[1024];
  char **tab;
  struct ExtLibs *el; 

  snprintf (buf, 1024, "%s.libs", prefix);

  if (!config_exists (buf)) {
    return NULL;
  }
  
  n = config_get_table_size (buf);

  if (n == 0) {
    return NULL;
  }
  tab = config_get_table_string (buf);

  NEW (el, struct ExtLibs);
  A_INIT (el->files);
  
  for (i=0; i < n; i++) {
    A_NEW (el->files, struct extern_libs);
    A_NEXT (el->files).name = tab[i];
    snprintf (buf, 1024, "%s.%s.path", prefix, tab[i]);
    A_NEXT (el->files).path = config_get_string (buf);
    A_NEXT (el->files).dl = NULL;
    A_INC (el->files);
  }
  el->prefix = Strdup (prefix);
  return el;
}


void *act_find_dl_func (struct ExtLibs *el, ActNamespace *ns, const char *f)
{
  int i;
  char buf[2048];
  char *ns_name;
  void *fret;

  if (!el) {
    return NULL;
  }

  if (ns == ActNamespace::Global()) {
    ns_name = NULL;
  }
  else {
    ns_name = ns->Name(true);
  }

  fret = NULL;

  for (i=0; i < A_LEN (el->files); i++) {
    snprintf (buf, 2048, "%s.%s.%s%s", el->prefix,
	      el->files[i].name, ns_name ? (ns_name+2) : "", f);
    buf[strlen(buf)-2] = '\0';

    if (config_exists (buf)) {
      if (!el->files[i].dl) {
	el->files[i].dl = dlopen (el->files[i].path, RTLD_LAZY);
	if (!el->files[i].dl) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Expected to find `%s%s' as `%s' in library `%s'\n",
		   ns_name ? (ns_name+2) : "", f,
		   config_get_string (buf), el->files[i].path);
	  if (dlerror()) {
	    fprintf (stderr, "%s\n", dlerror());
	  }
	  fatal_error ("Could not open dynamic library `%s'", el->files[i].path);
	}
	{
	  void *tmpf = dlsym (el->files[i].dl, "_builtin_update_config");
	  if (tmpf) {
	    ((void(*)(void *))tmpf) (config_get_state());
	  }
	}
      }
      fret = dlsym (el->files[i].dl, config_get_string (buf));
      if (!f) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Expected to find `%s%s' as `%s' in library `%s'\n",
		 ns_name ? (ns_name+2) : "", f,
		 config_get_string (buf), el->files[i].path);
	fatal_error ("Could not find function in library", f,
		     config_get_string (buf), el->files[i].path);
      }
      if (ns_name) {
	FREE (ns_name);
      }
      return fret;
    }
  }      
  if (ns_name) {
    FREE (ns_name);
  }
  return NULL;
}

