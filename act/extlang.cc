/*************************************************************************
 *
 *  Copyright (c) 2023 Rajit Manohar
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
#include "act_parse_int.h"
#include "act_walk.extra.h"
#include <act/types.h>
#include <act/extlang.h>
#include <act/namespaces.h>

extern list_t *act_parse_id_stack (void);

static struct ExtLibs *_ext_lang = NULL;

extern "C" void act_init_extern_lang (LFILE *l)
{
  // look for libraries in support of external languages
  _ext_lang = act_read_extern_table ("act.extern_lang");
}

extern "C" int act_is_a_extern_lang (LFILE *l)
{
  list_t *t = act_parse_id_stack ();
  
  if (!_ext_lang || list_length (t) == 0) {
    if (list_length (t) > 0) {
      const char *nm = (const char *) stack_peek (t);
      act_error_ctxt (stderr);
      fprintf (stderr, "No parser found for external language `%s'; skipping!\n", nm);
    }
    return 1;
  }
  
  const char *val = ((const char *) stack_peek (t));
  char buf[1024];
  snprintf (buf, 1024, "is_a_%s<>", val);

  int (*f)(LFILE *) =
    (int (*)(LFILE *))
    act_find_dl_func (_ext_lang, ActNamespace::Global(), buf);
  
  if (!f) {
    act_error_ctxt (stderr);
    fprintf (stderr, "No parser found for external language `%s'; skipping!\n", val);
    return 1;
  }
  
  return (*f) (l);
}

extern "C" void *act_parse_a_extern_lang (LFILE *l)
{
  char buf[1024];

  const char *val = (const char *) stack_peek (act_parse_id_stack());
  snprintf (buf, 1024, "parse_a_%s<>", val);

  void *(*f) (LFILE *) = NULL;

  if (_ext_lang) {
    f = (void * (*) (LFILE *))
      act_find_dl_func (_ext_lang, ActNamespace::Global(), buf);
  }

  if (!f) {
    /* dummy parser to ignore the body */
    int braces = 1;
    int start = file_addtoken (l,  "{");
    int end = file_addtoken (l,  "}");
    while (!file_eof (l) && braces > 0) {
      if (file_sym (l) == start) {
	braces++;
	file_getsym (l);
      }
      else if (file_sym (l) == end) {
	braces--;
	if (braces != 0) {
	  file_getsym (l);
	}
      }
      else {
	file_getsym (l);
      }
    }
    if (file_eof (l)) {
      fprintf (stderr, "EOF encountered while skipping external language `%s'\n", val);
      return NULL;
    }
    else {
      struct act_extern_language_header *x;
      NEW (x, struct act_extern_language_header);
      x->name = Strdup (val);
      return x;
    }
  }
  return (*f) (l);
}


extern "C" void *act_walk_X_extern_lang (ActTree *t, void *v)
{
  char buf[1024];
  struct act_extern_language_header *h;

  h = (struct act_extern_language_header *)v;
  Assert (h->name, "Hmm...");
  snprintf (buf, 1024, "walk_a_%s<>", h->name);
  void *(*f) (ActTree *, void *) = NULL;
  if (_ext_lang) {
    f = (void * (*) (ActTree *, void *))
      act_find_dl_func (_ext_lang, ActNamespace::Global(), buf);
  }
  if (!f) {
    struct act_extern_language_header *x;
    NEW (x, struct act_extern_language_header);
    x->name = Strdup (h->name);
    return x;
  }
  return (*f) (t, v);
}

extern "C" void act_free_a_extern_lang (void *v)
{
  char buf[1024];
  struct act_extern_language_header *h;

  h = (struct act_extern_language_header *)v;
  Assert (h->name, "Hmm...");
  snprintf (buf, 1024, "free_a_%s<>", h->name);
  
  void (*f) (void *) = NULL;
  if (_ext_lang) {
    f = (void (*) (void *))
      act_find_dl_func (_ext_lang, ActNamespace::Global(), buf);
  }
  if (!f) {
    FREE ((char *)h->name);
    FREE (h);
    return;
  }
  (*f) (v);
}


void lang_extern_print (FILE *fp, const char *nm, void *v)
{
  char buf[1024];
  struct act_extern_language_header *h;

  if (!v) return;

  h = (struct act_extern_language_header *)v;
  Assert (h->name, "Hmm...");
  snprintf (buf, 1024, "print_a_%s<>", h->name);

  void (*f) (FILE *, void *) = 
    (void (*) (FILE *, void *))
    act_find_dl_func (_ext_lang, ActNamespace::Global(), buf);
  if (!f) {
    return;
  }
  fprintf (fp, "extern (%s) {\n", h->name);
  (*f) (fp, v);
  fprintf (fp, "\n}\n");
}

void *lang_extern_expand (const char *nm, void *v, ActNamespace *ns, Scope *s)
{
  char buf[1024];
  struct act_extern_language_header *h;

  if (!v) return NULL;

  h = (struct act_extern_language_header *)v;
  Assert (h->name, "Hmm...");
  Assert (strcmp (nm, h->name) == 0, "What!");
  snprintf (buf, 1024, "expand_a_%s<>", h->name);
  void *(*f) (void *, ActNamespace *, Scope *) = 
    (void *(*) (void *, ActNamespace *, Scope *))
    act_find_dl_func (_ext_lang, ActNamespace::Global(), buf);
  if (!f) {
    return NULL;
  }
  return (*f) (v, ns, s);
}
