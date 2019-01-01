/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

#include <stdio.h>
#include "ext.h"
#include "lvs.h"
#include "var.h"
#include "misc.h"
#include "hier.h"

static int flatten_errors = 0;
unsigned long num_fets = 0;

static char *mystrdup (char *s)
{
  char *t;
  MALLOC (t, char, strlen(s)+1);
  strcpy (t, s);
  return t;
}

static
int bang_exists (char *s)
{
  while (*s) {
    if (*s == '!') return 1;
    s++;
  }
  return 0;

}


#define Min(a,b) ((a) < (b) ? (a) : (b))
/*#define Max(a,b) ((a) > (b) ? (a) : (b))*/


static
void _flatten_ext_file (struct ext_file *ext, VAR_T *V, char *path, int mark)
{
  struct ext_fets *fet;
  struct ext_list *subcells;
  struct ext_alias *aliases;
  struct ext_cap *cap;
  struct ext_attr *attr;
  struct ext_ap *ap;
  char *name, *nm, *name3;
  int len;
  int i, j;
  var_t *v1, *v2, *v3, *v4, *v5;
  edgelist_t *e;
  int ret;
  dots_t d;
  int glob;
  var_t *vdd, *gnd;

  if (!mark && ext->fet) ext->mark = 1;
  len = strlen (path) + 1;

  if (ext->h) {
    /*struct hash_cell *cell;*/
    hash_bucket_t *cell;

    /* process hierarchical file */
    MALLOC (name, char, strlen (path)+1);
    strcpy (name, path);
    name_convert (name, &d);
    v1 = var_locate (V, name);
    if (v1)
      fatal_error ("Subcell `%s' is also a node name!\n", path);
    v1 = var_enter (V, name);
    validate_name (v1, &d);
    v1->hcell = 1;
    v1->hc = (hash_bucket_t *)ext->h;
    if (!var_locate (V, path)) {
      v1 = var_enter (V, path);
      v1->hcell = 1;
      v1->hc = (hash_bucket_t *)ext->h;
    }
    FREE (name);

    for (i=0; i < ext->h->size; i++)
      for (cell = ext->h->head[i]; cell; cell = cell->next)
	MK_UNSEEN(cell);


    for (i=0; i < ext->h->size; i++)
      for (cell = ext->h->head[i]; cell; cell = cell->next) {
	MALLOC (name, char, len + strlen (KEY(cell))+1);
	sprintf (name, "%s/%s", path, KEY(cell));
	name_convert (name, &d);
	/* 
	   no name validation here; the subcell has already validated
	   the name.
	 */
	v1 = var_find (V, name);
	if (!v1) {
	  if (bang_exists (KEY(cell)))
	    /* global nodes */
	    MK_SEEN (ROOT(cell));
	  v1 = var_enter (V, name);
	}
	else
	  MK_SEEN(ROOT(cell));
	FREE (name);
	Assert (v1, "Hmm");
	if (v1) {
	  v1->hname = 1;
	  v1->hc = cell;
	  v1->inlayout = 1;
	}
      }
    
    for (i=0; i < ext->h->size; i++)
      for (cell = ext->h->head[i]; cell; cell = cell->next) {
	if (ROOT(cell) != cell) {
	  MALLOC (name, char, len + strlen (KEY(cell)) + 1);
	  sprintf (name, "%s/%s", path, KEY(cell));
	  name_convert (name, &d);
	  v1 = var_enter (V, name);
	  validate_name (v1, &d);
	  FREE (name);

	  MALLOC (name, char, len + strlen (KEY(ROOT(cell))) + 1);
	  sprintf (name, "%s/%s", path, KEY(ROOT(cell)));
	  name_convert (name, &d);
	  v2 = var_enter (V, name);
	  validate_name (v2, &d);
	  FREE (name);

	  v1->inlayout = v2->inlayout = 1;
	  v3 = canonical_name (v1);
	  v4 = canonical_name (v2);
	  if (v4 != v3) {
	    v4->alias = v3;
	    v5 = v4->alias_ring;
	    v4->alias_ring = v3->alias_ring;
	    v3->alias_ring = v5;
	  }
	}
	else {
	  if (!IS_SEEN(cell))
	    fatal_error ("hierarchical name `%s/%s' not found. Invalid prs file!",
			 path, KEY(cell));

	  MALLOC (name, char, len + strlen (KEY(cell)) + 1);
	  sprintf (name, "%s/%s", path, KEY(cell));
	  name_convert (name, &d);
	  v1 = var_enter (V, name);
	  validate_name (v1, &d);
	  FREE (name);
	}
      }
    return;
  }

  for (subcells = ext->subcells; subcells; subcells = subcells->next) {
    MALLOC (name, char, len + 3 + 40); /* gross. oh well. */
    strcpy (name, path);
    if (len > 1)
      strcat (name, "/");
    strcat (name, subcells->id);
    if (subcells->xlo == 0 && subcells->xhi == 0 &&
	subcells->ylo == 0 && subcells->yhi == 0) {
      _flatten_ext_file (subcells->ext, V, name, ext->mark);
    }
    else {
      nm = mystrdup (name);
      for (i=Min(subcells->xlo,subcells->xhi); 
	   i <= Max(subcells->xlo,subcells->xhi); 
	   i++)
	for (j=Min(subcells->ylo,subcells->yhi); 
	     j <= Max(subcells->ylo,subcells->yhi); 
	     j++) {
	  if (subcells->xlo == subcells->xhi)
	    sprintf (name, "%s[%d]", nm, j);
	  else if (subcells->ylo == subcells->yhi)
	    sprintf (name, "%s[%d]", nm, i);
	  else
	    sprintf (name, "%s[%d,%d]", nm, j, i);
	  _flatten_ext_file (subcells->ext, V, name, ext->mark);
	}
      FREE (nm);
    }
    FREE (name);
  }

  /* instantiate fets */
  MALLOC (name, char, len + 1024);
  MALLOC (nm, char, len + 1024);
  MALLOC (name3, char, len + 1024);

  for (fet = ext->fet; fet;  fet = fet->next) {
    num_fets++;
    if (len > 1) {
      sprintf (name, "%s/%s", path, fet->g);
      sprintf (nm, "%s/%s", path, fet->t1);
      sprintf (name3, "%s/%s", path, fet->t2);
    }
    else {
      sprintf (name, "%s", fet->g);
      sprintf (nm, "%s", fet->t1);
      sprintf (name3, "%s", fet->t2);
    }
    name_convert (name, &d); v1 = var_enter (V, name);
    validate_name (v1, &d);

    name_convert (nm, &d); v2 = var_enter (V, nm);
    validate_name (v2, &d);

    name_convert (name3, &d); v3 = var_enter (V, name3);
    validate_name (v3, &d);

    v1->inlayout = v2->inlayout = v3->inlayout = 1;

    /* add edge from v2 -> v3, and from v3 -> v2 */
    MALLOC (e, edgelist_t, 1);
    e->isweak = fet->isweak;
    e->gate = v1; e->t1 = v3;
    e->type = fet->type;
    e->length = fet->il;
    e->width = fet->iw;
    e->next = v2->edges; v2->edges = e;

    MALLOC (e, edgelist_t, 1);
    e->isweak = fet->isweak;
    e->gate = v1; e->t1 = v2;
    e->type = fet->type;
    e->length = fet->il;
    e->width = fet->iw;
    e->next = v3->edges; v3->edges = e;

#ifndef DIGITAL_ONLY
    /* add capacitances */
    if (fet->type == P_TYPE) {
      v1->c.p_gA += (fet->iw*fet->il);
      v1->c.p_gP += 2*(fet->iw+fet->il);
    }
    else {
      v1->c.n_gA += (fet->iw*fet->il);
      v1->c.n_gP += 2*(fet->iw+fet->il);
    }
#endif
    /* if strong */
    if ((strip_by_width == 0
	 && (double)e->width/(double)e->length >= strip_threshold) ||
	(strip_by_width == 1 && e->width >= width_threshold))
      v1->flags |= VAR_INPUT;
    if (debug_level > 60) {
      pp_printf (PPout, "edge: %s - %s - %s [%c,weak=%d] w=%d, l=%d", var_name(v2), 
		 var_name(v1), var_name(v3), fet->type == P_TYPE ? 'p' : 'n', fet->isweak, fet->iw, fet->il);
      pp_forced (PPout,0);
    }
  }
  FREE (name3);

  /* make connections */
  for (aliases = ext->aliases; aliases; aliases = aliases->next) {
    if (len > 1) {
      sprintf (name, "%s/%s", path, aliases->n1);
      sprintf (nm, "%s/%s", path, aliases->n2);
    }
    else {
      sprintf (name, "%s", aliases->n1);
      sprintf (nm, "%s", aliases->n2);
    }
    /* connect nm with name */
    array_fixup (nm);
    array_fixup (name);
    if ((name3 = hier_subcell_node (V, nm, &v4, '/'))) {
      /* FIXME: check dots to subcell name */
      if (!hash_lookup ((struct Hashtable *)v4->hc, name3)) {
	pp_printf (PPout, "Connection `%s' =* `%s' to non i/o node in subcell.",
		   name, nm);
	pp_forced (PPout, 0);
	flatten_errors++;
	continue;
	/*fatal_error ("Connection `%s=%s' to non i/o node in subcell.\n",
		     name, nm);*/
      }
    }
    if ((name3 = hier_subcell_node (V, name, &v4, '/'))) {
      /* FIXME: check dots to subcell name */
      if (!hash_lookup ((struct Hashtable*)v4->hc, name3)) {
	pp_printf (PPout, "Connection `%s' *= `%s' to non i/o node in subcell.",
		   name, nm);
	pp_forced (PPout, 0);
	flatten_errors++;
	continue;
	/*fatal_error ("Connection `%s=%s' to non i/o node in subcell.\n",
		     name, nm);*/
      }
    }

    name_convert (name, &d); v1 = var_enter (V, name);
    validate_name (v1, &d);

    name_convert (nm, &d); v2 = var_enter (V, nm);
    validate_name (v2, &d);

    v1->inlayout = v2->inlayout = 1;
    v3 = canonical_name (v1);
    v4 = canonical_name (v2);
    if (v4 != v3) {
      v4->alias = v3;
      v1 = v4->alias_ring;
      v4->alias_ring = v3->alias_ring;
      v3->alias_ring = v1;
    }
  }

  vdd = var_locate (V, Vddnode);
  if (vdd) vdd = canonical_name (vdd);
  gnd = var_locate (V, GNDnode);
  if (gnd) gnd = canonical_name (gnd);

#ifndef DIGITAL_ONLY
  /* put in capacitances */
  for (cap = ext->cap; cap; cap =  cap->next) {
    if (len > 1) {
      sprintf (name, "%s/%s", path, cap->n1);
      if (cap->n2)
	sprintf (nm, "%s/%s", path, cap->n2);
    }
    else {
      sprintf (name, "%s", cap->n1);
      if (cap->n2)
	sprintf (nm, "%s", cap->n2);
    }
    /* connect nm with name */
    array_fixup (nm);
    array_fixup (name);
    if (cap->n2 && (name3 = hier_subcell_node (V, nm, &v4, '/'))) {
      /* okay... to hxt subcell. let's ignore this for now, shall we? */
      continue;
    }
    if (name3 = hier_subcell_node (V, name, &v4, '/')) {
      /* okay... ignore! */
      continue;
    }
    name_convert (name, &d); v1 = var_enter (V, name);
    validate_name (v1, &d);

    if (cap->n2) {
      name_convert (nm, &d); v2 = var_enter (V, nm);
      validate_name (v2, &d);
    }
    if (cap->type == CAP_GND) {
      v1->c.goodcap += cap->cap;
    }
    else if (cap->type == CAP_CORRECT) {
      v1->c.goodcap += cap->cap;
      /*v2->cap += cap->cap;*/
    }
    else if (cap->type == CAP_INTERNODE) {
      if (canonical_name (v1) == vdd || canonical_name (v1) == gnd)
	v2->c.goodcap += cap->cap;
      else if (canonical_name (v2) == vdd || canonical_name (v2) == gnd) 
	v1->c.goodcap += cap->cap;
      else {
	struct coupling_list *cl;

	v1->c.badcap += cap->cap;
	v2->c.badcap += cap->cap;

	MALLOC (cl, struct coupling_list, 1);
	cl->next = v1->c.cap;
	cl->cap = cap->cap/2;
	cl->node = v2;
	v1->c.cap = cl;

	MALLOC (cl, struct coupling_list, 1);
	cl->next = v2->c.cap;
	cl->cap = cap->cap/2;
	cl->node = v1;
	v2->c.cap = cl;
      }
    }
  }
#endif
  for (attr = ext->attr; attr; attr =  attr->next) {
    if (len > 1)
      sprintf (name, "%s/%s", path, attr->n);
    else
      sprintf (name, "%s", attr->n);
    array_fixup (name);
    if ((name3 = hier_subcell_node (V, name, &v4, '/'))) {
      /* okay... ignore! */
      continue;
    }
    name_convert (name, &d); v1 = var_enter (V, name);
    validate_name (v1, &d);
    v1->flags2 |= attr->attr;
  }
#ifndef DIGITAL_ONLY
  for (ap = ext->ap; ap; ap = ap->next) {
    if (len > 1)
      sprintf (name, "%s/%s", path, ap->node);
    else
      sprintf (name, "%s", ap->node);
    array_fixup (name);
    if (name3 = hier_subcell_node (V, name, &v4, '/')) {
      /* okay... ignore! */
      continue;
    }
    name_convert (name, &d); v1 = var_enter (V, name);
    validate_name (v1, &d);

    v1->c.n_area += ap->n_area;
    v1->c.n_perim += ap->n_perim;

    v1->c.p_area += ap->p_area;
    v1->c.p_perim += ap->p_perim;
  }
#endif

  FREE (name);
  FREE (nm);
}


/*------------------------------------------------------------------------
 *
 *  flatten_ext_file --
 *
 *      Flatten the extract file into the variable table
 *
 *------------------------------------------------------------------------
 */
void flatten_ext_file (struct ext_file *ext, VAR_T *V)
{
  flatten_errors = 0;
  num_fets = 0;
  _flatten_ext_file (ext, V, "", 0);
  if (flatten_errors) 
    fatal_error ("%d hierarchy error%s, cannot continue.", flatten_errors,
		 flatten_errors > 1 ? "s" : "");
}



static void _clear_mark (struct ext_file *ext)
{
  struct ext_list *subcells;
  
  if (ext->mark == 0) return;
  
  ext->mark = 0;
  
  if (ext->h) {
    /* summary, we're done */
    return;
  }

  for (subcells = ext->subcells; subcells; subcells = subcells->next) {
    _clear_mark (subcells->ext);
  }
}

static void _width_length_lambda (struct ext_file *ext)
{
  struct ext_list *subcells;
  struct ext_fets *fet;
  
  if (ext->mark) return;
  ext->mark = 1;
  
  if (ext->h) {
    /* summary, we're done */
    return;
  }

  for (fet = ext->fet; fet; fet = fet->next) {
    /* convert w, l into lambda units */
    fet->il = fet->length/lambda;
    fet->iw = fet->width/lambda;
    if (fet->il < 2) { fet->il = 2; }
  }

  for (subcells = ext->subcells; subcells; subcells = subcells->next) {
    _width_length_lambda (subcells->ext);
  }
}

/*------------------------------------------------------------------------
 *
 * Convert dimensions into lambda units
 *
 *------------------------------------------------------------------------
 */
void width_length_lambda (struct ext_file *ext)
{
  _width_length_lambda (ext);
  _clear_mark (ext);
}
