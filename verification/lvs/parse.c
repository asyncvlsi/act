/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

/*
 *
 *  Pure production rule parser. Understands normal expression syntax
 *  and parses production rules into pull-up and pull-down expressions.
 *  Production rules of the form:
 *          G1 -> z+
 *          G2 -> z+
 *  are turned into:
 *          G1 | G2 -> z+
 *
 *  The program does not understand any directives.
 *
 *  
 *
 */

#include <stdio.h>
#include "parse.h"
#include "pp.h"
#include "misc.h"
#include "bool.h"
#include "lvs.h"

#undef TOKEN
#define TOKEN(a,b) a,
static int
#include "tokens.def"
TRAILER;

static VAR_T *vtab = NULL;
static expr_t *expr (LEX_T *);

int exports_found;


/*------------------------------------------------------------------------
 * prs aliases
 *------------------------------------------------------------------------
 */
static
var_t *prs_canonical_name (var_t *v)
{
  var_t *o = v, *root;

  while (v->alias_prs)
    v = v->alias_prs;
  root = v;
  if (root != o) {
    while (o->alias_prs) {
      v = o->alias_prs;
      o->alias_prs = root;
      o = v;
    }
  }
  return root;
}

static expr_t *newexpr (void)
{
  expr_t *e;
  MALLOC (e,expr_t,1);
  e->l = e->r = NULL;
  return e;
}

struct excl *firstlist = NULL;
static struct excl *firsttl = NULL;

struct excl *crossinvlist = NULL;
static struct excl *crosstl = NULL;

struct excl *filterlist = NULL;
static struct excl *filtertl = NULL;

static void addtolist (struct excl **hd, struct excl **tl, var_t *v)
{
  struct excl *e;

  MALLOC (e, struct excl, 1);
  e->v = v;
  e->next = NULL;
  if (*tl) {
    (*tl)->next = e;
    *tl = e;
  }
  else {
    *tl = *hd = e;
  }

}

static void addcross (var_t *v)
{
  addtolist (&crossinvlist, &crosstl, v);
}

static void addfirst (var_t *v)
{
  addtolist (&firstlist, &firsttl, v);
}

static void addfilter (var_t *v)
{
  addtolist (&filterlist, &filtertl, v);
}

/*------------------------------------------------------------------------
 *
 *  Turn expression into a canonical form
 *
 *------------------------------------------------------------------------
 */
bool_t *expr_to_bool (BOOL_T *B, expr_t *e)
{
  bool_t *b1, *b2;
  bool_t *b;
  if (!e) return bool_false(B);
  switch (e->type) {
  case E_AND:
    b1 = expr_to_bool (B, e->l);
    b2 = expr_to_bool (B, e->r);
    b = bool_and (B, b1,b2);
    bool_free (B, b1);
    bool_free (B, b2);
    break;
  case E_OR:
    b1 = expr_to_bool (B, e->l);
    b2 = expr_to_bool (B, e->r);
    b = bool_or (B, b1, b2);
    bool_free (B, b1);
    bool_free (B, b2);
    break;
  case E_NOT:
    b1 = expr_to_bool (B, e->l);
    b = bool_not (B, b1);
    bool_free (B, b1);
    break;
  case E_ID:
    if (((var_t*)e->l)->flags & VAR_HAVEVAR)
      b = bool_var (B, ((var_t*)e->l)->v);
    else {
      ((var_t*)e->l)->flags |= VAR_HAVEVAR;
      ((var_t*)e->l)->v = bool_topvar (b = bool_newvar (B));
    }
    break;
  case E_TRUE:
    b = bool_true (B);
    break;
  case E_FALSE:
    b = bool_false (B);
    break;
  default:
    fatal_error ("expr_to_bool: unknown expression type");
  }
  return b;
}

/*------------------------------------------------------------------------
 *
 *  Pretty-print expression
 *
 *------------------------------------------------------------------------
 */
static void printexpr (pp_t *pp, expr_t *e, int prec)
{
  if (!e) { pp_puts (pp, "false"); return; }
  switch (e->type) {
  case E_AND:
    if (prec > 1) { pp_setb (pp); pp_puts (pp, "("); }
    printexpr (pp, e->l, 1);
    pp_lazy (pp,2);
    pp_puts (pp,"&");
    printexpr (pp, e->r, 1);
    if (prec > 1) { pp_puts (pp, ")"); pp_endb (pp); }
    break;
  case E_OR:
    if (prec > 0) { pp_setb (pp); pp_puts (pp, "("); }
    printexpr (pp, e->l, 0);
    pp_lazy (pp,2);
    pp_puts (pp,"|");
    printexpr (pp, e->r, 0);
    if (prec > 0) { pp_puts (pp,")"); pp_endb (pp); }
    break;
  case E_NOT:
    pp_puts (pp,"~");
    printexpr (pp, e->l, 2);
    break;
  case E_ID:
    pp_puts (pp,var_name((var_t*)e->l));
    break;
  case E_TRUE:
    pp_puts (pp, "true");
    break;
  case E_FALSE:
    pp_puts (pp, "false");
    break;
  default:
    fatal_error ("printexpr: unknown expression type");
  }
}

static void printexpr2 (pp_t *pp, expr_t *e, int prec)
{
  if (!e) { pp_puts (pp, "false"); return; }
  switch (e->type) {
  case E_AND:
    if (prec > 1) { pp_setb (pp); pp_puts (pp, "("); }
    printexpr2 (pp, e->l, 1);
    pp_lazy (pp,2);
    pp_puts (pp,"&");
    printexpr2 (pp, e->r, 1);
    if (prec > 1) { pp_puts (pp, ")"); pp_endb (pp); }
    break;
  case E_OR:
    if (prec > 0) { pp_setb (pp); pp_puts (pp, "("); }
    printexpr2 (pp, e->l, 0);
    pp_lazy (pp,2);
    pp_puts (pp,"|");
    printexpr2 (pp, e->r, 0);
    if (prec > 0) { pp_puts (pp,")"); pp_endb (pp); }
    break;
  case E_NOT:
    pp_puts (pp,"~");
    printexpr2 (pp, e->l, 2);
    break;
  case E_ID:
    pp_puts (pp,var_name((var_t*)e->l));
    pp_printf (pp, ":(%d)", ((var_t*)e->l)->v);
    break;
  case E_TRUE:
    pp_puts (pp, "true");
    break;
  case E_FALSE:
    pp_puts (pp, "false");
    break;
  default:
    fatal_error ("printexpr2: unknown expression type");
  }
}

extern void print_expr (pp_t *p, expr_t *e)
{
  printexpr (p,e,0);
}


extern void print_expr2 (pp_t *p, expr_t *e)
{
  printexpr2 (p,e,0);
}


/*------------------------------------------------------------------------
 *
 *  Parse expression
 *
 *------------------------------------------------------------------------
 */
static expr_t *unary (LEX_T *l)
{
  expr_t *e, *ret;
  int parity;
  var_t *v;

  ret = NULL;
  if (lex_have_keyw (l, "true")) {
    ret = newexpr();
    ret->type = E_TRUE;
  }
  else if (lex_have_keyw (l, "false")) {
    ret = newexpr();
    ret->type = E_FALSE;
  }
  else if (lex_have (l, l_id)) {
    ret = newexpr();
    ret->type = E_ID;
    v = var_enter (vtab,lex_prev(l));
    v->inprs = 1;
    v->flags |= VAR_INPUT;
    ret->l = (expr_t *)prs_canonical_name(v);
  }
  else if (lex_have (l,l_string)) {
    char *s;
    ret = newexpr();
    ret->type = E_ID;
    s = lex_prev(l);
    s[strlen(s)-1] = '\0';
    v = var_enter (vtab,s+1);
    v->inprs = 1;
    v->flags |= VAR_INPUT;
    ret->l = (expr_t *)prs_canonical_name(v);
  }
  else if (lex_sym (l) == NOT) {
    parity = 1;
    while (lex_have (l, NOT)) {
      parity = 1-parity;
    }
    if (parity)
      ret = unary(l);
    else {
      e = unary (l);
      ret = newexpr();
      ret->type = E_NOT;
      ret->l = e;
    }
  }
  else if (lex_have (l, LPAR)) {
    ret = expr (l);
    lex_mustbe (l,RPAR);
  }
  return ret;
}

static expr_t *term (LEX_T *l)
{
  expr_t *e, *ret;
  
  ret = unary (l);
  while (ret && lex_have (l,AND)) {
    e = ret;
    ret = newexpr ();
    ret->type = E_AND;
    ret->l = e;
    ret->r = unary (l);
  }
  return ret;
}

static expr_t *expr (LEX_T *l)
{
  expr_t *e, *ret;

  ret = term (l);
  while (ret && lex_have (l,OR)) {
    e = ret;
    ret = newexpr ();
    ret->type = E_OR;
    ret->l = e;
    ret->r = term(l);
  }
  return ret;
}


/*------------------------------------------------------------------------
 *
 * Add a disjunct corresponding to the rule e -> v+
 *
 *------------------------------------------------------------------------
 */
static
void add_upguard (var_t *v, expr_t *e, int which)
{
  if (!v->up[which])
    v->up[which] = (void*)e;
  else {
    expr_t *e1;
    e1 = newexpr();
    e1->type = E_OR;
    e1->l = e;
    e1->r = (expr_t *)v->up[which];
    v->up[which] = (void*)e1;
  }
}

/*------------------------------------------------------------------------
 *
 * Add a disjunct corresponding to the rule  e -> v-
 *
 *------------------------------------------------------------------------
 */
static
void add_dnguard (var_t *v, expr_t *e, int which)
{
  if (!v->dn[which])
    v->dn[which] = (void*)e;
  else {
    expr_t *e1;
    e1 = newexpr();
    e1->type = E_OR;
    e1->l = e;
    e1->r = (expr_t *)v->dn[which];
    v->dn[which] = (void*)e1;
  }
}



char *lex_mustbe_id (LEX_T *l)
{
  char *s;

  if (lex_have (l,l_id))
    s = lex_prev(l);
  else if (lex_have (l, l_string)) {
    s = lex_prev(l);
    s[strlen(s)-1] = '\0';
    s++;
  }
  else
    fatal_error ("Error in parsing. Expecting id.\n%s", lex_errstring (l));
  return s;
}
    

/*------------------------------------------------------------------------
 *
 *  Create prs connection
 *
 *  Given strings s1 and s2, connects s1 and s2 in the prs  file.
 *
 *------------------------------------------------------------------------
 */
void create_prs_connection (char *s1, char *s2)
{
  var_t *v, *v1, *circ;

  v = var_enter (vtab, s1);
  v->inprs = 1;
  v = prs_canonical_name(v);
  v1 = var_enter (vtab, s2);
  v1->inprs = 1;
  v1 = prs_canonical_name(v1);
  if (v != v1) {
    v1->alias_prs = v;
    circ = v1->alias_ring_prs;
    v1->alias_ring_prs = v->alias_ring_prs;
    v->alias_ring_prs = circ;
  }
}

/*------------------------------------------------------------------------
 *
 * Parse a production rule, and add it to the production rule set stored
 * in the variable table.
 *
 *   vtab: global with variable table stored in it.
 *
 *------------------------------------------------------------------------
 */
void parse_prs (LEX_T *l, int which)
{
  expr_t *e;
  var_t *v;
  var_t *circ;
  var_t *tmp;
  var_t *v1;
  char *s;
  int flag;
     	   
  /* read EXCL(a,b) rules, connect directives */
  do {
    flag = 1;
    if (lex_have_keyw (l, "excl") || lex_have_keyw (l, "exclhi")) {
      circ = NULL;
      lex_mustbe (l, LPAR);
      do {
	s = lex_mustbe_id (l);
	v = var_enter (vtab, s);
	v->inprs = 1;
	v = prs_canonical_name (v);
	if (v->exclhi != v) {
	  struct excl *ll = NULL;
	  /*fatal_error ("Name [%s] in multiple exclusive lists (feature not implemented)", var_name(v));*/
	  if (circ != NULL) {
	    ll = add_to_excl_list (ll, circ);
	    for (tmp = circ->exclhi; tmp != circ; tmp = tmp->exclhi)
	      ll = add_to_excl_list (ll, tmp);
	  }
	  while (1) {
	    ll = add_to_excl_list (ll, v);
	    if (!lex_have (l, COMMA)) break;
	    s = lex_mustbe_id (l);
	    v = var_enter (vtab, s);
	    v->inprs = 1;
	    v = prs_canonical_name (v);
	  }
	  add_exclhi_list (ll);
	  goto multihi;
	}
	if (!circ)
	  circ = v;
	else {
	  tmp = circ->exclhi;
	  circ->exclhi = v->exclhi;
	  v->exclhi = tmp;
	}
      } while (lex_have (l, COMMA));
multihi:      
      lex_mustbe (l, RPAR);
    }
    else if (lex_have_keyw (l, "excllo")) {
      circ = NULL;
      lex_mustbe (l, LPAR);
      do {
	s = lex_mustbe_id(l);
	v = var_enter (vtab, s);
	v->inprs = 1;
	v = prs_canonical_name(v);
	if (v->excllo != v) {
	  struct excl *ll = NULL;
	  /*fatal_error ("Name [%s] in multiple exclusive lists (feature not implemented)", var_name (v));*/
	  if (circ != NULL) {
	    ll = add_to_excl_list (ll, circ);
	    for (tmp = circ->excllo; tmp != circ; tmp = tmp->excllo)
	      ll = add_to_excl_list (ll, tmp);
	  }
	  while (1) {
	    ll = add_to_excl_list (ll, v);
	    if (!lex_have (l, COMMA)) break;
	    s = lex_mustbe_id (l);
	    v = var_enter (vtab, s);
	    v->inprs = 1;
	    v = prs_canonical_name (v);
	  }
	  add_excllo_list (ll);
	  goto multilo;
	}
	if (!circ)
	  circ = v;
	else {
	  tmp = circ->excllo;
	  circ->excllo = v->excllo;
	  v->excllo = tmp;
	}
      } while (lex_have (l, COMMA));
multilo:
      lex_mustbe (l, RPAR);
    }
    else if (lex_have_keyw (l, "connect")) {
      /* connect */
      s = lex_mustbe_id (l);
      v = var_enter (vtab, s);
      v->inprs = 1;
      v = prs_canonical_name(v);
      s = lex_mustbe_id (l);
      v1 = var_enter (vtab, s);
      v1->inprs = 1;
      v1 = prs_canonical_name(v1);
      if (v != v1) {
	v1->alias_prs = v;
	circ = v1->alias_ring_prs;
	v1->alias_ring_prs = v->alias_ring_prs;
	v->alias_ring_prs = circ;
      }
    }
    else if (lex_have_keyw (l, "order")) {
      /* variable order directives */
      lex_mustbe (l, LPAR);
      do {
	s = lex_mustbe_id (l);
	addfirst (var_enter (vtab, s));
      } while (lex_have (l, COMMA));
      lex_mustbe (l, RPAR);
    }
    else if (lex_have_keyw (l, "cross_coupled_inverters")) {
      lex_mustbe (l, LPAR);
      s = lex_mustbe_id (l);
      addcross (var_enter (vtab, s));
      lex_mustbe (l, COMMA);
      s = lex_mustbe_id (l);
      addcross (var_enter (vtab, s));
      lex_mustbe (l, RPAR);
    }
    else if (lex_have_keyw (l, "filter")) {
      lex_mustbe (l, LPAR);
      s = lex_mustbe_id (l);
      addfilter (var_enter (vtab, s));
      lex_mustbe (l, COMMA);
      s = lex_mustbe_id (l);
      addfilter (var_enter (vtab, s));
      lex_mustbe (l, RPAR);
    }
    else if (lex_have_keyw (l, "export")) {
      lex_mustbe (l, LPAR);
      s = lex_mustbe_id (l);
      v = var_enter (vtab, s);
      v->flags |= VAR_EXPORT;
      exports_found = 1;
      lex_mustbe (l, RPAR);
    }
    else if (lex_have_keyw (l, "attrib")) {
      s = lex_mustbe_id (l);
      if (strcmp (s, "unstaticized") == 0) {
	lex_mustbe (l, LPAR);
	s = lex_mustbe_id (l);
	v = var_enter (vtab, s);
	v->flags |= VAR_UNSTAT;
	lex_mustbe (l, RPAR);
      }
      else {
	/* skip it */
	lex_mustbe (l, LPAR);
	do {
	  s = lex_mustbe_id (l);
	} while (lex_have (l, COMMA) || lex_have (l, SEMI));
	lex_mustbe (l, RPAR);
      }
    }
    else
      flag = 0;
  } while (flag);
  /* accept "after N" format */
  if (lex_have_keyw (l,"after"))
    lex_mustbe (l, l_integer);
  e = expr (l);
  if (e == NULL && lex_eof (l)) 
    return;
  lex_mustbe (l, ARROW);
  if (lex_have (l, l_id)) {
    v = var_enter (vtab, lex_prev(l));
    v->inprs = 1;
    v = prs_canonical_name(v);
    if (lex_have (l, UP))
      add_upguard (v, e, which);
    else if (lex_have (l, DN))
      add_dnguard (v, e, which);
    else
      fatal_error ("Error parsing production rule. Expected \"+\" or \"-\".\n%s", lex_errstring (l));
    v->flags |= VAR_OUTPUT|VAR_PRSOUTPUT;
  }
  else if (lex_have (l, l_string)) {
    s = lex_prev(l);
    s[strlen(s)-1] = '\0';
    v = var_enter (vtab, s+1);
    v->inprs = 1;
    v = prs_canonical_name(v);
    if (lex_have (l, UP))
      add_upguard (v, e, which);
    else if (lex_have (l, DN))
      add_dnguard (v, e, which);
    else
      fatal_error ("Error parsing production rule. Expected \"+\" or \"-\".\n%s", lex_errstring (l));
    v->flags |= VAR_OUTPUT|VAR_PRSOUTPUT;
  }
  else
    fatal_error ("Error parsing production rule. Expected an identifier.\n%s", 
		 lex_errstring (l));
}


/*------------------------------------------------------------------------
 *
 *  Parse a file containing production rules.
 *
 *------------------------------------------------------------------------
 */
int parse_prs_input (LEX_T *l, VAR_T *v, int which)
{
#undef TOKEN
#define TOKEN(a,b) a = lex_addtoken(l,b);
#include "tokens.def"  
  exports_found = 0;
  lex_getsym (l);
  vtab = v;
  while (!lex_eof (l)) {
    parse_prs (l,which);
  }
  return 1;
}


/*------------------------------------------------------------------------
 *
 *  Print production rules
 *
 *------------------------------------------------------------------------
 */
void 
print_prs_expr (pp_t *p, VAR_T *V, int which)
{
  var_t *v;

  for (v = var_step_first (V); v; v = var_step_next (V)) {
    if (!v->up[which] && !v->dn[which]) continue;
    if (v->up[which])
      print_expr (p, (expr_t*)v->up[which]);
    else
      pp_puts (p, "<null> ");
    pp_puts (p, "-> ");
    pp_puts (p, var_name (v));
    pp_puts (p, "+");
    pp_forced (p,0);
    if (v->dn[which])
      print_expr (p, (expr_t*)v->dn[which]);
    else
      pp_puts (p, "<null> ");
    pp_puts (p, "-> ");
    pp_puts (p, var_name (v));
    pp_puts (p, "-");
    pp_forced (p,0);
    pp_forced (p,0);
  }
}
 
