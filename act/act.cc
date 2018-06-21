/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <act/act.h>
#include <act/act_parse.h>
#include <act/act_walk_X.h>
#include <act/types.h>

void Act::Init (int *iargc, char ***iargv)
{
  static int initialize = 0;
  int argc = *iargc;
  char **argv = *iargv;

  if (initialize) return;
  initialize = 1;
  ActNamespace::Init();
  Type::Init();
  
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

  tr.curns = ActNamespace::global;
  tr.os = new ActOpen ();
  tr.tf = new TypeFactory ();
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

  a = act_parse (s);
  act_walk_X (&tr, a);
  act_parse_free (a);

  gns = ActNamespace::global;
  tf = tr.tf;
}


void Act::Expand ()
{
  Assert (gns, "Expand() called without an object?");
  /* expand each namespace! */
  gns->Expand ();
}
