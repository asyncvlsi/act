/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

#include <stdio.h>
#include <signal.h>
#include "lvs.h"
#include <common/misc.h>

static int interrupted = 0;

/*------------------------------------------------------------------------
 *
 *  Convert bool identifier into var_t structure
 *
 *------------------------------------------------------------------------
 */
static
var_t *id_to_var (VAR_T *V, bool_var_t id)
{
  var_t *v;
  int i;
  hash_bucket_t *e;
  for (i = 0; i < V->H->size; i++)
    for (e = V->H->head[i]; e; e = e->next) {
      v = (var_t*)e->v;
      if ((v->flags & VAR_HAVEVAR) && id == v->v)
	return v;
    }
  return NULL;
}

/*------------------------------------------------------------------------
 *
 *  Check if names needs to be quoted
 *
 *------------------------------------------------------------------------
 */
static
int quote_name (char *s)
{
  if (*s == '"') return 0;
  while (*s) {
    if (*s == '/' || *s == '!' || *s == '[' || *s == ']' || *s == '.'
	|| *s == '#' || *s == '&')
      return 1;
    s++;
  }
  return 0;
}



/*------------------------------------------------------------------------
 * 
 *  print_bool --
 *
 *     Print boolean expression out to "pp"
 *
 *------------------------------------------------------------------------
 */
void print_bool (pp_t *pp, BOOL_T *B, VAR_T *V, bool_t *b, int type)
{
  



}



/*------------------------------------------------------------------------
 * 
 *
 *   Original output routine
 *
 *
 *------------------------------------------------------------------------
 */
struct output {
  var_t **names;
  bool_t *b;
  int len;
  int prune;
  struct output *next;
};

static struct output *HEAD = NULL;


static int  namecnt;
static int first;
static var_t **names = NULL;

/*------------------------------------------------------------------------
 *
 *  Save the disjunct in the names[] array into HEAD
 *
 *------------------------------------------------------------------------
 */
static
void add_disjunct (BOOL_T *B)
{
  struct output *o;
  int i;

  MALLOC (o, struct output, 1);
  o->next = HEAD;
  o->prune = 0;
  o->len = namecnt;
  if (namecnt != 0)
    MALLOC (o->names, var_t *, namecnt);
  o->b = bool_true (B);
  for (i=0; i < namecnt; i++) {
    if (interrupted) return;
    o->names[i] = names[i];
    o->b = bool_and (B, o->b, bool_var (B, names[i]->v));
  }
  o->next = HEAD;
  HEAD = o;
}


/*------------------------------------------------------------------------
 *
 *  Prune redundant terms from "HEAD"
 *
 *------------------------------------------------------------------------
 */
static
void prune_output (BOOL_T *B)
{
  struct output *o, *p;
  int i, j, done;
  bool_t *b1, *b2;

  for (o = HEAD; o ; o = o->next) {
    if (interrupted) return;
    if (o->prune) continue;
    for (p = o->next; p ; p = p->next) {
      if (p->prune || o->prune) continue;
      b1 = bool_or (B, o->b, p->b);
      if (b1 == p->b)
	o->prune = 1;
      else if (b1 == o->b)
	p->prune = 1;
    }
  }
}


/*------------------------------------------------------------------------
 *
 *  Dump non-redundant terms on output
 *
 *------------------------------------------------------------------------
 */
static
void dump_output (pp_t *pp, int type)
{
  int first = 0;
  struct output *o;
  int i;
  
  while (HEAD) {
    if (!HEAD->prune) {
      if (first) {
	pp_lazy (pp, 2);
	pp_puts (pp, " | ");
      }
      pp_setb (pp);
      for (i=0; i < HEAD->len; i++) {
	if (type == P_TYPE)
	  pp_puts (pp,"~");
	if (quote_name (var_name (HEAD->names[i]))) {
	  pp_puts (pp, "\"");
	  pp_puts (pp, var_name (HEAD->names[i]));
	  pp_puts (pp, "\"");
	}
	else
	  pp_puts (pp, var_name (HEAD->names[i]));
	if (i != HEAD->len-1)
	  pp_puts (pp, "&");
	pp_lazy (pp, 2);
      }
      pp_endb (pp);
      first = 1;
      if (HEAD->len == 0) {
	if (type == P_TYPE) {
	  pp_puts (pp, "~");
	  if (quote_name (GNDnode))
	    pp_printf (pp, "\"%s\"", GNDnode);
	  else
	    pp_puts (pp, GNDnode);
	}
	else  {
	  if (quote_name (Vddnode))
	    pp_printf (pp, "\"%s\"", Vddnode);
	  else
	    pp_puts (pp, Vddnode);
	}
      }
    }
    if (HEAD->len > 0)
      FREE (HEAD->names);
    o = HEAD->next;
    FREE (HEAD);
    HEAD = o;
  }
}

/*------------------------------------------------------------------------
 *
 *  Check if v1 & v2 == false
 *
 *------------------------------------------------------------------------
 */
static
int are_they_excl (var_t *v1, var_t *v2, int type)
{
  var_t *v;
  if (type == N_TYPE) {
    for (v = v1->exclhi; v != v1; v = v->exclhi)
      if (v2 == v) return 1;
    return extra_exclhi (v1, v2);
  }
  else {
    for (v = v1->excllo; v != v1; v = v->excllo)
      if (v2 == v) return 1;
    return extra_excllo (v1, v2);
  }
  /*NOTREACHED*/
  return 0;
}

/*------------------------------------------------------------------------
 *
 *  Check if names[] array has exclusive conjuncts
 *
 *------------------------------------------------------------------------
 */
static
int check_excl (int type)
{
  int i, j;
  int excl = 0;
  for (i=0; i < namecnt; i++)
    for (j=i+1; j < namecnt; j++)
      if (are_they_excl (names[i], names[j], type))
	return 1;
  return 0;
}


/*------------------------------------------------------------------------
 *
 * Recursive print routine
 *
 *------------------------------------------------------------------------
 */
static
void slow_special (pp_t *pp, BOOL_T *B, VAR_T *V, bool_t *b, int type)
{
  int lfirst;
  int i;

  if (interrupted) return;

  if (bool_isleaf(b)) {
    if (b == bool_true (B)) {
      if (check_excl (type))
	return;
      add_disjunct (B);
    }
  }
  else {
    if (type == N_TYPE) {
      names[namecnt] = id_to_var (V,b->id);
      namecnt++;
    }
    slow_special (pp,B,V,b->l,type);
    if (type == N_TYPE) namecnt--;
    if (type == P_TYPE) {
      names[namecnt] = id_to_var (V,b->id);
      namecnt++;
    }
    slow_special (pp,B,V,b->r,type);
    if (type == P_TYPE) namecnt--;
  }
}

void stop_printing (int x)
{
  interrupted = 1;
  return;
}

void print_slow_special (pp_t *pp, BOOL_T *B, VAR_T *V, bool_t *b, int type)
{
  first = 0;

  if (!names) {
    MALLOC (names,var_t *, var_number (V));
  }
  namecnt = 0;
  interrupted = 0;
  signal (SIGALRM, stop_printing);
  alarm(30); /* 30 seconds */
  slow_special (pp, B, V, b, type);
  if (interrupted) goto intr;
  prune_output (B);
  if (interrupted) goto intr;
  alarm(0);
  pp_setb (pp);
  dump_output (pp, type);
  pp_endb (pp);
  return;
intr:
  alarm(0);
  pp_printf (PPout, "[interrupted by timeout]");
  return;
}


/*------------------------------------------------------------------------
 *
 *   print_bexpr --
 *
 *      Raw dump of bool_t structure
 *
 *------------------------------------------------------------------------
 */
static
void print_bexpr (pp_t *pp, VAR_T *V, bool_t *b)
{
  if (bool_isleaf(b))
    pp_printf (pp,"%s", b->id & 0x01 ? "T" : "F");
  else {
    pp_setb (pp);
    pp_printf (pp,"[ %s,", var_name(id_to_var(V,b->id)));
    pp_lazy (pp, 2);
    pp_puts (pp, "t=");
    print_bexpr (pp,V,b->l); pp_printf (pp,",");
    pp_lazy (pp, 2);
    pp_puts (pp, "f=");
    print_bexpr (pp,V,b->r);
    pp_lazy (pp, 0);
    pp_printf (pp," ]");
    pp_endb (pp);
  }
}

