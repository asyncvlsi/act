/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <act/passes/aflat.h>

static enum output_formats {
  PRSIM_FMT,
  LVS_FMT
} export_format;

#define ARRAY_STYLE (export_format == LVS_FMT ? 1 : 0)
#define EXTRA_ARGS  NULL, (export_format == LVS_FMT ? 1 : 0)

/* hash table for labels */
static struct Hashtable *labels;

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-prsim|-lvs] <file.act>\n", s);
  exit (1);
}


static void print_connect()
{
  if (export_format == LVS_FMT) {
    printf ("connect ");
  }
  else {
    printf ("= ");
  }
}

#define ARRAY_STYLE (export_format == LVS_FMT ? 1 : 0)
#define EXTRA_ARGS  NULL, (export_format == LVS_FMT ? 1 : 0)

static ActId *current_prefix = NULL;

static void prefix_id_print (Scope *s, ActId *id, const char *str = "")
{
  printf ("\"");
  if (s->Lookup (id, 0)) {
    if (current_prefix) {
      if (id->getName()[0] != ':') {
	current_prefix->Print (stdout);
	printf (".");
      }
    }
  }
  else {
    ValueIdx *vx;
    /* must be a global */
    vx = s->FullLookupVal (id->getName());
    Assert (vx, "Hmm.");
    Assert (vx->global, "Hmm");
    if (vx->global == ActNamespace::Global()) {
      /* nothing to do */
    }
    else {
      char *tmp = vx->global->Name ();
      printf ("%s::", tmp);
      FREE (tmp);
    }
  }
  id->Print (stdout, EXTRA_ARGS);
  printf ("%s\"", str);
}


#define PREC_BEGIN(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      printf ("(");				\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      printf (")");				\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    _print_prs_expr (s, e->u.e.l, (myprec), flip);	\
    printf ("%s", (sym));			\
    _print_prs_expr (s, e->u.e.r, (myprec), flip);	\
    PREC_END (myprec);				\
  } while (0)

#define EMIT_UNOP(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    printf ("%s", sym);				\
    _print_prs_expr (s, e->u.e.l, (myprec), flip);	\
    PREC_END (myprec);				\
  } while (0)

/*
  3 = ~
  2 = &
  1 = |
*/
static void _print_prs_expr (Scope *s, act_prs_expr_t *e, int prec, int flip)
{
  hash_bucket_t *b;
  act_prs_lang_t *pl;
  
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    EMIT_BIN(2, "&");
    break;
    
  case ACT_PRS_EXPR_OR:
    EMIT_BIN(1, "|");
    break;
    
  case ACT_PRS_EXPR_VAR:
    if (flip) {
      printf ("~");
    }
    prefix_id_print (s, e->u.v.id);
    break;
    
  case ACT_PRS_EXPR_NOT:
    EMIT_UNOP(3, "~");
    break;
    
  case ACT_PRS_EXPR_LABEL:
    if (!labels) {
      fatal_error ("No labels defined!");
    }
    b = hash_lookup (labels, e->u.l.label);
    if (!b) {
      fatal_error ("Missing label `%s'", e->u.l.label);
    }
    pl = (act_prs_lang_t *) b->v;
    if (pl->u.one.dir == 0) {
      printf ("~");
    }
    printf ("(");
    _print_prs_expr (s, pl->u.one.e, 0, flip);
    printf (")");
    break;

  case ACT_PRS_EXPR_TRUE:
    printf ("true");
    break;
    
  case ACT_PRS_EXPR_FALSE:
    printf ("false");
    break;

  default:
    fatal_error ("What?");
    break;
  }
}

static void print_attr_prefix (act_attr_t *attr)
{
  /* examine attributes:
     after
     weak
     unstab
  */
  int weak = 0;
  int unstab = 0;
  int after = 0;
  int have_after = 0;
  act_attr_t *x;

  if (export_format == LVS_FMT) return;

  for (x = attr; x; x = x->next) {
    if (strcmp (x->attr, "weak") == 0) {
      weak = 1;
    }
    else if (strcmp (x->attr, "unstab") == 0) {
      unstab = 1;
    }
    else if (strcmp (x->attr, "after") == 0) {
      after = x->e->u.v;
      have_after = 1;
    }
  }
  if (weak) {
    printf ("weak ");
  }
  if (unstab) {
    printf ("unstab ");
  }
  if (have_after) {
    printf ("after %d ", after);
  }
}

static void aflat_print_spec (Scope *s, act_spec *spec)
{
  const char *tmp;
  ActId *id;
  while (spec) {
    tmp = act_spec_string (spec->type);
    Assert (tmp, "Hmm");
    if ((export_format == PRSIM_FMT &&
	 (strcmp (tmp, "mk_exclhi") == 0 || strcmp (tmp, "mk_excllo") == 0)) ||
	(export_format == LVS_FMT &&
	 (strcmp (tmp, "exclhi") == 0 || strcmp (tmp, "excllo") == 0))) {
      if (spec->count > 0) {
	int comma = 0;
	printf ("%s(", tmp);
	for (int i=0; i < spec->count; i++) {
	  id = spec->ids[i];
	  ValueIdx *vx = s->FullLookupVal (id->getName());
	  InstType *it = vx->t;
	  Assert (vx, "What");
	  while (id->Rest()) {
	    UserDef *u = dynamic_cast<UserDef *> (it->BaseType ());
	    Assert (u, "Hm");
	    id = id->Rest();
	    it = u->Lookup (id);
	    Assert (it, "Hmm");
	  }
	  /* spec->ids[i] might be a bool, or a bool array! fix bool
	     array case */
	  if (it->arrayInfo() &&
	      (!id->arrayInfo() || !id->arrayInfo()->isDeref())) {
	    Arraystep *astep;
	    Array *a = it->arrayInfo();
	    if (id->arrayInfo()) {
	      astep = a->stepper (id->arrayInfo());
	      a = id->arrayInfo();
	    }
	    else {
	      astep = a->stepper();
	      a = NULL;
	    }
	    int restore = 0;
	    if (id->arrayInfo()) {
	      id->setArray (NULL);
	      restore = 1;
	    }
	    while (!astep->isend()) {
	      char *tmp = astep->string();
	      if (comma != 0) {
		printf (",");
	      }
	      prefix_id_print (s, spec->ids[i], tmp);
	      comma = 1;
	      FREE (tmp);
	      astep->step();
	    }
	    delete astep;
	    if (restore) {
	      id->setArray (a);
	    }
	  }
	  else {
	    if (comma != 0) {
	      printf (",");
	    }
	    prefix_id_print (s, spec->ids[i]);
	    comma = 1;
	  }
	}
	printf (")\n");
      }
    }
    spec = spec->next;
  }
}

static void aflat_print_prs (Scope *s, act_prs_lang_t *p)
{
  if (!p) return;
  
  while (p) {
    switch (p->type) {
    case ACT_PRS_RULE:
      /* attributes */
      if (p->u.one.label) {
	hash_bucket_t *b;
	if (hash_lookup (labels, (char *)p->u.one.id)) {
	  fatal_error ("Duplicate label `%s'", (char *)p->u.one.id);
	}
	b = hash_add (labels, (char *)p->u.one.id);
	b->v = p;
      }
      else {
	print_attr_prefix (p->u.one.attr);
	_print_prs_expr (s, p->u.one.e, 0, 0);
	printf ("->");
	prefix_id_print (s, p->u.one.id);
	if (p->u.one.dir) {
	  printf ("+\n");
	}
	else {
	  printf ("-\n");
	}
	if (p->u.one.arrow_type == 1) {
	  print_attr_prefix (p->u.one.attr);
	  printf ("~(");
	  _print_prs_expr (s, p->u.one.e, 0, 0);
	  printf (")");
	  printf ("->");
	  prefix_id_print (s, p->u.one.id);
	  if (p->u.one.dir) {
	    printf ("-\n");
	  }
	  else {
	    printf ("+\n");
	  }
	}
	else if (p->u.one.arrow_type == 2) {
	  print_attr_prefix (p->u.one.attr);
	  _print_prs_expr (s, p->u.one.e, 0, 1);
	  printf ("->");
	  prefix_id_print (s, p->u.one.id);
	  if (p->u.one.dir) {
	    printf ("-\n");
	  }
	  else {
	    printf ("+\n");
	  }
	}
	else if (p->u.one.arrow_type != 0) {
	  fatal_error ("Eh?");
	}
      }
      break;
    case ACT_PRS_GATE:
      /* not printing pass fets */
      break;
    case ACT_PRS_TREE:
      /* this is fine */
      aflat_print_prs (s, p->u.l.p);
      break;
    case ACT_PRS_SUBCKT:
      /* this is fine */
      aflat_print_prs (s, p->u.l.p);
      break;
    default:
      fatal_error ("loops should have been expanded by now!");
      break;
    }
    p = p->next;
  }
}

static void aflat_dump (Scope *s, act_prs *prs, act_spec *spec)
{
  while (prs) {
    aflat_print_prs (s, prs->p);
    prs = prs->next;
  }
  aflat_print_spec (s, spec);
}

static void aflat_ns (ActNamespace *ns)
{
  aflat_dump (ns->CurScope(), ns->getprs(), ns->getspec());
}
		     
void aflat_body (void *cookie, ActId *prefix, Process *p)
{
  Assert (p->isExpanded(), "What?");
  current_prefix = prefix;
  aflat_dump (p->CurScope(), p->getprs(), p->getspec());
  current_prefix = NULL;
}

ActApplyPass *gpass;

void aflat_conns (void *cookie, ActId *id1, ActId *id2)
{
  print_connect ();

  printf ("\"");
  gpass->printns (stdout);
  id1->Print (stdout);
  printf ("\" \"");
  gpass->printns (stdout);
  id2->Print (stdout);
  printf ("\"\n");
}


int main (int argc, char **argv)
{
  Act *a;
  char *file;

  Act::Init (&argc, &argv);
  
  export_format = PRSIM_FMT;

  if (argc > 3) usage(argv[0]);
  if (argc == 3) {
    if (strcmp (argv[1], "-prsim") == 0) {
      export_format = PRSIM_FMT;
    }
    else if (strcmp (argv[1], "-lvs") == 0) {
      export_format = LVS_FMT;
    }
    else {
      usage (argv[0]);
    }
    file = argv[2];
  }
  else if (argc == 2) {
    file = argv[1];
  }
  else {
    usage (argv[0]);
  }

  a = new Act (file);
  a->Expand ();

  ActApplyPass *ap = new ActApplyPass (a);

  gpass = ap;

  ap->setInstFn (aflat_body);
  ap->setConnPairFn (aflat_conns);
  ap->run();
  aflat_ns (a->Global());

  //aflat_prs (a, export_format);

  return 0;
}
