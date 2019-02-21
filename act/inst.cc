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
#include <act/types.h>
#include <act/inst.h>

Expr *const_expr (int val);

InstType::InstType (Scope *_s, Type *_t, int is_temp)
{
  expanded = 0;
  t = _t;
  nt = 0;
  a = NULL;
  dir = Type::NONE;
  u = NULL;
  s = _s;
  temp_type = (is_temp ? 1 : 0);
}

/*
  Clone an InstType
*/
InstType::InstType (InstType *i, int skip_array)
{
  t = i->t;
  dir = i->dir;
  s = i->s;
  expanded = i->expanded;
  if (!skip_array) {
    Assert (i->a == NULL, "Replication in the presence of arrays?");
  }
  a = NULL;
  nt = 0;
  if (i->nt > 0) {
    nt = i->nt;
    MALLOC (u, inst_param, i->nt);
    /* clone this too */
    for (int k = 0; k < nt; k++) {
      u[k].isatype = i->u[k].isatype;
      if (i->u[k].isatype) {
	if (i->u[k].u.tt) {
	  u[k].u.tt = new InstType (i->u[k].u.tt);
	}
	else {
	  u[k].u.tt = NULL;
	}
      }
      else {
	if (i->u[k].u.tp) {
	  u[k].u.tp = i->u[k].u.tp->Clone ();
	}
	else {
	  u[k].u.tp = NULL;
	}
      }
    }
  }
  temp_type = 1;
}


/*
  @return 1 if the kth parameter is a type, 0 if it is an AExpr 
*/
int InstType::isParamAType (int k)
{
  Assert (0 <= k && k < nt, "Hmm");
  if (TypeFactory::isExactChanType (t) || TypeFactory::isPTypeType (t)) {
    /* all are type parameters */
    return 1;
  }
  if (TypeFactory::isIntType (t)) {
    /* all are AExpr parameters */
    return 0;
  }
  UserDef *u;
  InstType *x;
  u = dynamic_cast<UserDef *> (t);
  Assert (u, "What on earth");
  x = u->getPortType (-(k+1));
  Assert (x, "Huh");
  if (TypeFactory::isPTypeType (x->BaseType ())) {
    return 1;
  }
  else {
    return 0;
  }
}


/*
  Return 1 if the type is expanded.
  simple param types (except ptypes) are always expanded
*/
int InstType::isExpanded()
{
  if (expanded) {
    return 1;
  }
  if (TypeFactory::isParamType (this) && !this->a) {
    if (!TypeFactory::isPTypeType (BaseType())) {
      return 1;
    }
    return 0;
  }
  else {
    return 0;
  }
}

InstType::~InstType ()
{
  int i;

  if (temp_type == 0) return; /* cached */

  /*
  printf ("free'ing [%x]: ", this);
  Print (stdout);
  printf ("\n");
  fflush (stdout);
  */

  if (nt > 0) {
    for (i=0; i < nt; i++) {
      if (u[i].isatype) {
	delete u[i].u.tt;
      }
      else {
	delete u[i].u.tp;
      }
    }
    FREE (u);
  }
  delete a;
}


void InstType::MkArray (Array *_a)
{
  Assert (a == NULL, "Array of an array?");
  a = _a;
}


/*------------------------------------------------------------------------
 * Compare two instance types
 *
 * @param weak : this parameter only matters for array types. All
 *               cases check that the # of dimensions match.
 *
 *               0 = check that the arrays are in fact equal and have
 *               the same index ranges (and range expressions).
 *               Used for checking that port parameters are the same
 *               in the definition and declaration.
 *
 *               1 = checks that arrays are compatible in terms of #
 *               of dimensions, rather than equal.
 *               Used to sanity check connections prior to expanding
 *               parameters. 
 *
 *               2 = check that array ranges have the same size; they
 *               could have different index ranges.
 *               Used to check that two types can be connected.
 *
 *               3 = check that array ranges have the same size,
 *               except for the first dimension.
 *               Used to check that two types can be concatenated in
 *               an array expression.
 * 
 *
 *------------------------------------------------------------------------
 */
int InstType::isEqual (InstType *it, int weak)
{
  int valcheck;

  valcheck = isExpanded();
  if (weak == 0) valcheck = 1;

#if 0
  printf ("valcheck = %d\n", valcheck);
#endif

  if (t != it->t) return 0;   /* same base type */

  if (nt != it->nt) return 0; /* same number of template params, if
				 any */

  /* check that the template parameters of the type are the same */
  for (int i=0; i < nt; i++) {
    if (u[i].isatype != it->u[i].isatype) return 0;
    if (u[i].isatype) {
      if ((u[i].u.tt && !it->u[i].u.tt) ||
	  (!u[i].u.tt && it->u[i].u.tt)) return 0;
      if (u[i].u.tt && it->u[i].u.tt) {
	if (!u[i].u.tt->isEqual (it->u[i].u.tt)) return 0;
      }
    }
    else {
      AExpr *xconstexpr;
      if (!u[i].u.tp || !it->u[i].u.tp) {
	xconstexpr = new AExpr (const_expr (32));
      }
      else {
	xconstexpr = NULL;
      }
      /* being NULL is the same as const 32 */
      if (u[i].u.tp && !it->u[i].u.tp) {
	if (valcheck && (!u[i].u.tp->isEqual (xconstexpr))) return 0;
	delete xconstexpr;
      }
      else if (it->u[i].u.tp && !u[i].u.tp) {
	if (valcheck && (!xconstexpr->isEqual (it->u[i].u.tp))) return 0;
	delete xconstexpr;
      }
      else if (u[i].u.tp && it->u[i].u.tp) {
	if (valcheck && (!u[i].u.tp->isEqual (it->u[i].u.tp))) return 0;
      }
      else {
	delete xconstexpr;
      }
    }
  }

  if ((a && !it->a) || (!a && it->a)) return 0; /* both are either
						   arrays or not
						   arrays */


  /* dimensions must be compatible no matter what */
  if (a && !a->isDimCompatible (it->a)) return 0;

  if (!a || (weak == 1)) return 1; /* we're done */

#if 0
  printf ("checking arrays [weak=%d]\n", weak);
#endif

  if (weak == 0) {
    if (!a->isEqual (it->a, 1)) return 0;
  }
  else if (weak == 2) {
    if (!a->isEqual (it->a, 0)) return 0;
  }
  else if (weak == 3) {
    if (!a->isEqual (it->a, -1)) return 0;
  }
  return 1;
}

/* XXX: FIXME */
Type *InstType::isConnectable (InstType *it, int weak)
{
  int valcheck;
  Type *retval;

  valcheck = isExpanded();
  if (weak == 0) valcheck = 1;

#if 0
  printf ("valcheck = %d\n", valcheck);
#endif

  /* XXX: have to fix this. Even if the base types are not the same,
     they might be connectable because they have a common root */
  if (t != it->t) {
    if (TypeFactory::isUserType (t) ||
	TypeFactory::isUserType (it->t)) {
      Type *t1, *t2;
      t1 = t;
      t2 = it->t;
      if (!TypeFactory::isUserType (t1)) {
	t1 = it->t;
	t2 = t;
      }
      UserDef *u = dynamic_cast<UserDef *>(t1);
      InstType *pit = u->getParent();
      while (pit) {
	if (pit->t == t2)
	  break;
	if (TypeFactory::isUserType (pit)) {
	  u = dynamic_cast<UserDef *>(pit->t);
	  pit = u->getParent();
	}
	else {
	  pit = NULL;
	}
      }
      if (pit) {
	retval = t1;
      }
      else {
	if (TypeFactory::isUserType (t2)) {
	  u = dynamic_cast<UserDef *>(t2);
	  pit = u->getParent();
	  while (pit) {
	    if (pit->t == t1)
	      break;
	    if (TypeFactory::isUserType (pit)) {
	      u = dynamic_cast<UserDef *>(pit->t);
	      pit = u->getParent();
	    }
	    else {
	      pit = NULL;
	    }
	  }
	  if (!pit) {
	    return NULL;
	  }
	}
	else {
	  return NULL;
	}
	retval = t2;
      }
    }
    else {
      return NULL; /* not the same base type */
    }
  }
  else {
    retval = t;
  }

  Assert (retval, "HMM!");

  if (nt != it->nt) return NULL; /* same number of template params, if
				 any */

  /* check that the template parameters of the type are the same */
  for (int i=0; i < nt; i++) {
    if (u[i].isatype != it->u[i].isatype) return NULL;
    if (u[i].isatype) {
      if ((u[i].u.tt && !it->u[i].u.tt) ||
	  (!u[i].u.tt && it->u[i].u.tt)) return NULL;
      if (u[i].u.tt && it->u[i].u.tt) {
	if (!u[i].u.tt->isEqual (it->u[i].u.tt)) return NULL;
      }
    }
    else {
      AExpr *xconstexpr;
      if (!u[i].u.tp || !it->u[i].u.tp) {
	xconstexpr = new AExpr (const_expr (32));
      }
      else {
	xconstexpr = NULL;
      }
      /* being NULL is the same as const 32 */
      if (u[i].u.tp && !it->u[i].u.tp) {
	if (valcheck && (!u[i].u.tp->isEqual (xconstexpr))) return NULL;
	delete xconstexpr;
      }
      else if (it->u[i].u.tp && !u[i].u.tp) {
	if (valcheck && (!xconstexpr->isEqual (it->u[i].u.tp))) return NULL;
	delete xconstexpr;
      }
      else if (u[i].u.tp && it->u[i].u.tp) {
	if (valcheck && (!u[i].u.tp->isEqual (it->u[i].u.tp))) return NULL;
      }
      else {
	delete xconstexpr;
      }
    }
  }


  if ((a && !it->a) || (!a && it->a)) return NULL; /* both are either
						   arrays or not
						   arrays */


  /* dimensions must be compatible no matter what */
  if (a && !a->isDimCompatible (it->a)) return NULL;

  if (!a || (weak == 1)) return retval; /* we're done */

#if 0
  printf ("checking arrays [weak=%d]\n", weak);
#endif

  if (weak == 0) {
    if (!a->isEqual (it->a, 1)) return NULL;
  }
  else if (weak == 2) {
    if (!a->isEqual (it->a, 0)) return NULL;
  }
  else if (weak == 3) {
    if (!a->isEqual (it->a, -1)) return NULL;
  }
  return retval;
}



void InstType::Print (FILE *fp)
{
  char buf[10240];

  sPrint (buf, 10240);
  fprintf (fp, "%s", buf);
  return;
}


void InstType::sPrint (char *buf, int sz)
{
  UserDef *ud;
  InstType *x;
  int k = 0;
  int l;

#define PRINT_STEP				\
  do {						\
    l = strlen (buf+k);				\
    k += l;					\
    sz -= l;					\
    if (sz <= 0) return;			\
  } while (0)
    
#if 0
  /* view cached flag */
  if (temp_type == 0) {
    fprintf (fp, "c:");
  }
#endif
  snprintf (buf+k, sz, "%s", t->getName());
  PRINT_STEP;
  
  if (nt > 0) {
    /* templates are used for int, chan, ptype, and userdef */
    snprintf (buf+k, sz, "<");
    PRINT_STEP;
    
    for (int i=0; i < nt; i++) {
      if (u[i].isatype) {
	if (u[i].u.tt) {
	  u[i].u.tt->sPrint (buf+k, sz);
	}
	PRINT_STEP;
      }
      else {
	if (u[i].u.tp) {
	  u[i].u.tp->sPrint (buf+k, sz);
	}
	PRINT_STEP;
      }
      if (i < nt-1) {
	snprintf (buf+k, sz, ",");
	PRINT_STEP;
      }
    }
    snprintf (buf+k, sz, ">");
    PRINT_STEP;
  }
  switch (dir) {
  case Type::NONE:
    break;
  case Type::IN:
    snprintf (buf+k, sz, "?");
    PRINT_STEP;
    break;
  case Type::OUT:
    snprintf (buf+k, sz, "!");
    PRINT_STEP;
    break;
  case Type::INOUT:
    snprintf (buf+k, sz, "?!");
    PRINT_STEP;
    break;
  case Type::OUTIN:
    snprintf (buf+k, sz, "!?");
    PRINT_STEP;
    break;
  }
  if (a) {
    a->sPrint (buf+k, sz);
    PRINT_STEP;
  }
}



/*------------------------------------------------------------------------
 *  Used to print the name of the instance type into a buffer
 *   buf: buffer
 *    sz: size of buffer
 *     t: Type to be printed
 *  u,nt: template parameters
 *   dir: direction
 *------------------------------------------------------------------------
 */

static void sPrintTypeName (char *buf, int sz, Type *t, 
			    int nt, inst_param *u,
			    Type::direction dir)
{
  UserDef *ud;
  InstType *x;
  int k = 0;
  int l;

#define PRINT_STEP				\
  do {						\
    l = strlen (buf+k);				\
    k += l;					\
    sz -= l;					\
    if (sz <= 0) return;			\
  } while (0)
    
  snprintf (buf+k, sz, "%s", t->getName());
  PRINT_STEP;
  switch (dir) {
  case Type::NONE:
    break;
  case Type::IN:
    snprintf (buf+k, sz, "?");
    PRINT_STEP;
    break;
  case Type::OUT:
    snprintf (buf+k, sz, "!");
    PRINT_STEP;
    break;
  case Type::INOUT:
    snprintf (buf+k, sz, "?!");
    PRINT_STEP;
    break;
  case Type::OUTIN:
    snprintf (buf+k, sz, "!?");
    PRINT_STEP;
    break;
  }
  
  if (nt > 0) {
    /* templates are used for int, chan, ptype, and userdef */
    snprintf (buf+k, sz, "<");
    PRINT_STEP;
    
    for (int i=0; i < nt; i++) {
      if (u[i].isatype) {
	if (u[i].u.tt) {
	  u[i].u.tt->sPrint (buf+k, sz);
	}
	PRINT_STEP;
      }
      else {
	if (u[i].u.tp) {
	  u[i].u.tp->sPrint (buf+k, sz);
	}
	PRINT_STEP;
      }
      if (i < nt-1) {
	snprintf (buf+k, sz, ",");
	PRINT_STEP;
      }
    }
    snprintf (buf+k, sz, ">");
    PRINT_STEP;
  }
}


/*------------------------------------------------------------------------
 *
 *  Expand instance type!
 *
 *------------------------------------------------------------------------
 */
InstType *InstType::Expand (ActNamespace *ns, Scope *s)
{
  InstType *xit = NULL;
  Type *xt = NULL;

  if (expanded) {
    return this;
  }

  /* copy in my scope and parent base type. But it must be an expanded
     scope.
     
     NOTE:: An expanded instance type has a NULL scope for variable
     evaluation, since there aren't any variables to evaluate!
  */
  Assert (t, "Missing parent type?");

  char *tmp;
  MALLOC (tmp, char, 1024);
  sPrint (tmp, 1024);
  act_error_push (tmp, NULL, 0);

  /* expand template parameters, and then expand the type */
  inst_param *xu;
  int i;

  /* parameter expansion should be done in the scope (ns,s) where this instance
     was created */
  xu = NULL;
  if (nt > 0) {
    MALLOC (xu, inst_param, nt);
    for (i=0; i < nt; i++) {
      xu[i].isatype = u[i].isatype;
      if (u[i].isatype) {
	if (u[i].u.tt) {
	  xu[i].u.tt = u[i].u.tt->Expand (ns, s);
	}
	else {
	  xu[i].u.tt = NULL;
	}
      }
      else {
	if (!u[i].u.tp) {
	  /* only for int<> */
	  xu[i].u.tp = new AExpr (const_expr (32));
	}
	else {
	  xu[i].u.tp = u[i].u.tp->Expand (ns, s);
	}
      }
    }
  }
  
  /* now change the tmp name! */
  sPrintTypeName (tmp, 10240, this->BaseType(), nt, xu, dir);

  /* Expand the core type using template parameters, if any */

  /*-- the body of the type should be expanded using the namespace and
    scope in which the type was *DEFINED* --*/
  Scope *c_s;
  ActNamespace *c_ns;
  UserDef *u;
  u = dynamic_cast<UserDef *>(t);
  if (u) {
    c_ns = u->getns();
    c_s = c_ns->CurScope();
  }
  else {
    c_ns = ns;
    c_s = s;
  }

  xt = t->Expand (c_ns, c_s, nt, xu);

#if 0
  printf ("[%x] Name: %s\n", t, t->getName());
  printf ("[%x] Expanded: %s\n", xt, xt->getName());
#endif  
  
  /* If parent is user-defined, we need to make sure we have the
     expanded version of this in place!
  */
  xit = new InstType (c_s, xt, 0);
  xit->expanded = 1;
  xit->MkCached();

  /* array derefs */
  if (a) {
    xit->MkArray (a->Expand (ns, s));
  }

#if 0
  fprintf (stderr, "expand: ");
  this->Print (stderr);
  fprintf (stderr, " -> ");
  xit->Print (stderr);
  fprintf (stderr, "\n");
#endif
  xit->dir = dir;
  
  FREE (tmp);
  act_error_pop ();

  return xit;
}




/*------------------------------------------------------------------------
 *
 *   InstType::setNumParams --
 *
 *   Create slots for the template parameters for an instance type
 *
 *------------------------------------------------------------------------
 */
void InstType::setNumParams (int n)
{
  Assert (nt == 0, "Modifying the number of template parameters specified in an instance type??");
  nt = n;
  MALLOC (u, inst_param, nt);
  for (int k=0; k < nt; k++) {
    u[k].isatype = 0;
    u[k].u.tp = NULL;
  }
}

/*------------------------------------------------------------------------
 *
 *  InstType::setParam -- 
 *    set parameter for slot "pn" to value "a".
 *    Also sets the type of the parameter depending on which method
 *    was used.
 *
 *------------------------------------------------------------------------
 */
void InstType::setParam (int pn, AExpr *a)
{
  Assert (pn < nt && pn >= 0, "setParam() called with an invalid value");
  Assert (u[pn].u.tp == NULL, "setParam() changing an existing parameter!");
  /*printf ("setting param %d to %x\n", pn, a);*/
  u[pn].isatype = 0;
  u[pn].u.tp = a;
}

void InstType::setParam (int pn, Expr *a)
{
  Assert (pn < nt && pn >= 0, "setParam() called with an invalid value");
  Assert (u[pn].u.tp == NULL, "setParam() changing an existing parameter!");
  if (a) {
    u[pn].isatype = 0;
    u[pn].u.tp = new AExpr (a);
  }
}

void InstType::setParam (int pn, InstType *t)
{
  Assert (pn < nt && pn >= 0, "setParam() called with an invalid value");
  Assert (u[pn].u.tt == NULL, "setParam() changing an existing parameter!");
  u[pn].isatype = 1;
  u[pn].u.tt = t;
}


InstType *InstType::getTypeParam (int pn)
{
  Assert (u[pn].isatype, "getTypeParam() called for non-type!");
  return u[pn].u.tt;
}


AExpr *InstType::getAExprParam (int pn)
{
  Assert (!u[pn].isatype, "getAExprParam() called for non-AExpr!");
  return u[pn].u.tp;
}


void InstType::appendParams (int na, inst_param *a)
{
  if (na <= 0) return;

  if (nt > 0) {
    REALLOC (u, inst_param, na+nt);
  }
  else {
    MALLOC (u, inst_param, na+nt);
  }

  for (int i=0; i < na; i++) {
    u[nt].isatype = a[i].isatype;
    if (a[i].isatype) {
      if (a[i].u.tt) {
	u[nt].u.tt = new InstType (a[i].u.tt);
      }
      else {
	u[nt].u.tt = NULL;
      }
    }
    else {
      if (a[i].u.tp) {
	u[nt].u.tp = a[i].u.tp->Clone();
      }
      else {
	u[nt].u.tp = NULL;
      }
    }
    nt++;
  }
}


void InstType::refineBaseType (InstType *ut)
{
  /* XXX: do something fun! */
  nt = ut->nt;
  u = ut->u;
  t = ut->t;
}

