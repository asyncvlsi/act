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
#include <act/passes/cells.h>

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
  fprintf (stderr, "Usage: %s [act-options] [-c] [-prsim|-lvs] <file.act>\n", s);
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

static void print_attr_prefix (act_attr_t *attr, int force_after)
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
  if (!have_after && force_after) {
    after = 0;
    have_after = 1;
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
    if (ACT_SPEC_ISTIMING (spec)) {
      /* timing constraint: special representation */
      if (export_format == PRSIM_FMT) {
	Expr *e = (Expr *)spec->ids[3];
	int delay;
	if (e) {
	  Assert (e->type == E_INT, "What?");
	  delay = e->u.v;
	}
	Array *aref[3];
	InstType *it[3];

	Assert (spec->count == 4, "Hmm...");

	for (int i=0; i < 3; i++) {
	  it[i] = s->FullLookup (spec->ids[i], &aref[i]);
	  Assert (it[i], "Hmm...");
	}

	if (it[0]->arrayInfo () && (!aref[0] || !aref[0]->isDeref())) {
	  spec->ids[0]->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Timing directive: LHS cannot be an array!");
	}
	ActId *tl[2];
	Arraystep *as[2];

	for (int i=1; i < 3; i++) {
	  if (it[i]->arrayInfo() && (!aref[i] || !aref[i]->isDeref ())) {
	    if (aref[i]) {
	      as[i-1] = it[i]->arrayInfo()->stepper (aref[i]);
	    }
	    else {
	      as[i-1] = it[i]->arrayInfo()->stepper();
	    }
	    tl[i-1] = spec->ids[i]->Tail();
	  }
	  else {
	    tl[i-1] = NULL;
	    as[i-1] = NULL;
	  }
	}

	if (!as[0] && !as[1]) {
	  printf ("timing(");
	  prefix_id_print (s, spec->ids[0]);

#define PRINT_EXTRA(x)					\
	  do {						\
	    if (spec->extra[x] & 0x03) {		\
	      if ((spec->extra[x] & 0x03) == 1) {	\
		printf ("+");				\
	      }						\
	      else {					\
		printf ("-");				\
	      }						\
	    }						\
	  } while (0)

	  PRINT_EXTRA (0);
	  printf (",");
	  prefix_id_print (s, spec->ids[1]);
	  PRINT_EXTRA (1);
	  printf (",");
	  prefix_id_print (s, spec->ids[2]);
	  PRINT_EXTRA (2);
	  if (e) {
	    printf (",%d", delay);
	  }
	  printf (")\n");
	}
	else {
	  if (aref[1]) {
	    tl[0]->setArray (NULL);
	  }
	  if (aref[2]) {
	    tl[1]->setArray (NULL);
	  }
	  
	  while ((as[0] && !as[0]->isend()) ||
		 (as[1] && !as[1]->isend())) {
	    char *tmp[2];
	    for (int i=0; i < 2; i++) {
	      if (as[i]) {
		tmp[i] = as[i]->string();
	      }
	      else {
		tmp[i] = NULL;
	      }
	    }
	    
	    printf ("timing(");
	    prefix_id_print (s, spec->ids[0]);
	    PRINT_EXTRA (0);
	    printf (",");
	    if (tmp[0]) {
	      prefix_id_print (s, spec->ids[1], tmp[0]);
	      FREE (tmp[0]);
	    }
	    else {
	      prefix_id_print (s, spec->ids[1]);
	    }
	    PRINT_EXTRA (1);
	    printf (",");
	    if (tmp[1]) {
	      prefix_id_print (s, spec->ids[2], tmp[1]);
	      FREE (tmp[1]);
	    }
	    else {
	      prefix_id_print (s, spec->ids[2]);
	    }
	    PRINT_EXTRA (2);
	    if (e) {
	      printf (",%d", delay);
	    }
	    printf (")\n");

	    if (as[1]) {
	      as[1]->step();
	      if (as[1]->isend()) {
		if (as[0]) {
		  as[0]->step();
		  if (as[0]->isend()) {
		    /* we're done! */
		  }
		  else {
		    /* refresh as[1] */
		    delete as[1];
		    if (aref[2]) {
		      as[1] = it[2]->arrayInfo()->stepper (aref[2]);
		    }
		    else {
		      as[1] = it[2]->arrayInfo()->stepper();
		    }
		  }
		}
		else {
		  /* we're done */
		}
	      }
	      else {
		/* keep going! */
	      }
	    }
	    else if (as[0]) {
	      as[0]->step();
	    }
	  }
	  if (as[0]) {
	    delete as[0];
	  }
	  if (as[1]) {
	    delete as[1];
	  }
	  if (aref[1]) {
	    tl[0]->setArray (aref[1]);
	  }
	  if (aref[2]) {
	    tl[1]->setArray (aref[2]);
	  }
	}
      }
      spec = spec->next;
      continue;
    }
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
	  Array *aref;
	  id = spec->ids[i];
	  InstType *it = s->FullLookup (id, &aref);
	  /* spec->ids[i] might be a bool, or a bool array! fix bool
	     array case */
	  if (it->arrayInfo() && (!aref || !aref->isDeref())) {
	    Arraystep *astep;
	    Array *a = it->arrayInfo();

	    id = id->Tail();
	    
	    if (aref) {
	      astep = a->stepper (aref);
	      a = aref;
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
	if (!labels) {
	  labels = hash_new (4);
	}
	if (hash_lookup (labels, (char *)p->u.one.id)) {
	  fatal_error ("Duplicate label `%s'", (char *)p->u.one.id);
	}
	b = hash_add (labels, (char *)p->u.one.id);
	b->v = p;
      }
      else {
	print_attr_prefix (p->u.one.attr, 0);
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
	  print_attr_prefix (p->u.one.attr, 0);
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
	  print_attr_prefix (p->u.one.attr, 0);
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
      print_attr_prefix (p->u.p.attr, 1);
      if (p->u.p.g) {
	/* passn */
	prefix_id_print (s, p->u.p.g);
	printf (" & ~");
	prefix_id_print (s, p->u.p.s);
	printf (" -> ");
	prefix_id_print (s, p->u.p.d);
	printf ("-\n");
      }
      if (p->u.p._g) {
	printf ("~");
	prefix_id_print (s, p->u.p._g);
	printf (" & ");
	prefix_id_print (s, p->u.p.s);
	printf (" -> ");
	prefix_id_print (s, p->u.p.d);
	printf ("+\n");
      }
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
  if (labels) {
    hash_clear (labels);
  }
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
  int do_cells = 0;
  char *cells = NULL;

  Act::Init (&argc, &argv);
  
  export_format = PRSIM_FMT;

  if (argc < 2 || argc > 4) usage (argv[0]);

  int idx = 1;

  if (strncmp (argv[idx], "-c", 2) == 0) {
     do_cells = 1;
     if (argv[idx][2] == '\0') {
	cells = NULL;
     }
     else {
        cells = argv[idx]+2;
     }
     idx++;
  }
  if (idx >= argc) usage (argv[0]);
  if (strcmp (argv[idx], "-prsim") == 0) {
     export_format = PRSIM_FMT;
     idx++;
  }
  else if (strcmp (argv[idx], "-lvs") == 0) {
    export_format = LVS_FMT;
     idx++;
  }
  else {
    if (idx != argc-1) {
      usage (argv[0]);
    }
  }
  if (idx != argc-1) usage (argv[0]);
  file = argv[idx];

  a = new Act (file);
  if (cells) {
    a->Merge (cells);
  }
  a->Expand ();
  if (do_cells) {
     ActCellPass *cp = new ActCellPass (a);
     cp->run ();
  }

  ActApplyPass *ap = new ActApplyPass (a);

  gpass = ap;

  ap->setInstFn (aflat_body);
  ap->setConnPairFn (aflat_conns);
  ap->run();
  aflat_ns (a->Global());

  //aflat_prs (a, export_format);

  return 0;
}
