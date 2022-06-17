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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <act/act.h>
#include <act/tech.h>
#include "act_parse.h"
#include "act_walk_X.h"
#include <common/config.h>
#include <common/array.h>
#include <act/path.h>
#include "fexpr.h"

#ifdef DEBUG_PERFORMANCE
#include <common/mytime.h>
#endif

#include <common/log.h>

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
int Act::emit_depend;
char *Act::_getopt_string;

#define WARNING_FLAG(x,y) \
  int Act::x;
#include "warn.def"

Log *Act::L;
list_t *Act::cmdline_args;

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


/*-- returns 1 if matched and processed, 0 otherwise --*/
int Act::_process_act_arg (const char *argvp, int *tech_specified, char **conf)
{
  if (strncmp (argvp, "-D", 2) == 0) {
    char *tmp, *s, *t;
    int isint;
    int s_value;
    unsigned int u_value;

    tmp = Strdup (argvp+2);
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
  else if (strncmp (argvp, "-T", 2) == 0) {
    if (argvp[2] == '/') {
      /* absolute path specified */
      config_append_path (argvp+2);
      config_append_path (".");
    }
    else {
      config_stdtech_path (argvp+2);
      mysetenv ("ACT_TECH", Strdup (argvp+2));
    }
    *tech_specified = 1;
  }
  else if (strncmp (argvp, "-W", 2) == 0) {
    char *s, *tmp;
    s = Strdup (argvp+2);
    tmp = strtok (s, ",");
    while (tmp) {
#define WARNING_FLAG(x,y)						\
      if (strncmp (tmp, #x, sizeof (#x)-1) == 0) {			\
	if (strcmp (tmp + sizeof(#x)-1, ":on") == 0) {			\
	  config_set_int ("act.warn." #x, 1);				\
	}								\
	else if (strcmp (tmp + sizeof(#x)-1, ":off") == 0) {		\
	  config_set_int ("act.warn." #x, 0);				\
	}								\
	else {								\
	  fatal_error ("-W option `%s' must end in :off or :on", tmp);	\
	}								\
      } else
#include "warn.def"
      if (strcmp (tmp, "all:on") == 0) {
#define WARNING_FLAG(x,y) config_set_int ("act.warn." #x, 1);
#include "warn.def"
      } else if (strcmp (tmp, "all:off") == 0) {
#define WARNING_FLAG(x,y) config_set_int ("act.warn." #x, 0);
#include "warn.def"
      }
      else {
	fprintf (stderr, "FATAL: -W option `%s' is unknown", tmp);
	fprintf (stderr, "  legal options are:\n   ");
#define WARNING_FLAG(x,y) fprintf (stderr, " " #x);
#include "warn.def"
	fprintf (stderr, "\nAppend :on or :off to turn the flag on/off\n");
	exit (1);
      }
      tmp = strtok (NULL, ",");
    }
    FREE (s);
  }
  else if (strncmp (argvp, "-V", 2) == 0) {
    char *s, *tmp;
    s = Strdup (argvp+2);
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
  else if (strncmp (argvp, "-opt=", 5) == 0) {
    /* -- getopt string! -- */
    if (_getopt_string) {
      fatal_error ("-opt can only be used once!");
    }
    _getopt_string = Strdup (argvp + 5);
  }
  else if (strncmp (argvp, "-log=", 5) == 0) {
    char *s;
    s = Strdup (argvp+5);
    Log::OpenLog (s);
    FREE (s);
  }
  else if (strncmp (argvp, "-cnf=", 5) == 0) {
    if (*conf) {
      FREE (*conf);
    }
    *conf = Strdup (argvp+5);
  }
  else if (strncmp (argvp, "-ref=", 5) == 0) {
    /* refinement steps */
    int r = atoi(argvp+5);
    if (r < 0) {
      fatal_error ("-ref option needs a non-negative integer");
    }
    config_set_int ("act.refine_steps", r);
  }
  else if (strncmp (argvp, "-lev=", 5) == 0) {
    Log::UpdateLogLevel(argvp+5);
  }
  else {
    return 0;
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

  _getopt_string = NULL;

  if (initialize) return;
  initialize = 1;

  Assert (sizeof (void *) == sizeof (unsigned long), "Pointer hashing issues");

  Act::L = new Log("actcore", 'A');
  ActNamespace::Init();
  Type::Init();

  config_set_default_int ("act.max_recurse_depth", 1000);
  config_set_default_int ("act.max_loop_iterations", 1000);
  
#define WARNING_FLAG(x,y) \
  config_set_default_int ("act.warn." #x, y);

#include "warn.def"  
  
  Act::emit_depend = 0;
  Act::double_expand = 1;

  Log::OpenStderr ();
  Log::Initialize_LogLevel ("");

  A_INIT (vars);

  A_DECL (int, args_remain);
  A_INIT (args_remain);
  args_remain = NULL;

  A_NEW (args_remain, int);
  A_NEXT (args_remain) = 0;
  A_INC (args_remain);

  if (getenv ("ACT_STD_CMDLINE")) {
    char *tmp = Strdup (getenv ("ACT_STD_CMDLINE"));

    /* simple strtok */
    char *s, *nexts;
    char tch;

    nexts = tmp;

#define SEPCHAR(x) (((x) == ' ') || ((x) == '\t'))

    while (*nexts != '\0') {
      /*-- find split --*/
      s = nexts;
      while (*nexts != '\0' && !SEPCHAR (*nexts)) {
	nexts++;
      }
      tch = *nexts;
      *nexts = '\0';

      if (!_process_act_arg (s, &tech_specified, &conf_file)) {
	warning ("ACT_STD_CMDLINE `%s' is not an ACT argument; skipped",
		 s);
      }

      /* skip space/tab */
      *nexts = tch;
      while (*nexts != '\0' && SEPCHAR (*nexts)) {
	  nexts++;
      }
    }
    FREE (tmp);
  }
  

  for (i=1; i < argc; i++) {
    if (!_process_act_arg (argv[i], &tech_specified, &conf_file)) {
      A_NEW (args_remain, int);
      A_NEXT (args_remain) = i;
      A_INC (args_remain);
    }
  }
  /* arguments from position 1 ... 1 + (i-1) have been wacked */
  *iargc = A_LEN (args_remain);
  for (j=1; j < *iargc; j++) {
    argv[j] = argv[args_remain[j]];
  }
  argv[j] = NULL;
  A_FREE (args_remain);

  if (!tech_specified) {
    if (getenv ("ACT_TECH")) {
      config_stdtech_path (getenv ("ACT_TECH"));
    }
    else {
      config_stdtech_path ("generic");
      mysetenv ("ACT_TECH", Strdup ("generic"));
    }
  }

  Act::config_info ("global.conf");
  config_read ("global.conf");
  Act::config_info ("prs2net.conf");
  config_read ("prs2net.conf");
  if (conf_file) {
    config_read (conf_file);
    Act::config_info (conf_file);
  }

#define WARNING_FLAG(x,y) \
  Act::x = config_get_int ("act.warn." #x);
#include "warn.def"
  
  Act::max_recurse_depth = config_get_int ("act.max_recurse_depth");
  Act::max_loop_iterations = config_get_int ("act.max_loop_iterations");
  Act::cmdline_args = NULL;
  
  return;
}

void Act::setOptionString (char *s)
{
  if (_getopt_string) {
    FREE (_getopt_string);
  }
  _getopt_string = Strdup (s);
}

int Act::getOptions (int *iargc, char ***iargv)
{
  int ch;
  int opt_arg[256];
  char *argv0 = (*iargv)[0];
  int return_code = 1;
  
  /* -- if getopt options -- */
  if (!_getopt_string) return return_code;
    
  /* continue parsing! */
  for (int i=0; i < 256; i++) {
    opt_arg[i] = 0;
  }
  for (int i=0; _getopt_string[i]; i++) {
    if (_getopt_string[i] == 'W' || _getopt_string[i] == 'D' ||
	_getopt_string[i] == 'V' || _getopt_string[i] == 'T') {
      warning ("Option `%c' is already a core ACT option", _getopt_string[i]);
    }
    if (_getopt_string[i+1] && (_getopt_string[i+1] == ':')) {
      opt_arg[0xff & _getopt_string[i]] = 1;
      i++;
    }
  }

  Act::cmdline_args = list_new ();
  while ((ch = getopt (*iargc, *iargv, _getopt_string)) != -1) {
    if (ch == '?') {
      return_code = 0;
    }
    else {
      list_iappend (Act::cmdline_args, ch);
      if (opt_arg[ch]) {
	/* get arg */
	list_append (Act::cmdline_args, Strdup (optarg));
      }
      else {
	list_append (Act::cmdline_args, NULL);
      }
    }
  }
  *iargc -= (optind-1);
  *iargv += (optind-1);
  *iargv[0] = argv0;

  FREE (_getopt_string);
  _getopt_string = NULL;
  
  return return_code;
}

static void _init_tr (ActTree *tr, TypeFactory *tf, ActNamespace *G)
{
  tr->curns = G;
  tr->global = tr->curns;
  tr->os = new ActOpen ();
  tr->tf = tf;
  tr->scope = tr->curns->CurScope ();

  tr->u = NULL;

  tr->u_p = NULL;
  tr->u_d = NULL;
  tr->u_c = NULL;
  tr->u_f = NULL;
  tr->u_i = NULL;
  tr->t = NULL;

  tr->t_inst = NULL;

  tr->i_t = NULL;
  tr->i_id = NULL;
  tr->a_id = NULL;

  tr->supply.vdd = NULL;
  tr->supply.gnd = NULL;
  tr->supply.psc = NULL;
  tr->supply.nsc = NULL;
  
  tr->strict_checking = 0;
  tr->func_template = 0;

  tr->in_tree = 0;
  tr->in_subckt = 0;

  tr->attr_num = config_get_table_size ("act.instance_attr");
  tr->attr_table = config_get_table_string ("act.instance_attr");

  tr->sizing_info = NULL;
  tr->sz_loop_stack = list_new ();
  tr->skip_id_check = 0;

  tr->emit_depend = Act::emit_depend;
  tr->in_cond = 0;

  tr->ptype_expand = 0;
  tr->ptype_name = NULL;

  tr->um = NULL;

  tr->is_assignable_override = -1;
}

static void _free_tr (ActTree *tr)
{
  delete tr->os;
  list_free (tr->sz_loop_stack);
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

  _init_tr (&tr, tf, ActNamespace::global);

#ifdef DEBUG_PERFORMANCE
  realtime_msec ();
#endif
  expr_parse_basecase_bool = act_parse_expr_syn_loop_bool;
  expr_parse_basecase_num = act_parse_expr_intexpr_base;
  expr_parse_basecase_extra = act_expr_any_basecase;
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
    b = new ActBody_Inst (0, it, vars[i].varname);
    NEW (e, Expr);

    if (vars[i].isint == 0) {
      e->type = (vars[i].u_value ? E_TRUE : E_FALSE);
    }
    else if (vars[i].isint == 1) {
      e->type = E_INT;
      e->u.v = vars[i].u_value;
      e->u.v_extra = NULL;
    }
    else {
      e->type = E_INT;
      e->u.v = vars[i].s_value;
      e->u.v_extra = NULL;
    }

    Expr *tmp = TypeFactory::NewExpr (e);
    FREE (e);
    e = tmp;

    bc = new ActBody_Conn (0, new ActId (vars[i].varname),
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
  _free_tr (&tr);
}

void Act::Merge (const char *s)
{
  act_Token *a;
  ActTree tr;

  if (!s) {
    return;
  }

  _init_tr (&tr, tf, ActNamespace::global);

#ifdef DEBUG_PERFORMANCE
  realtime_msec ();
#endif

  expr_parse_basecase_bool = act_parse_expr_syn_loop_bool;
  expr_parse_basecase_num = act_parse_expr_intexpr_base;
  expr_parse_basecase_extra = act_expr_any_basecase;
  expr_parse_newtokens = act_expr_parse_newtokens;

  if (act_isimported (s)) {
    return;
  }

  a = act_parse (s);

#ifdef DEBUG_PERFORMANCE
  printf ("Parser time: %g\n", (realtime_msec()/1000.0));
#endif

  act_walk_X (&tr, a);
  
  act_parse_free (a);

#ifdef DEBUG_PERFORMANCE
  printf ("Walk and free time: %g\n", (realtime_msec()/1000.0));
#endif
  _free_tr (&tr);
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


void act_add_global_pint (const char *name, int val)
{
  int argc = 1;
  char **argv;

  MALLOC (argv, char *, 2);
  argv[0] = Strdup ("-internal-");
  argv[1] = NULL;

  Act::Init (&argc, &argv);
  FREE (argv[0]);
  FREE (argv);


  A_NEW (vars, struct command_line_defs);
  A_NEXT (vars).varname = Strdup (name);
  A_NEXT (vars).u_value = val;
  A_NEXT (vars).isint = 1;
  A_INC (vars);
}

void act_add_global_pbool (const char *name, int val)
{
  int argc = 1;
  char **argv;

  MALLOC (argv, char *, 2);
  argv[0] = Strdup ("-internal-");
  argv[1] = NULL;

  Act::Init (&argc, &argv);
  FREE (argv[0]);
  FREE (argv);

  A_NEW (vars, struct command_line_defs);
  A_NEXT (vars).varname = Strdup (name);
  A_NEXT (vars).u_value = val ? 1 : 0;
  A_NEXT (vars).isint = 0;
  A_INC (vars);
}


list_t *Act::getDecomp (Process *p)
{
  static ActPass *_mp = NULL;
  static ActPass *_ap = NULL;

  if (!_mp) {
    _mp = pass_find ("chpmem");
  }
  if (!_ap) {
    _ap = pass_find ("chparb");
  }

  void *l1, *l2;
  l1 = NULL;
  l2 = NULL;

  if (_mp && _mp->completed()) {
    l1 = _mp->getMap (p);
  }
  if (_ap && _ap->completed()) {
    l2 = _ap->getMap (p);
  }

  if (l1 || l2) {
    list_t *ret = list_new ();
    if (l1) {
      list_append (ret, l1);
    }
    if (l2) {
      list_append (ret, l2);
    }
    return ret;
  }
  return NULL;
}


list_t *Act::getDecompTypes ()
{
  static ActPass *_mp = NULL;
  static ActPass *_ap = NULL;

  if (!_mp) {
    _mp = pass_find ("chpmem");
  }
  if (!_ap) {
    _ap = pass_find ("chparb");
  }

  list_t *l1, *l2;
  l1 = NULL;
  l2 = NULL;

  if (_mp && _mp->completed()) {
    l1 = (list_t *) _mp->getGlobalInfo ();
  }
  if (_ap && _ap->completed()) {
    l2 = (list_t *) _ap->getGlobalInfo ();
  }

  list_t *ret = NULL;

  if (l1) {
    ret = list_dup (l1);
  }
  if (l2) {
    if (!ret) {
      ret = list_dup (l2);
    }
    else {
      list_concat (ret, list_dup (l2));
    }
  }
  return ret;
}



bool Act::LocalizeGlobal (const char *s)
{
  if (gns->isExpanded()) {
    warning ("Act::LocalizeGlobal() called on an expanded design.");
    return false;
  }

  /* 
     Step 1: Check that "s" is in fact a global signal
  */
  InstType *it;
  if (!(it = gns->findInstance (s))) {
    warning ("Act::LocalizeGlobal(): `%s' is not a global.", s);
    return false;
  }

  /* apply first phase substitutions */
  list_t *defs_subst = list_new ();
  gns->_subst_globals (defs_subst, it, s);

  if (!list_isempty (defs_subst)) {
    /* We have a list of process definitions where we have 
       substituted the global signal "s".
       
       Proceed via an iterative algorithm across *ALL* process
       definitions in all namespaces:
       
       * whenever an instance of a type in the defs subst list is found,
       we add a connection. If it is an array instance, we need to add
       an ActBody_Loop that contains the connection; and a nested set
       for multi-dimensional arrays.
       
       * when we do this update, we must CHECK to see if the current
       process is in the def list; if it is, nothing has to be done

       * if the process is NOT in the defs list, then
          - if it has a port with the same name as the global, we need
	    to pick a fresh name; try "globalN"
	  - add the fresh port
	  - add the process to the defs list
	  - add to fresh defs list
       
       * we may have a bunch of new defs we need to handle.
         - walk through 
    */
    listitem_t *li = list_first (defs_subst);
    do {
      listitem_t *tl = list_tail (defs_subst);
      /* In this round, we analyze li .. tl */
      
      gns->_subst_globals_addconn (defs_subst /* all the defs with
						 extra ports */,
				   li, /* the ones processed in this
					  round */
				   it, s);

      /* set li to the next item after the tail */
      li = list_next (tl);
    } while (li);
    



  }
  return true;
}
