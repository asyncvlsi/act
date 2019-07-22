/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
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
#include <act/act.h>
#include <string.h>


ActPass::ActPass (Act *_a, const char *s)
{
  Assert (_a, "ActPass: requires non-NULL Act pointer!");
  _finished = 0;
  a = _a;
  deps = list_new ();
  _a->pass_register (s, this);
  name = _a->pass_name (s);
}

ActPass::~ActPass ()
{
  a->pass_unregister (getName());
  list_free (deps);
}

int ActPass::AddDependency (const char *pass)
{
  ActPass *p = a->pass_find (pass);
  if (!p) {
    fatal_error ("Adding dependency to unknown pass `%s'", pass);
  }
  list_append (deps, p);
  return 1;
}

int ActPass::rundeps (Process *p)
{
  listitem_t *li;

  if (completed()) return 1;

  if (!pending()) {
    _finished = 1;
  }
  for (li = list_first (deps); li; li = list_next (li)) {
    ActPass *x = (ActPass *) list_value (li);
    if (x->pending()) {
      fatal_error ("Cyclic dependency to pass %s", x->getName());
      return 0;
    }
    if (!x->completed()) {
      if (!x->run (p)) {
	fatal_error ("Pass %s failed; aborting", x->getName());
	return 0;
      }
    }
  }
  return 1;
}


const char *ActPass::getName ()
{
  return name;
}

/* dummies */
int ActPass::run (Process *p) { return 0; }

int ActPass::init () { return 0; }
 
