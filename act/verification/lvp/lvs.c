/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

/*
 *
 *  Main comparison routines
 *
 */

#include <stdio.h>
#include <common/lex.h>
#include <common/ext.h>
#include <common/pp.h>
#include <common/misc.h>
#include <common/bool.h>
#include "parse.h"
#include "lvs.h"
#include "dots.h"

/*------------------------------------------------------------------------
 *
 *  array_fixup --
 *
 *     Interpret "( )" in names; If they are immediately followed by [ ],
 *     then they are interpreted as an offset to the array dimension;
 *     If they are not followed by any [ ], they get turned into [ ].
 *
 *------------------------------------------------------------------------
 */
void array_fixup (char *s)
{
  char *t = s;
  char *u, *v, *w, *x;
  int idx1[2], idx2[2];
  int d1, d2;
  char buf[10];

  while (*t) {
next:
    /* Assert: the previous component of the path in the name has been
       processed; t points to the place in the input string at the beginning
       of the next component in the path; s points to the place in the
       input where we have to copy the the fixed name.

       IMPORTANT: The new name is guaranteed to be _shorter_ than the
       original one; therefore we can do this in-place; 
       How? Well, the code is obvious :)
    */
    while (*t && *t != '(')
      *s++ = *t++;
    if (!*t) break;
    u = t+1;
    while (*u && *u != '/' && *u != ')')
      u++;
    if (*u == '/' || !*u) {
      v = t;
      while (v != u+1)
	*s++ = *v++;
      t = v;
      goto next;
    }
    /* t .. u corresponds to ( .. ) */
    if (*(u+1) != '[') {
      *t = '[';
      *u = ']';
      /* copy from t .. u until end of path name */
      while (t != u+1) {
	if (*t == ';') {
	  *s++ = ';';
	  t++;
	}
	else {
	  *s++ = *t++;
	}
      }
      goto next;
    }
    /* we have an array beginning at u+1; find the end of it */
    v = u+2;
    while (*v && *v != '/' && *v != ']')
      v++;
    if (!*v || *v == '/') {
      while (t != v)
	*s++ = *t++;
      goto next;
    }
    /* we have an array from u+1 .. v  [ to ] */
    d1 = 0;
    x = w = t+1;
    while (w != u) {
      if (*w == ';') {
	if (d1 == 1) {
	  pp_printf (PPout, "WARNING: in name `%s'; too many dimensions!", s);
	  goto err;
	}
	*w = '\0';
	sscanf (x, "%d", &idx1[d1++]);
	x = w+1;
	*w = ';';
      }
      w++;
    }
    *w = '\0';
    sscanf (x, "%d", &idx1[d1++]);
    *w = ')';

    d2 = 0;
    x = w = u+2;
    while (w != v) {
      if (*w == ',') {
	if (d2 == 1) {
	  pp_printf (PPout, "WARNING: in name `%s'; too many dimensions!", s);
	  goto err;
	}
	*w = '\0';
	sscanf (x, "%d", &idx2[d2++]);
	x = w+1;
	*w = ',';
      }
      w++;
    }
    *w = '\0';
    sscanf (x, "%d", &idx2[d2++]);
    *w = ']';
    if (d2 != d1) {
      pp_printf (PPout, "WARNING: dimension mismatch in name `%s'", s);
      goto err;
    }
    *s++ = '[';
    for (d1 = 0; d1 < d2; d1++) {
      sprintf (buf, "%d", idx1[d1] + idx2[d1]);
      t = buf;
      while (*t)
	*s++ = *t++;
      if (d1 != d2-1)
	*s++ = ',';
    }
    *s++ = ']';
    t = v+1;
  }
  *s = '\0';
  return;

err:
  pp_forced (PPout, 0);
  while (*t)
    *s++ = *t++;
  return;
}

/*------------------------------------------------------------------------
 *
 * Strips trailing "!" in variable name
 *
 *------------------------------------------------------------------------
 */
static
int strip_trailing_bang (char *s)
{
  int l;
  l = strlen(s);
  if (s[l-1] == '!') {
    s[l-1] = '\0';
    return 1;
  }
  return 0;
}


/*------------------------------------------------------------------------
 *
 *  Turn name into global name
 *
 *  Returns 1 if actually a global name, 0 otherwise
 *
 *  Path separators are either / or .
 *
 *------------------------------------------------------------------------
 */
static
int make_global (char *s)
{
  int slash;
  int i, j;

  slash = -1;
  for (i=0; s[i] && s[i] != '!'; i++)
    if (s[i] == '/') slash = i+1;
  
  if (s[i] != '!') return 0;
  
  if (slash >= 0) {
    /* slash..i */
    for (j=0; j < i-slash+1; j++)
      s[j] = s[slash+j];
    i = j-1;
  }
  s[i+1] = '\0';
  return 1;
}

/*------------------------------------------------------------------------
 *
 *  Replace "/" with "." 
 *
 *------------------------------------------------------------------------
 */
static
void replace_slash_with_dot (char *s, dots_t *d)
{
  char *t;
  int count;
  static int dotpos[MAXDOTS];

  count = 0;
  t = s;
  while (*t) {
    if (*t == '.')
      dotpos[count++] = t-s;
    if (*t == '/') *t='.';
    t++;
  }
  create_dots (d, count, dotpos);
}

/*------------------------------------------------------------------------
 *
 *  Does format conversion on names
 *
 *  Also handles the -b option by connecting globals in the prs file if
 *  so desired.
 *
 *------------------------------------------------------------------------
 */
void name_convert (char *s, dots_t *d)
{
  char *t = NULL;
  int retval = 0;

  if (connect_globals)
    make_global (s);
  else if (connect_globals_in_prs && !print_only) {
    MALLOC (t, char, strlen (s)+1);
    strcpy (t, s);
    if (make_global (t))
      strip_trailing_bang (t);
    else {
      FREE (t);
      t = NULL;
    }
  }
  if (!dont_strip_bang)
    retval = strip_trailing_bang (s);
  array_fixup (s);
  if (use_dot_separator)
    replace_slash_with_dot (s, d);
  else
    d->ndots = 0;
  if (connect_globals_in_prs && t) {
    create_prs_connection (s,t);
    FREE (t);
    t = NULL;
  }
  d->globalnode = retval;
}

/*------------------------------------------------------------------------
 *
 *  Reconcile name conversion flags with existing ones for a variable
 *
 *------------------------------------------------------------------------
 */
void validate_name (var_t *v, dots_t *d)
{
  int flag = 0;

  if (v->name_convert.ndots == -2) return;
  merge_dots (&v->name_convert, d);
  if (v->name_convert.globalnode == -1)
    v->name_convert.globalnode = d->globalnode;
  else if (v->name_convert.globalnode != d->globalnode) {
    v->name_convert.ndots = -2;
    flag = 1;
  }
  if (v->name_convert.ndots == -2) {
    if (!flag)
      pp_printf (PPout, "ERROR: name validation failed for `%s' ",
		 var_string (v));
    else
      pp_printf (PPout, "ERROR: name `%s' appears with and without `!'",
		 var_string (v));
    pp_forced (PPout, 0);
    inc_naming_violations ();
    return;
  }
}


/*------------------------------------------------------------------------
 *
 *  Parse .sim file
 *
 *------------------------------------------------------------------------
 */
static
void lvs_parse_sim_line (VAR_T *V, char *line)
{
  var_t *v1, *v2, *v3, *v4;
  static char var1[MAXLINE], var2[MAXLINE], var3[MAXLINE];
  static char tok1[MAXLINE], tok2[MAXLINE], tok3[MAXLINE];
  char *gattr;
  int flag;
  int length, width;
  int weak;
  edgelist_t *e;
  int ret;
  dots_t d;
  double cval;

  if (line[0] == '=') {
    /* alias two nodes using union-find tree */
    sscanf (line+2, "%s%s", var1, var2);

    name_convert (var1, &d); v1 = var_enter (V, var1);
    validate_name (v1, &d);

    name_convert (var2, &d); v2 = var_enter (V, var2);
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
  else if (line[0] == 'n' || line[0] == 'p') {
    if (line[0] == 'n') 
      flag = N_TYPE;
    else 
      flag = P_TYPE;
    
    tok1[0] = '\0';
    tok2[0] = '\0';
    tok3[0] = '\0';
    sscanf (line+2, "%s%s%s%d%d%s%s%s", var1, var2, var3, &length, &width,
	    tok1, tok2, tok3);
    
    /* var1 = gate; var2, var3 are terminals */
    name_convert (var1, &d); v1 = var_enter (V, var1);
    validate_name (v1, &d);

    name_convert (var2, &d); v2 = var_enter (V, var2);
    validate_name (v2, &d);

    name_convert (var3, &d); v3 = var_enter (V, var3);
    validate_name (v3, &d);

    v1->inlayout = v2->inlayout = v3->inlayout = 1;

    if (tok1[0] == 'g' && tok1[1] == '=') gattr = tok1+2;
    else if (tok2[0] == 'g' && tok2[1] == '=') gattr = tok2+2;
    else gattr = tok3+2;
    
    if (strstr (gattr, "weak"))
      weak = 1;
    else
      weak = 0;
    
    /* add edge from v2 -> v3, and from v3 -> v2 */
    MALLOC (e, edgelist_t, 1);
    e->isweak = weak;
    e->gate = v1; e->t1 = v3;
    e->type = flag;
    if (length < 2) length = 2;
    e->length = length;
    e->width = width;
    e->next = v2->edges; v2->edges = e;
    
    MALLOC (e, edgelist_t, 1);
    e->isweak = weak;
    e->gate = v1; e->t1 = v2;
    e->type = flag;
    e->length = length;
    e->width = width;
    e->next = v3->edges; v3->edges = e;

#ifndef DIGITAL_ONLY
    /* add gate cap */
    if (e->type == P_TYPE) {
      v1->c.p_gA += (e->width*e->length);
      v1->c.p_gP += 2*(e->width+e->length);
    }
    else {
      v1->c.n_gA += (e->width*e->length);
      v1->c.n_gP += 2*(e->width+e->length);
    }
    /* the sim file doesn't tell us the right area/perim for diffusion;
       assume that the diffusion is a  3 by e->width rectangle,
       and add half the area/perim to the source and drain; if the
       layout is compact, this is about right for internal nodes,
       and it underestimates output capacitance. This means the program
       will be more conservative w.r.t. charge-sharing.

       In any case, why on earth would you use sim files? :)
    */
    if (e->type == P_TYPE) {
      v2->c.p_perim += (3+e->width)*lambda;
      v3->c.p_perim += (3+e->width)*lambda;
      v2->c.p_area  += 1.5*lambda*e->width*lambda;
      v3->c.p_area  += 1.5*lambda*e->width*lambda;
    }
    else {
      v2->c.n_perim += (3+e->width)*lambda;
      v3->c.n_perim += (3+e->width)*lambda;
      v2->c.n_area  += 1.5*lambda*e->width*lambda;
      v3->c.n_area  += 1.5*lambda*e->width*lambda;
    }
#if 0
    /* old cap estimate */
    v2->cap +=
      1.5*e->width*(flag == P_TYPE ? P_diff_cap : N_diff_cap);
    v3->cap += 
      1.5*e->width*(flag == P_TYPE ? P_diff_cap : N_diff_cap);
#endif
#endif

    /* if strong */
    if ((strip_by_width == 0 
	 && (double)e->width/(double)e->length >= strip_threshold) ||
	(strip_by_width == 1 && e->width >= width_threshold))
      v1->flags |= VAR_INPUT;
    
    if (debug_level > 60) {
      pp_printf (PPout, "edge: %s - %s - %s [%c,weak=%d] w=%d, l=%d", var_name(v2), 
		 var_name(v1), var_name(v3), flag ? 'p' : 'n', weak, width, length);
      pp_forced (PPout,0);
    }
  }
  else if (line[0] == 'A') {
    sscanf (line+2,"%s%s", var1, var2);
    name_convert (var1,&d); v1 = var_enter (V, var1);
    validate_name (v1,&d);
#define ATTR(a,b)  if (strcmp (var2,a) == 0) { v1->flags2 |= b; goto attrdone; }
#include "attr.def"
#undef ATTR
  attrdone:
    ;
  }
#if 0
  /* these C lines seem to be totally off... hmm... */
  else if (line[0] == 'C') {
    sscanf (line+2, "%s%s%g", var1, var2, &cval);
    name_convert (var1, &d); v1 = var_enter (V, var1);
    validate_name (v1, &d);

    name_convert (var2, &d); v2 = var_enter (V, var2);
    validate_name (v2, &d);

    v1->othercap += cval;
  }
#endif
  else
    ;
}



/*------------------------------------------------------------------------
 *
 *  Read .sim/.ext and .prs files, compare them
 *
 *------------------------------------------------------------------------
 */
void
lvs (char *name, FILE *sim, FILE *prs, FILE *aliases, FILE *dump)
{
  LEX_T *L; VAR_T *V;
  BOOL_T *B;
  static char buf[MAXLINE];
  int length, width;
  struct ext_file *ext;
  
  V = var_init ();

  /* parse all production rules for this file */
  if (!print_only) {
    L = lex_file (prs);
    parse_prs_input (L,V,0);
    lex_free (L);
  }

  /* parse extract/sim file */
  if (extract_file) {
    ext_validate_timestamp (name);
    //ext = parse_ext_file (sim, NULL, NULL);
    ext = ext_read (name);
    flatten_ext_file (ext, V);
  }
  else {
    /* parse sim file */
    buf[MAXLINE-1] = '\n';
    while (fgets (buf, MAXLINE, sim)) {
      if (buf[MAXLINE-1] == '\0')
	fatal_error ("This needs to be fixed!");      /* FIXME */
      lvs_parse_sim_line (V, buf);
    }
    fclose (sim);
    if (aliases) {
      buf[MAXLINE-1] = '\n';
      while (fgets (buf, MAXLINE, aliases)) {
	if (buf[MAXLINE-1] == '\0')
	  fatal_error ("This needs to be fixed!");      /* FIXME */
	lvs_parse_sim_line (V, buf);
      }
      fclose (aliases);
    }
  }
  pp_flush (PPout);

  B = bool_init ();

  gen_prs (V, B);
  if (print_only)
    print_prs (V, B);
  else {
    if (only_check_connects)
      Assert (wizard, "Huh?");
    else
      check_prs (V, B);
  }

  if (pr_aliases)
    print_aliases (V);

  if (dump_hier_file && (exit_status == 0 || generate_summary_anyway() || wizard))
    save_io_nodes (V, dump, ext->timestamp);
}
