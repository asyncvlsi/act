/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2017-2019 Rajit Manohar
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
#include <ctype.h>
#include <act/act.h>
#include "act_parse.h"
#include "act_walk_X.h"
#include <config.h>
#include "array.h"

#ifdef DEBUG_PERFORMANCE
#include "mytime.h"
#endif

#include <log.h>

struct command_line_defs {
  char *varname;
  union {
    int s_value;
    unsigned int u_value;
  };
  int isint;
};

int Act::max_recurse_depth;
int Act::max_loop_iterations;
int Act::warn_emptyselect;
int Act::emit_depend;
int Act::warn_double_expand;
int Act::warn_no_local_driver;
Log *Act::L;

L_A_DECL (struct command_line_defs, vars);

#define isid(x) (((x) == '_') || isalpha(x))

const char *act_model_names[ACT_MODEL_TOTAL] =
  { "chp",
    "hse",
    "prs",
    "device"
  };


/*------------------------------------------------------------------------
 *
 *  _valid_id_string --
 *
 *   Returns 1 if it is a valid id string, 0 otherwise
 *
 *------------------------------------------------------------------------
 */
static int 
_valid_id_string (char *s)
{
  if (!*s) return 0;
  if (!isid (*s)) return 0;
  s++;
  while (*s) {
    if (!isid (*s) && !isdigit (*s)) return 0;
    s++;
  }
  return 1;
}


void Act::Init (int *iargc, char ***iargv)
{
  static int initialize = 0;
  int argc = *iargc;
  char **argv = *iargv;
  int i, j;
  int tech_specified = 0;
  char *conf_file = NULL;

  if (initialize) return;
  initialize = 1;

  Assert (sizeof (void *) == sizeof (unsigned long), "Pointer hashing issues");

  Act::L = new Log("actcore", 'A');
  ActNamespace::Init();
  Type::Init();

  config_set_default_int ("act.max_recurse_depth", 1000);
  config_set_default_int ("act.max_loop_iterations", 1000);
  config_set_default_int ("act.warn.emptyselect", 0);
  config_set_default_int ("act.warn.dup_pass", 1);
  config_set_default_int ("act.warn_no_local_driver", 1);
  
  Act::emit_depend = 0;
  Act::warn_double_expand = 1;

  Log::OpenStderr ();
  Log::Initialize_LogLevel ("");

  A_INIT (vars);

  for (i=1; i < argc; i++) {
    if (strncmp (argv[i], "-D", 2) == 0) {
      char *tmp, *s, *t;
      int isint;
      int s_value;
      unsigned int u_value;

      tmp = Strdup (argv[i]+2);
      /* tmp should look like var=true or var=false */
      s = tmp;
      while (*s && *s != '=') {
	s++;
      }

      if (*s != '=') {
	fatal_error ("-D option must be of the form -Dvar=value (bool or int)");
      }
      *s = '\0';
      s++;
      if (strcmp (s, "true") != 0 &&
	  strcmp (s, "false") != 0) {
	if (strncmp (s, "0x", 2) == 0) {
	  isint = 1; /* pint */
	  t = s+2;
	  while (*t) {
	    if (('0' <= *t  && *t <= '9')
		|| ('a' <= *t && *t <= 'f')
		|| ('A' <= *t && *t <= 'F')) {
	      t++;
	    }
	    else {
	      isint = -1;
	      break;
	    }
	  }
	  if (isint != -1) {
	    sscanf (s+2, "%x", &u_value);
	  }
	}
	else {
	  if (*s == '-') {
	    isint = 2;
	    t = s + 1;
	  }
	  else {
	    isint = 1;
	    t = s;
	  }
	  while (*t) {
	    if ('0' <= *t  && *t <= '9') {
	      t++;
	    }
	    else {
	      isint = -1;
	      break;
	    }
	  }
	  if (isint == 1) {
	    sscanf (s, "%u", &u_value);
	  }
	  else if (isint == 2) {
	    sscanf (s, "%d", &s_value);
	  }
	}
      }
      else {
	isint = 0;
	if (strcmp (s, "true") == 0) {
	  u_value = 1;
	}
	else {
	  u_value = 0;
	}
      }

      if (isint == -1) {
	fatal_error ("-D option must be of the form -Dvar=value (bool or int)");
      }

      if (!_valid_id_string (tmp)) {
	fatal_error ("-D option: variable `%s' is not a valid identifier",
		     tmp);
      }

      A_NEW (vars, struct command_line_defs);
      A_NEXT (vars).varname = tmp;
      if (isint == 0) {
	A_NEXT (vars).u_value = u_value;
      }
      else if (isint == 1) {
	A_NEXT (vars).u_value = u_value;
      }
      else if (isint == 2) {
	A_NEXT (vars).s_value = s_value;
      }
      A_NEXT (vars).isint = isint;
      A_INC (vars);
    }
    else if (strncmp (argv[i], "-T", 2) == 0) {
      config_stdtech_path (argv[i]+2);
      tech_specified = 1;
    }
    else if (strncmp (argv[i], "-W", 2) == 0) {
      char *s, *tmp;
      s = Strdup (argv[i]+2);
      tmp = strtok (s, ",");
      while (tmp) {
	if (strcmp (tmp, "empty-select") == 0) {
	  config_set_int ("act.warn.emptyselect", 1);
	}
	else if (strcmp (tmp, "dup-pass:off") == 0) {
	  config_set_int ("act.warn.dup_pass", 0);
	}
	else if (strcmp (tmp, "local-driver:off") == 0) {
	  config_set_int ("act.warn_no_local_driver", 0);
	}
	else if (strcmp (tmp, "all") == 0) {
	  config_set_int ("act.warn.emptyselect", 1);
	  config_set_int ("act.warn.dup_pass", 1);
	}
	else {
	  fprintf (stderr, "FATAL: -W option `%s' is unknown", tmp);
	  fprintf (stderr, "  legal values:\n");
	  fprintf (stderr, "     empty-select, dup-pass:off, local-driver:off, all\n");
	  exit (1);
	}
	tmp = strtok (NULL, ",");
      }
      FREE (s);
    }
    else if (strncmp (argv[i], "-V", 2) == 0) {
      char *s, *tmp;
      s = Strdup (argv[i]+2);
      tmp = strtok (s, ",");
      while (tmp) {
	if (strcmp (tmp, "config") == 0) {
	  Log::UpdateLogLevel("A");
	}
	else {
	  fatal_error ("-V option `%s' is unknown", tmp);
	}
	tmp = strtok (NULL, ",");
      }
      FREE (s);
    }
    else if (strncmp (argv[i], "-log=", 5) == 0) {
      char *s;
      s = Strdup (argv[i]+5);
      Log::OpenLog (s);
      FREE (s);
    }
    else if (strncmp (argv[i], "-cnf=", 5) == 0) {
      if (conf_file) {
	FREE (conf_file);
      }
      conf_file = Strdup (argv[i]+5);
    }
    else if (strncmp (argv[i], "-ref=", 5) == 0) {
      /* refinement steps */
      int r = atoi(argv[i]+5);
      if (r < 0) {
	fatal_error ("-ref option needs a non-negative integer");
      }
      config_set_default_int ("act.refine_steps", r);
    }
    else if (strncmp (argv[i], "-lev=", 5) == 0) {
      Log::UpdateLogLevel(argv[i]+5);
    }
    else {
      break;
    }
  }
  /* arguments from position 1 ... 1 + (i-1) have been wacked */
  *iargc = (argc - i + 1);
  for (j=1; j < *iargc; j++) {
    argv[j] = argv[j+(i-1)];
  }
  argv[j] = NULL;

  if (!tech_specified) {
    config_stdtech_path ("generic");
  }

  Act::config_info ("global.conf");
  config_read ("global.conf");
  Act::config_info ("prs2net.conf");
  config_read ("prs2net.conf");
  if (conf_file) {
    config_read (conf_file);
    Act::config_info (conf_file);
  }

  Act::warn_emptyselect = config_get_int ("act.warn.emptyselect");
  Act::warn_no_local_driver = config_get_int ("act.warn_no_local_driver");
  Act::max_recurse_depth = config_get_int ("act.max_recurse_depth");
  Act::max_loop_iterations = config_get_int ("act.max_loop_iterations");
  
  return;
}


Act::Act (const char *s)
{
  act_Token *a;
  ActTree tr;
  int argc = 1;
  char **argv;

  MALLOC (argv, char *, 2);
  argv[0] = Strdup ("-internal-");
  argv[1] = NULL;

  default_level = ACT_MODEL_PRS;
  config_set_default_int ("act.refine_steps", 0);

  for (int i=0; i < ACT_MODEL_TOTAL; i++) {
    num_type_levels[i] = 0;
    num_inst_levels[i] = 0;
  }

  Act::Init (&argc, &argv);
  FREE (argv[0]);
  FREE (argv);

  refine_steps = config_get_int ("act.refine_steps");
  
  passes = hash_new (2);

  gns = ActNamespace::global;
  tf = new TypeFactory();
  
  if (!s) {
    return;
  }

  tr.curns = ActNamespace::global;
  tr.global = tr.curns;
  tr.os = new ActOpen ();
  tr.tf = tf;
  tr.scope = tr.curns->CurScope ();

  tr.u = NULL;

  tr.u_p = NULL;
  tr.u_d = NULL;
  tr.u_c = NULL;
  tr.u_f = NULL;
  tr.u_i = NULL;
  tr.t = NULL;

  tr.t_inst = NULL;

  tr.i_t = NULL;
  tr.i_id = NULL;
  tr.a_id = NULL;

  tr.supply.vdd = NULL;
  tr.supply.gnd = NULL;
  tr.supply.psc = NULL;
  tr.supply.nsc = NULL;
  
  tr.strict_checking = 0;

  tr.in_tree = 0;
  tr.in_subckt = 0;

  tr.attr_num = config_get_table_size ("act.instance_attr");
  tr.attr_table = config_get_table_string ("act.instance_attr");

  tr.sizing_info = NULL;
  tr.sz_loop_stack = list_new ();
  tr.skip_id_check = 0;

  tr.emit_depend = Act::emit_depend;
  tr.in_cond = 0;

  tr.ptype_expand = 0;
  tr.ptype_name = NULL;

#ifdef DEBUG_PERFORMANCE
  realtime_msec ();
#endif
  expr_parse_basecase_bool = act_parse_expr_syn_loop_bool;
  expr_parse_basecase_num = act_parse_expr_intexpr_base;
  expr_parse_newtokens = act_expr_parse_newtokens;

  a = act_parse (s);

#ifdef DEBUG_PERFORMANCE
  printf ("Parser time: %g\n", (realtime_msec()/1000.0));
#endif

  ActBody *bhead, *prev;
  InstType *it;
  bhead = NULL;
  prev = NULL;
  for (int i=0; i < A_LEN (vars); i++) {
    ActBody *b;
    ActBody *bc;
    Expr *e;
    /* add variables to the global namespace
       Note: copied from instance[] rule in defs.m4
     */
    if (vars[i].isint == 0) {
      it = tr.tf->NewPBool();
    }
    else if (vars[i].isint == 1) {
      it = tr.tf->NewPInt();
    }
    else if (vars[i].isint == 2) {
      /* pint is signed */
      it = tr.tf->NewPInt();
    }
    else {
      Assert (0 <= vars[i].isint && vars[i].isint <= 2, "What?");
    }
    if (tr.global->CurScope()->Lookup (vars[i].varname)) {
      fatal_error ("Name `%s' defined through -D is a duplicate!", vars[i].varname);
    }
    tr.global->CurScope()->Add (vars[i].varname, it);
    b = new ActBody_Inst (it, vars[i].varname);
    NEW (e, Expr);

    if (vars[i].isint == 0) {
      e->type = (vars[i].u_value ? E_TRUE : E_FALSE);
    }
    else if (vars[i].isint == 1) {
      e->type = E_INT;
      e->u.v = vars[i].u_value;
    }
    else {
      e->type = E_INT;
      e->u.v = vars[i].s_value;
    }

    Expr *tmp = TypeFactory::NewExpr (e);
    FREE (e);
    e = tmp;

    bc = new ActBody_Conn (new ActId (vars[i].varname),
			   new AExpr (e));
    b->Append (bc);
    if (!bhead) {
      bhead = b;
      prev = bhead->Tail();
    }
    else {
      prev->Append (b);
      prev = prev->Tail ();
    }
  }
  if (bhead) {
    tr.global->AppendBody (bhead);
  }

  act_walk_X (&tr, a);
  
  act_parse_free (a);

#ifdef DEBUG_PERFORMANCE
  printf ("Walk and free time: %g\n", (realtime_msec()/1000.0));
#endif
  if (config_exists ("act.mangle_letter")) {
    const char *tmp = config_get_string ("act.mangle_letter");
    if (!mangle_set_char (*tmp)) {
      fatal_error ("act.mangle_letter: could not be used as a character!");
    }
  }
  else {
    mangle_set_char ('_');
  }
  if (config_exists ("act.mangle_chars")) {
    mangle (config_get_string ("act.mangle_chars"));
  }
  ActNamespace::act = this;

  /*-- now update level flags --*/

  if (config_exists ("level.default")) {
    const char *s = config_get_string ("level.default");

    default_level = -1;
    for (int i=0; i < ACT_MODEL_TOTAL; i++) {
      if (strcmp (s, act_model_names[i]) == 0) {
	default_level = i;
	break;
      }
    }
    if (default_level == -1) {
      fprintf (stderr, "level.default must be one of {");
      for (int i=0; i < ACT_MODEL_TOTAL; i++) {
	if (i != 0) {
	  fprintf (stderr, ", ");
	}
	fprintf (stderr, "%s", act_model_names[i]);
      }
      fprintf (stderr, "}\n");
      fatal_error ("Illegal level.default in configuration file.");
    }
  }

  for (int i=0; i < ACT_MODEL_TOTAL; i++) {
    char buf[1024];
    snprintf (buf, 1024, "level.types.%s", act_model_names[i]);
    if (config_exists (buf)) {
      num_type_levels[i] = config_get_table_size (buf);
      type_levels[i] = config_get_table_string (buf);
    }
    snprintf (buf, 1024, "level.inst.%s", act_model_names[i]);
    if (config_exists (buf)) {
      num_inst_levels[i] = config_get_table_size (buf);
      inst_levels[i] = config_get_table_string (buf);
    }
  }
}

void Act::Merge (const char *s)
{
  act_Token *a;
  ActTree tr;

  if (!s) {
    return;
  }

  tr.curns = ActNamespace::global;
  tr.global = tr.curns;
  tr.os = new ActOpen ();
  tr.tf = tf;
  tr.scope = tr.curns->CurScope ();

  tr.u = NULL;

  tr.u_p = NULL;
  tr.u_d = NULL;
  tr.u_c = NULL;
  tr.u_f = NULL;
  tr.u_i = NULL;
  tr.t = NULL;

  tr.t_inst = NULL;

  tr.i_t = NULL;
  tr.i_id = NULL;
  tr.a_id = NULL;

  tr.supply.vdd = NULL;
  tr.supply.gnd = NULL;
  tr.supply.psc = NULL;
  tr.supply.nsc = NULL;
  
  tr.strict_checking = 0;

  tr.in_tree = 0;
  tr.in_subckt = 0;

  tr.attr_num = config_get_table_size ("act.instance_attr");
  tr.attr_table = config_get_table_string ("act.instance_attr");

  tr.sizing_info = NULL;

  tr.in_cond = 0;
  tr.ptype_expand = 0;
  tr.ptype_name = NULL;
  

#ifdef DEBUG_PERFORMANCE
  realtime_msec ();
#endif

  expr_parse_basecase_bool = act_parse_expr_syn_loop_bool;
  expr_parse_basecase_num = act_parse_expr_intexpr_base;
  expr_parse_newtokens = act_expr_parse_newtokens;

  a = act_parse (s);

#ifdef DEBUG_PERFORMANCE
  printf ("Parser time: %g\n", (realtime_msec()/1000.0));
#endif

  act_walk_X (&tr, a);
  
  act_parse_free (a);

#ifdef DEBUG_PERFORMANCE
  printf ("Walk and free time: %g\n", (realtime_msec()/1000.0));
#endif
}


void Act::Expand ()
{
  Assert (gns, "Expand() called without an object?");
  /* expand each namespace! */
  gns->Expand ();
}


UserDef *Act::findUserdef (const char *s)
{
  if (!s) return NULL;
  
  int i;
  char *tmp = Strdup (s);
  char *f = tmp;
  ActNamespace *ns = gns;

  i = 0;
  while (tmp[i]) {
    if (tmp[i] == ':') {
      if (tmp[i+1] != ':') {
	return NULL;
      }
      else {
	tmp[i] = '\0';
	if (i == 0) {
	  ns = gns;
	}
	else {
	  ns = ns->findNS (tmp);
	}
	if (!ns) {
	  return NULL;
	}
	tmp = tmp + i + 2;
	i = 0;
      }
    }
    else {
      i++;
    }
  }

  UserDef *u = ns->findType (tmp);
  FREE (f);
  return u;
}



Process *Act::findProcess (const char *s)
{
  if (!s) return NULL;
  
  int i;
  char *tmp = Strdup (s);
  char *f = tmp;
  ActNamespace *ns = gns;

  i = 0;
  while (tmp[i]) {
    if (tmp[i] == ':') {
      if (tmp[i+1] != ':') {
	return NULL;
      }
      else {
	tmp[i] = '\0';
	if (i == 0) {
	  ns = gns;
	}
	else {
	  ns = ns->findNS (tmp);
	}
	if (!ns) {
	  return NULL;
	}
	tmp = tmp + i + 2;
	i = 0;
      }
    }
    else {
      i++;
    }
  }

  Process *p = findProcess (ns, tmp);
  FREE (f);
  return p;
}

Process *Act::findProcess (ActNamespace *n, const char *s)
{
  UserDef *u;

  if (!s || !n) return NULL;

  u = n->findType (s);
  if (!u) return NULL;
  return dynamic_cast<Process *>(u);
}


ActNamespace *Act::findNamespace (const char *s)
{
  if (!s) return NULL;
  
  int i;
  char *tmp = Strdup (s);
  char *f = tmp;
  ActNamespace *ns = gns;

  i = 0;
  while (tmp[i]) {
    if (tmp[i] == ':') {
      if (tmp[i+1] != ':') {
	return NULL;
      }
      else {
	tmp[i] = '\0';
	if (i == 0) {
	  ns = gns;
	}
	else {
	  ns = ns->findNS (tmp);
	}
	if (!ns) {
	  return NULL;
	}
	tmp = tmp + i + 2;
	i = 0;
      }
    }
    else {
      i++;
    }
  }

  ns = ns->findNS (tmp);
  
  FREE (f);

  return ns;
}


ActNamespace *Act::findNamespace (ActNamespace *ns, const char *s)
{
  if (!ns || !s) return NULL;
  return ns->findNS (s);
}
		 
void Act::Print (FILE *fp)
{
  gns->Print (fp);
}


void Act::pass_register (const char *name, ActPass *p)
{
  hash_bucket_t *b;

  b = hash_lookup (passes, name);
  if (b) {
    if (b->v) {
      if (config_get_int ("act.warn.dup_pass")) {
	warning ("Act::pass_register is replacing an existing pass `%s'", name);
      }
    }
  }
  else {
    b = hash_add (passes, name);
  }
  b->v = p;
}

ActPass *Act::pass_find (const char *name)
{
  hash_bucket_t *b;

  b = hash_lookup (passes, name);
  if (b) {
    return (ActPass *)b->v;
  }
  else {
    return NULL;
  }
}

void Act::pass_unregister (const char *name)
{
  hash_bucket_t *b;

  b = hash_lookup (passes, name);
  if (!b) {
    if (config_get_int ("act.warn.dup_pass")) {
      warning ("Act::pass_unregister, pass `%s' doesn't exist", name);
    }
  }
  else {
    hash_delete (passes, name);
  }
}

const char *Act::pass_name (const char *name)
{
  hash_bucket_t *b;
  b = hash_lookup (passes, name);
  if (!b) return NULL;
  return b->key;
}
			   

void Act::config_info (const char *name)
{
  char *s = config_file_name (name);
  if (s) {
    (*L) << "Read configuration file: " << s << "\n";
    FREE (s);
  }
}


void Act::generic_msg (const char *msg)
{
  (*L) << msg;
}


int Act::getLevel ()
{
  return default_level;
}

int Act::getLevel (Process *p)
{
  int lev;

  if (!p) {
    return -1;
  }
  
  for (lev = 0; lev < ACT_MODEL_TOTAL; lev++) {
    for (int i=0; i < num_type_levels[lev]; i++) {
      if (strncmp (type_levels[lev][i],
		   p->getName(), strlen (type_levels[lev][i])) == 0) {
	return lev;
      }
    }
  }
  return -1;
}

int Act::getLevel (ActId *id)
{
  char buf[10240];
  int lev;
  
  if (!id) {
    return -1;
  }
  
  id->sPrint (buf, 10240);
  
  for (lev = 0; lev < ACT_MODEL_TOTAL; lev++) {
    for (int i=0; i < num_inst_levels[lev]; i++) {
      if (strncmp (inst_levels[lev][i],
		   buf, strlen (inst_levels[lev][i])) == 0) {
	return lev;
      }
    }
  }
  return -1;
}
