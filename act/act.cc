/*************************************************************************
 *
 *  Copyright (c) 2017-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <ctype.h>
#include <act/act.h>
#include "act_parse.h"
#include "act_walk_X.h"
#include "config.h"
#include "array.h"

#ifdef DEBUG_PERFORMANCE
#include "mytime.h"
#endif

struct command_line_defs {
  char *varname;
  int value;
};

int Act::max_recurse_depth;

L_A_DECL (struct command_line_defs, vars);

#define isid(x) (((x) == '_') || isalpha(x))

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

  if (initialize) return;
  initialize = 1;
  ActNamespace::Init();
  Type::Init();

  config_std_path ("act");
  config_set_default_int ("act.max_recurse_depth", 1000);
  config_read ("global.conf");
  Act::max_recurse_depth = config_get_int ("act.max_recurse_depth");

  A_INIT (vars);

  for (i=1; i < argc; i++) {
    if (strncmp (argv[i], "-D", 2) == 0) {
      char *tmp, *s;

      tmp = Strdup (argv[i]+2);
      /* tmp should look like var=true or var=false */
      s = tmp;
      while (*s && *s != '=') {
	s++;
      }

      if (*s != '=') {
	fatal_error ("-D option must be of the form -Dvar=true or -Dvar=false");
      }
      *s = '\0';
      s++;
      if (strcmp (s, "true") != 0 &&
	  strcmp (s, "false") != 0) {
	fatal_error ("-D option must be of the form -Dvar=true or -Dvar=false");
      }

      if (!_valid_id_string (tmp)) {
	fatal_error ("-D option: variable `%s' is not a valid identifier",
		     tmp);
      }

      A_NEW (vars, struct command_line_defs);
      A_NEXT (vars).varname = tmp;
      if (strcmp (s, "true") == 0) {
	A_NEXT (vars).value = 1;
      }
      else {
	A_NEXT (vars).value = 0;
      }
      A_INC (vars);
    }
    else if (strncmp (argv[i], "-T", 2) == 0) {
      config_stdtech_path (argv[i]+2);
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
  Act::Init (&argc, &argv);
  FREE (argv[0]);
  FREE (argv);

  aux = hash_new (2);

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

  tr.param_mode = 0;

  tr.u_p = NULL;
  tr.u_d = NULL;
  tr.u_c = NULL;
  tr.u_f = NULL;
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

#ifdef DEBUG_PERFORMANCE
  realtime_msec ();
#endif

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
    it = tr.tf->NewPBool();
    if (tr.global->CurScope()->Lookup (vars[i].varname)) {
      fatal_error ("Name `%s' defined through -D is a duplicate!", vars[i].varname);
    }
    tr.global->CurScope()->Add (vars[i].varname, it);
    b = new ActBody_Inst (it, vars[i].varname);
    NEW (e, Expr);
    e->type = (vars[i].value ? E_TRUE : E_FALSE);

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

  if (config_exists ("act.mangle_chars")) {
    mangle (config_get_string ("act.mangle_chars"));
  }
  ActNamespace::act = this;
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

  tr.param_mode = 0;

  tr.u_p = NULL;
  tr.u_d = NULL;
  tr.u_c = NULL;
  tr.u_f = NULL;
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

#ifdef DEBUG_PERFORMANCE
  realtime_msec ();
#endif

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
  
  UserDef *u;
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
		 

void Act::aux_add (const char *phase, void *data)
{
  hash_bucket_t *b;

  b = hash_lookup (aux, phase);
  if (b) {
    if (b->v) {
      warning ("Act::aux_append is replacing old data for `%s'", phase);
    }
    b->v = data;
  }
  else {
    b = hash_add (aux, phase);
    b->v = data;
  }
}

void *Act::aux_find (const char *phase)
{
  hash_bucket_t *b;

  b = hash_lookup (aux, phase);
  if (b) {
    return b->v;
  }
  else {
    return NULL;
  }
}


void Act::Print (FILE *fp)
{
  gns->Print (fp);
}
