/*************************************************************************
 *
 *  Parser generator
 *
 *  Copyright (c) 2003-2011, 2018, 2019 Rajit Manohar
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
#include "pgen.h"

int is_user_ret_ptr (bnf_item_t *b);

#define WNAME  WALK[walk_id]
#define WCOOKIE cookie_type[walk_id]
#define WRET    return_type[walk_id]

extern int walk_id;
extern char *prefix;

int opt_token_no_data (token_type_t *);

/*
  Print body of an apply function, translating $xx to the appropriate
  thing.
*/
void print_munged_string (pp_t *pp, char *s, char *file, int line)
{
  char out[2];
  int i;
  char c;
  char *t;

  out[1] = '\0';

  pp_forced (pp, 0);
  pp_printf_raw (pp, "# %d \"%s\"", line, file);
  pp_forced (pp, 0);
  
  while (*s) {
    if (*s != '$') {
      out[0] = *s;
      if (*s == '\n') 
	pp_forced (pp, 0);
      else {
	pp_puts_raw (pp, out);
      }
      s++;
    }
    else if (*(s+1) == '$') {
      out[0] = *s;
      pp_puts_raw (pp, out);
      s += 2;
    }
    else {
      s++;
      t = s;
      c = *s;
      if (c == 'W' || c == 'E' || c == 'A' || c == 'e') {
	s++;
	if (*s == '(') {
	  if (c == 'A') {
	    pp_printf (pp, "my_Assert(");
	  }
	  else if (c == 'E' || c == 'W') {
	    pp_printf (pp, "%s_parse_%s (&curpos,", prefix, c == 'W' ? "warn" : "err");
	  }
	  else if (c == 'e') {
	    pp_printf (pp, "%s_parse_msg (&curpos,", prefix);
	  }
	  s++;
	  continue;
	}
	else {
	  s--;
	}
      }
      else if (c == 'f') {
	pp_printf (pp, "stderr");
	s++;
	continue;
      }
      else if (c == 'l') {
	pp_printf (pp, "(curpos.l)");
	s++;
	continue;
      }
      else if (c == 'c') {
	pp_printf (pp, "(curpos.c)");
	s++;
	continue;
      }
      else if (c == 'n') {
	pp_printf (pp, "(curpos.f)");
	s++;
	continue;
      }
      else if (c < '0' || c > '9') {
	s++;
	pp_printf (pp, "$%c", c);
	continue;
      }
      while (*s && (*s >= '0' && *s <= '9')) {
	s++;
      }
      c = *s;
      *s = '\0';
      if (s == t) {
	out[0] = '$';
	out[1] = '\0';
	pp_puts_raw (pp, out);
      }
      else {
	i = atoi (t);
	*s = c;
	pp_printf (pp, "arg%d", i);
      }
    }
  }
}



/*
  Standard prolog for a walker
*/
void print_header_prolog (pp_t *pp)
{
  char buf[1024];
  FILE *fp;
  int i;

  pp_printf (pp, "#ifndef __%s_WALK_%s_H__", prefix, WNAME);
  pp_forced (pp, 0);
  pp_printf (pp, "#define __%s_WALK_%s_H__", prefix, WNAME);
  pp_forced (pp, 0);
  
  pp_printf (pp, "#include \"%s_parse.h\"", prefix);
  pp_forced (pp, 0);
  sprintf (buf, "%s_walk.extra.h", prefix);
  if ((fp = fopen (buf, "r"))) {
    pp_printf (pp, "#include \"%s\"", buf); pp_forced (pp, 0);
    fclose (fp);
  }
  pp_printf (pp, "%s %s_walk_%s (%s *, %s_Token *);", 
	     user_ret_id (0), prefix, WNAME, WCOOKIE, prefix);
  pp_forced (pp, 0);

  if (found_expr) {
    pp_printf (pp, "%s *%s_wrap_%s_expr (Expr *);", WRET, prefix, WNAME);
    pp_forced (pp,0);
  }
  if (A_LEN (EXTERN_P) > 0) {
    pp_printf (pp, "%s *%s_wrap_%s_extern (void *);", WRET, prefix, WNAME);
    pp_nl;
  }
  pp_printf (pp, "%s *%s_wrap_%s_string (const char *);", WRET, prefix, WNAME);
  pp_forced (pp,0);
  pp_printf (pp, "%s *%s_wrap_%s_int (int);", WRET, prefix, WNAME);
  pp_forced (pp,0);
  pp_printf (pp, "%s *%s_wrap_%s_double (double);", WRET, prefix, WNAME);
  pp_forced (pp,0);
  pp_printf (pp, "%s *%s_wrap_%s_list (list_t *);", WRET, prefix, WNAME);
  pp_forced (pp,0);

  for (i=0; i < A_LEN (EXTERN_P); i++) {
    pp_printf (pp, "void *%s_walk_%s_%s (%s *cookie, void *v);",
	       prefix, WNAME, EXTERN_P[i], WCOOKIE);
    pp_nl;
  }
}


/*
  Print standard prolog for the walker
*/
void print_walker_prolog (pp_t *pp)
{
  int i, j, k;

  pp_printf (pp, "#include <stdio.h>"); pp_nl;
  pp_printf (pp, "#include <stdlib.h>"); pp_nl;
  pp_printf (pp, "#include <common/except.h>"); pp_nl;
  pp_printf (pp, "#include \"%s_parse.h\"", prefix); pp_nl;
  pp_printf (pp, "#include \"%s_parse_int.h\"", prefix); pp_nl;
  pp_printf (pp, "#include \"%s_walk_%s.h\"", prefix, WNAME); pp_nl;
  pp_puts (pp, "#define my_Assert(a) do { if (!(a)) { fprintf (stderr, \"Assertion failed, file %s, line %d\\n\", __FILE__, __LINE__); fprintf (stderr, \"Assertion: \" #a \"\\n\"); exit (4); } } while (0)");
  pp_nl;
  pp_printf (pp, "typedef struct %s_DefToken Token;", prefix);
  pp_nl;

  pp_printf (pp, "static struct %s_position curpos;", prefix);
  pp_nl;

  for (i=0; i < A_LEN (BNF); i++) {
    for (j=0; j < A_LEN (BNF[i].a); j++) {
      pp_printf (pp, "static %s apply_%s_%s_opt%d (%s * cookie", user_ret_id (i),
		 WNAME, BNF[i].lhs, j, WCOOKIE);
      for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	if (HAS_DATA (BNF[i].a[j].a[k])) {
	  pp_puts (pp, ", ");
	  
	  pp_printf (pp, "%s", production_to_ret_type (&BNF[i].a[j].a[k]));
	}
      }
      pp_puts (pp, ");");
      pp_nl;
    }
    pp_nl;
  }

  /* 
     Add prototypes for the walk functions; make the expression walk
     functions public.
  */
  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf (pp, "static %s walk_%s_%s (%s *cookie, Node_%s *n);", 
	       user_ret_id (i), WNAME, BNF[i].lhs, WCOOKIE, BNF[i].lhs);
    if (strcmp (BNF[i].lhs, "expr_id") == 0) {
      pp_printf (pp, "%s %s_walk_%s_%s (%s *cookie, Node_%s *n) {", 
		 user_ret_id (i), prefix, WNAME, BNF[i].lhs, WCOOKIE, BNF[i].lhs);
      pp_printf (pp, "return walk_%s_%s (cookie, n); }", WNAME, BNF[i].lhs);
    }
    pp_nl;
    pp_nl;
  }
  pp_nl;
  pp_printf (pp, "static %s *walkmap_%s (%s *cookie, Token *t);", 
	     WRET, WNAME, WCOOKIE); 
  pp_nl; pp_nl;
}


/*
  Top-level walk function.

  Returns a default return type, wrapping the types as necessary.
*/
void print_walker_main (pp_t *pp)
{
  int i;
  char buf[1024];
  char tmp[10];

  pp_printf (pp, "%s %s_walk_%s (%s *cookie, Token *t)", 
	     user_ret_id (0), prefix, WNAME, WCOOKIE);
  pp_nl;
  pp_puts (pp, "{");

  BEGIN_INDENT;

  pp_puts (pp, "switch (t->type) {");
  BEGIN_INDENT;

  /* all the user-defined stuff */
  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf (pp, "case %d: return %s(walk_%s_%s(cookie, t->u.Tok_%s.n0)); break;",
	       i, wrapper_name (i), WNAME, BNF[i].lhs, BNF[i].lhs);
    pp_nl;
  }
  
  /* shared prefix for all wrap functions */
  sprintf (buf, "%s_wrap_%s", prefix, WNAME);

  pp_printf (pp, "case %d: return %s_string (t->u.Tok_ID.n0); break;", 
	     A_LEN (BNF) + Tok_ID_offset, buf);  pp_nl;
  pp_printf (pp, "case %d: return %s_string (t->u.Tok_STRING.n0); break;", 
	     A_LEN (BNF) + Tok_STRING_offset, buf); pp_nl;
  pp_printf (pp, "case %d: return %s_double(t->u.Tok_FLOAT.n0); break;", 
	     A_LEN (BNF) + Tok_FLOAT_offset, buf); pp_nl;
  pp_printf (pp, "case %d: return %s_int(t->u.Tok_INT.n0); break;", 
	     A_LEN (BNF) + Tok_INT_offset, buf); pp_nl;
  pp_printf (pp, "case %d: return %s_list (list_map_cookie (t->u.Tok_OptList.n0, cookie, (LISTMAPFNCOOKIE) walkmap_%s)); break;", A_LEN (BNF) + Tok_OptList_offset, buf, WNAME); pp_nl; 
  pp_printf (pp, "case %d: return %s_list (list_map_cookie (t->u.Tok_SeqList.n0, cookie, (LISTMAPFNCOOKIE) walkmap_%s)); break;", A_LEN (BNF) + Tok_SeqList_offset, buf, WNAME); pp_nl; 
  if (A_LEN (EXTERN_P) > 0) {
    pp_printf (pp, "case %d: return %s_extern(t->u.Tok_EXTERN.n0); break;", 
	       A_LEN (BNF) + Tok_EXTERN_offset, buf); pp_nl;
  }

  /* built-in types */
  if (found_expr) {
    pp_printf (pp, "case %d: return %s_expr (t->u.Tok_expr.n0); break;", 
	       A_LEN (BNF) + Tok_expr_offset, buf);
    pp_nl;
  }

  pp_printf (pp, "default: fatal_error (\"invalid case\"); break;");
  pp_nl;

  END_INDENT;
  pp_puts (pp, "}");
  pp_nl;
  pp_puts (pp, "exit (1);");
  pp_nl;
  END_INDENT;
  pp_puts (pp, "}");
  pp_nl;
  pp_nl;
  pp_nl;

  /* helper functions required for the list_map above */
  pp_printf (pp, "static %s *walkmap_%s (%s *cookie, Token *t) { return %s_walk_%s(cookie, (Token *)t); }", WRET, WNAME, WCOOKIE, prefix, WNAME);
  pp_nl; pp_nl;

  /* helper functions for special walkmaps */
    
  if (found_expr) {
    pp_printf (pp, "static Expr *walkmap_%s_expr (%s *cookie, Token *t) { switch (t->type) { case %d: return t->u.Tok_expr.n0; break; default: THROW (EXC_NULL_EXCEPTION); return NULL; break; } }", WNAME, WCOOKIE, A_LEN (BNF) + Tok_expr_offset); pp_nl;
  }

  pp_printf (pp, "static const char *walkmap_%s_string (%s *cookie, Token *t) { switch (t->type) { case %d: return t->u.Tok_STRING.n0; break; case %d: return t->u.Tok_ID.n0; break; default: THROW (EXC_NULL_EXCEPTION); return NULL; break; } }", WNAME, WCOOKIE, A_LEN (BNF) + Tok_STRING_offset, 
	     A_LEN (BNF) + Tok_ID_offset); pp_nl;

  pp_printf (pp, "static double *walkmap_%s_double (%s *cookie, Token *t) { switch (t->type) { case %d: return &t->u.Tok_FLOAT.n0; break; default: THROW (EXC_NULL_EXCEPTION); return NULL; break; } }", WNAME, WCOOKIE, A_LEN (BNF) + Tok_FLOAT_offset); pp_nl;

  pp_printf (pp, "static int *walkmap_%s_int (%s *cookie, Token *t) { switch (t->type) { case %d: return &t->u.Tok_INT.n0; break; default: THROW(EXC_NULL_EXCEPTION); return NULL; break; } }", WNAME, WCOOKIE, A_LEN (BNF) + Tok_INT_offset); pp_nl;

  if (A_LEN (EXTERN_P) > 0) {
    pp_printf (pp, "static void *walkmap_%s_extern (%s *cookie, Token *t) { switch (t->type) { case %d: return t->u.Tok_EXTERN.n0; break; default: THROW(EXC_NULL_EXCEPTION); return NULL; break; } }", WNAME, WCOOKIE, A_LEN (BNF) + Tok_EXTERN_offset); pp_nl;
  }

  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf (pp, "static %s *walkmap_%s_%s (%s *cookie, Token *t) { ",
	       special_user_ret_id(i), WNAME, BNF[i].lhs, WCOOKIE);
    pp_printf (pp, "switch (t->type) { case %d: ", i);
    if (BNF[i].lhs_ret) {
      if (BNF[i].lhs_ret_p == 0 || BNF[i].lhs_ret_p == 3) {
	/* normal, non-pointer stuff. Convert to a pointer */
	pp_printf (pp, "{ %s *x; NEW (x, %s); *x=walk_%s_%s(cookie, ",
		   user_ret_id(i), user_ret_id(i), WNAME, BNF[i].lhs);
	pp_printf (pp, "t->u.Tok_%s.n0); return x; } break;", BNF[i].lhs);
      }
      else {
	/* pointers, don't have to do anything here */
	pp_printf (pp, " return walk_%s_%s (cookie, ",
		   WNAME, BNF[i].lhs);
	pp_printf (pp, "t->u.Tok_%s.n0); break;", BNF[i].lhs);
      }
    }
    else {
      pp_printf (pp, " return walk_%s_%s (cookie, ", WNAME, BNF[i].lhs);
      pp_printf (pp, "t->u.Tok_%s.n0); break; ", BNF[i].lhs);
    }
    pp_printf (pp, "default: THROW (EXC_NULL_EXCEPTION); break; } return NULL; }");
    pp_nl; pp_nl;
  }
  pp_nl;
  pp_nl;
}

static void print_trec_helper_body (pp_t *pp, pp_t *app, bnf_item_t *bnf, int j)
{
  token_list_t *tl;
  bnf_item_t *b;
  int k = 0;

  /* base case */
  tl = &bnf->a[j];

  if (HAS_DATA(tl->a[k])) {
    pp_printf (pp, "%s f%d = n->u.Option_%s%d.f%d;",
	       tok_type_to_parser_type (&tl->a[k]), k, bnf->lhs, j, k);
  }
  pp_nl;

  /* Part 2: Convert non-built-in types recursively, replacing
     f# with g# for those.
  */
  if (HAS_DATA (tl->a[k])) {
    switch (tl->a[k].type) {
    case T_L_EXPR: case T_L_IEXPR: case T_L_BEXPR: case T_L_REXPR:
    case T_L_ID: case T_L_STRING: case T_L_FLOAT:
    case T_L_INT:
      break;

    case T_LHS:
      b = (bnf_item_t *)tl->a[k].toks;
      pp_printf (pp, "%s g%d = walk_%s_%s (cookie, f%d);", 
		 user_ret (b), k, WNAME, b->lhs, k);
      pp_nl;
      break;

    case T_EXTERN:
      pp_printf (pp, "void *g%d = %s_walk_%s_%s (cookie, f%d);",
		 k, prefix, WNAME, tl->a[k].toks, k);
      pp_nl;
      break;

    case T_OPT:
    case T_LIST:
      if (opt_token_no_data (&tl->a[k])) {
	pp_printf (pp, "%s g%d = list_new ();", 
		   production_to_ret_type (&tl->a[k]), k);
	pp_printf (pp, "if (!list_isempty (f%d)) ", k);
	pp_printf (pp, "list_append (g%d, NULL);", k);
	pp_nl;
      }
      else {
	pp_printf (pp, "%s g%d = list_map_cookie " 
		   "(f%d, cookie, %s walkmap_%s);", 
		   production_to_ret_type (&tl->a[k]), k, k, 
		   "(LISTMAPFNCOOKIE)", WNAME);
	pp_nl;
      }
      break;

    case T_LIST_SPECIAL:
      pp_printf (pp, "%s g%d = list_map_cookie "
		 "(f%d, cookie, %s %s);", 
		 production_to_ret_type (&tl->a[k]), 
		 k, k, 
		 "(LISTMAPFNCOOKIE)",
		 special_wrapper_name (&tl->a[k]), 
		 WNAME);
      break;
    default:
      fatal_error ("Uh oh");
      break;
    }
  }
  pp_printf (pp, "return g%d;", k);
  pp_nl;
}

static void print_trec_helper (pp_t *pp, pp_t *app, bnf_item_t *bnf)
{
  
  pp_printf (pp, "static %s trec_walk_%s_%s (%s *cookie, Node_%s *n)",
	     user_ret (bnf), WNAME, bnf->lhs, WCOOKIE, bnf->lhs);
  pp_nl;
  pp_printf (pp, "{");
  BEGIN_INDENT;

  pp_printf (pp, "if (n->type == 1) {");
  pp_nl;
  print_trec_helper_body (pp, app, bnf, 1);

  pp_printf (pp, "}");
  pp_printf (pp, "else {");
  pp_nl;
  print_trec_helper_body (pp, app, bnf, 0);
  pp_printf (pp, "}");
  
  END_INDENT;
  pp_puts (pp, "}");
  pp_nl;
  pp_nl;
}

void print_walker_recursive (pp_t *pp, pp_t *app)
{
  int i, j, k, l;
  int flag;
  token_list_t *tl;
  bnf_item_t *b;

  /*
    Recursive call to walk the entire parse tree
  */
  for (i=0; i < A_LEN (BNF); i++) {

#if 0 
    if (BNF[i].tail_recursive) {
      print_trec_helper (pp, app, &BNF[i]);
    }
#endif

    /* Function that implements  the walker for  BNF[i] */
    pp_printf (pp, "static %s walk_%s_%s (%s *cookie, Node_%s *n)", 
	       user_ret (&BNF[i]), WNAME, BNF[i].lhs, WCOOKIE, BNF[i].lhs);
    pp_nl;
    pp_printf (pp, "{");
    BEGIN_INDENT;

#if 0
    if (BNF[i].tail_recursive) {
      pp_printf (pp, "if (n->type == 1) {");
      j = 1;
      /* base case */
      tl = &BNF[i].a[j];
      for (k=0; k < A_LEN (tl->a); k++) {
	if (HAS_DATA(tl->a[k])) {
	  pp_printf (pp, "%s f%d = n->u.Option_%s%d.f%d;", tok_type_to_parser_type (&tl->a[k]), k, BNF[i].lhs, j, k);
	}
      }
      BEGIN_INDENT;

      /* Part 2: Convert non-built-in types recursively, replacing
	         f# with g# for those.
      */
      for (k=0; k < A_LEN (tl->a); k++) {
	if (HAS_DATA (tl->a[k])) {
	  switch (tl->a[k].type) {
	  case T_L_EXPR: case T_L_IEXPR: case T_L_BEXPR: case T_L_REXPR:
	  case T_L_ID: case T_L_STRING: case T_L_FLOAT:
	  case T_L_INT:
	    break;

	  case T_LHS:
	    b = (bnf_item_t *)tl->a[k].toks;
	    pp_printf (pp, "%s g%d = walk_%s_%s (cookie, f%d);", 
		       user_ret (b), k, WNAME, b->lhs, k);
	    pp_nl;
	    break;

	  case T_EXTERN:
	    pp_printf (pp, "void *g%d = %s_walk_%s_%s (cookie, f%d);",
		       k, prefix, WNAME, tl->a[k].toks, k);
	    pp_nl;
	    break;

	  case T_OPT:
	  case T_LIST:
	    if (opt_token_no_data (&tl->a[k])) {
	      pp_printf (pp, "%s g%d = list_new ();", 
			 production_to_ret_type (&tl->a[k]), k);
	      pp_printf (pp, "if (!list_isempty (f%d)) ", k);
	      pp_printf (pp, "list_append (g%d, NULL);", k);
	      pp_nl;
	    }
	    else {
	      pp_printf (pp, "%s g%d = list_map_cookie " 
			 "(f%d, cookie, %s walkmap_%s);", 
			 production_to_ret_type (&tl->a[k]), k, k, 
			 "(LISTMAPFNCOOKIE)", WNAME);
	      pp_nl;
	    }
	    break;

	  case T_LIST_SPECIAL:
	    pp_printf (pp, "%s g%d = list_map_cookie "
		       "(f%d, cookie, %s %s);", 
		       production_to_ret_type (&tl->a[k]), 
		       k, k, 
		       "(LISTMAPFNCOOKIE)",
		       special_wrapper_name (&tl->a[k]), 
		       WNAME);
	    break;
	  default:
	    fatal_error ("Uh oh");
	    break;
	  }
	}

	/* Emit local apply function, if necessary */
	if (tl->a[k].opt_next && (k != A_LEN(tl->a)-1) &&
	    tl->a[k].opt_next[walk_id].s) {
	  /* Specified walker for a subset of the entire production; 
	     Create the local apply function
	  */
	  pp_puts (pp, "curpos = n->p;"); pp_nl;
	  pp_printf (pp, "lapply_%s_%s_%d_%d (cookie", WNAME, 
		     BNF[i].lhs, j, k);
	  pp_printf (app, "void lapply_%s_%s_%d_%d (%s *", WNAME,
		     BNF[i].lhs, j, k, WCOOKIE);
	  for (l=0; l <= k; l++) {
	    if (HAS_DATA (tl->a[l])) {
	      pp_printf (app, ", %s", production_to_ret_type (&tl->a[l]));
	      switch (tl->a[l].type) {
	      case T_L_EXPR: case T_L_IEXPR: case T_L_BEXPR: case T_L_REXPR:
	      case T_L_ID: case T_L_STRING: case T_L_FLOAT: case T_L_INT:
		pp_printf (pp, ", f%d", l);
		break;
	      case T_LHS: case T_OPT:
	      case T_LIST: case T_LIST_SPECIAL: case T_EXTERN:
		pp_printf (pp, ", g%d", l);
		break;
	      default:
		fatal_error ("Uh oh");
		break;
	      }
	    }
	  }
	  pp_puts (pp, ");"); pp_nl;
	  pp_puts (app, ");"); pp_forced (app, 0);
	}
      }

      /* Part 3: Call apply_... */
      pp_puts (pp, "curpos = n->p;"); pp_nl;
      pp_printf (pp, "return apply_%s_%s_opt%d (cookie", WNAME, BNF[i].lhs, j);

      /* args to apply */
      for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	if (HAS_DATA (BNF[i].a[j].a[k])) {
	  switch (BNF[i].a[j].a[k].type) {
	  case T_L_EXPR:
	  case T_L_IEXPR:
	  case T_L_BEXPR:
	  case T_L_REXPR:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_L_ID:
	  case T_L_STRING:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_L_FLOAT:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_L_INT:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_LHS:
	  case T_EXTERN:
	    pp_printf (pp, ", g%d", k);
	    break;
	  case T_OPT:
	  case T_LIST:
	  case T_LIST_SPECIAL:
	    pp_printf (pp, ", g%d", k);
	    break;
	  default:
	    fatal_error ("Uh oh");
	    break;
	  }
	}
      }
      pp_puts (pp, ");"); pp_nl;
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      pp_printf (pp, "else { ");
      BEGIN_INDENT;

      /*-- reverse list --*/
      
      

      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      goto finish;
    }
#endif

    pp_printf (pp, "switch (n->type)");
    pp_nl;
    pp_puts (pp, "{");
    BEGIN_INDENT;

    for (j=0; j < A_LEN (BNF[i].a); j++) {
      /* Option j for BNF[i] */
      tl = &BNF[i].a[j];

      /* Part 1: Emit

          case &Option_... (f#,..,f#):
      */
      pp_printf (pp, "case %d: {", j);
      /* unpack structure */
      for (k=0; k < A_LEN (tl->a); k++) {
	if (HAS_DATA(tl->a[k])) {
	  pp_printf (pp, "%s f%d = n->u.Option_%s%d.f%d;", tok_type_to_parser_type (&tl->a[k]), k, BNF[i].lhs, j, k);
	}
      }
      BEGIN_INDENT;

      /* Part 2: Convert non-built-in types recursively, replacing
	         f# with g# for those.
      */
      for (k=0; k < A_LEN (tl->a); k++) {
	if (HAS_DATA (tl->a[k])) {
	  switch (tl->a[k].type) {
	  case T_L_EXPR: case T_L_IEXPR: case T_L_BEXPR: case T_L_REXPR:
	  case T_L_ID: case T_L_STRING: case T_L_FLOAT:
	  case T_L_INT:
	    break;

	  case T_LHS:
	    b = (bnf_item_t *)tl->a[k].toks;
	    pp_printf (pp, "%s g%d = walk_%s_%s (cookie, f%d);", 
		       user_ret (b), k, WNAME, b->lhs, k);
	    pp_nl;
	    break;

	  case T_EXTERN:
	    pp_printf (pp, "void *g%d = %s_walk_%s_%s (cookie, f%d);",
		       k, prefix, WNAME, tl->a[k].toks, k);
	    pp_nl;
	    break;

	  case T_OPT:
	  case T_LIST:
	    if (opt_token_no_data (&tl->a[k])) {
	      pp_printf (pp, "%s g%d = list_new ();", 
			 production_to_ret_type (&tl->a[k]), k);
	      pp_printf (pp, "if (!list_isempty (f%d)) ", k);
	      pp_printf (pp, "list_append (g%d, NULL);", k);
	      pp_nl;
	    }
	    else {
	      pp_printf (pp, "%s g%d = list_map_cookie " 
			 "(f%d, cookie, %s walkmap_%s);", 
			 production_to_ret_type (&tl->a[k]), k, k, 
			 "(LISTMAPFNCOOKIE)", WNAME);
	      pp_nl;
	    }
	    break;

	  case T_LIST_SPECIAL:
	    pp_printf (pp, "%s g%d = list_map_cookie "
		       "(f%d, cookie, %s %s);", 
		       production_to_ret_type (&tl->a[k]), 
		       k, k, 
		       "(LISTMAPFNCOOKIE)",
		       special_wrapper_name (&tl->a[k]), 
		       WNAME);
	    break;
	  default:
	    fatal_error ("Uh oh");
	    break;
	  }
	}

	/* Emit local apply function, if necessary */
	if (tl->a[k].opt_next && (k != A_LEN(tl->a)-1) &&
	    tl->a[k].opt_next[walk_id].s) {
          int l;
	  /* Specified walker for a subset of the entire production; 
	     Create the local apply function
	  */
	  pp_puts (pp, "curpos = n->p;"); pp_nl;
	  pp_printf (pp, "lapply_%s_%s_%d_%d (cookie", WNAME, 
		     BNF[i].lhs, j, k);
	  pp_printf (app, "void lapply_%s_%s_%d_%d (%s *", WNAME,
		     BNF[i].lhs, j, k, WCOOKIE);
	  for (l=0; l <= k; l++) {
	    if (HAS_DATA (tl->a[l])) {
	      pp_printf (app, ", %s", production_to_ret_type (&tl->a[l]));
	      switch (tl->a[l].type) {
	      case T_L_EXPR: case T_L_IEXPR: case T_L_BEXPR: case T_L_REXPR:
	      case T_L_ID: case T_L_STRING: case T_L_FLOAT: case T_L_INT:
		pp_printf (pp, ", f%d", l);
		break;
	      case T_LHS: case T_OPT:
	      case T_LIST: case T_LIST_SPECIAL: case T_EXTERN:
		pp_printf (pp, ", g%d", l);
		break;
	      default:
		fatal_error ("Uh oh");
		break;
	      }
	    }
	  }
	  pp_puts (pp, ");"); pp_nl;
	  pp_puts (app, ");"); pp_forced (app, 0);
	}
      }

      /* Part 3: Call apply_... */
      pp_puts (pp, "curpos = n->p;"); pp_nl;
      pp_printf (pp, "return apply_%s_%s_opt%d (cookie", WNAME, BNF[i].lhs, j);

      /* args to apply */
      for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	if (HAS_DATA (BNF[i].a[j].a[k])) {
	  switch (BNF[i].a[j].a[k].type) {
	  case T_L_EXPR:
	  case T_L_IEXPR:
	  case T_L_BEXPR:
	  case T_L_REXPR:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_L_ID:
	  case T_L_STRING:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_L_FLOAT:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_L_INT:
	    pp_printf (pp, ", f%d", k);
	    break;
	  case T_LHS:
	  case T_EXTERN:
	    pp_printf (pp, ", g%d", k);
	    break;
	  case T_OPT:
	  case T_LIST:
	  case T_LIST_SPECIAL:
	    pp_printf (pp, ", g%d", k);
	    break;
	  default:
	    fatal_error ("Uh oh");
	    break;
	  }
	}
      }
      pp_puts (pp, ");"); pp_nl;
      pp_puts (pp, "}"); pp_nl;
      pp_printf (pp, "break;");
      END_INDENT;
    }

    END_INDENT;
    pp_puts (pp, "}"); pp_nl;

  finish:
    pp_puts (pp, "THROW (EXC_NULL_EXCEPTION);");
    /* check user ret */
    if (is_user_ret_ptr (&BNF[i])) {
      pp_nl; pp_puts (pp, "return NULL;");
    }
    pp_puts (pp, "exit (1);");
    pp_nl;
    END_INDENT;
    pp_puts (pp, "}");
    pp_nl;
    pp_nl;
  }
}


void print_walker_apply_fns (pp_t *pp)
{
  int i, j, k;
  token_list_t *tl;

  /* emit apply/opt0 functions */
  for (i=0; i < A_LEN (BNF); i++) {
    for (j=0; j < A_LEN (BNF[i].a); j++) {
      tl = &BNF[i].a[j];

      /* prototype */
      pp_printf (pp, "static %s apply_%s_%s_opt%d (%s *arg0", 
		 user_ret_id(i), WNAME, BNF[i].lhs, j, WCOOKIE);
      for (k=0; k < A_LEN (tl->a); k++) {
	if (HAS_DATA (tl->a[k])) {
	  pp_printf (pp, ", %s arg%d", production_to_ret_type (&tl->a[k]),k+1);
	}
      }
      pp_puts (pp, ")");
      pp_nl;
      pp_puts (pp, "{");

      if (k == 0 || !tl->a[k-1].opt_next || !tl->a[k-1].opt_next[walk_id].s) {
	extern int verbose;
	if (verbose)
	  warning ("Walk type `%s': using default NULL walker for `%s,' "
		   "option %d", WNAME, BNF[i].lhs, j);
	pp_puts (pp, "return NULL;"); pp_nl;
      }
      else {
	print_munged_string (pp, tl->a[k-1].opt_next[walk_id].s,
			     tl->a[k-1].opt_next[walk_id].file,
			     tl->a[k-1].opt_next[walk_id].line);
      }
      pp_nl;
      pp_puts (pp, "}");
      pp_nl;
    }
    pp_nl;
    pp_nl;
  }
}


void print_walker_local_apply_fns (pp_t *pp)
{
  int i, j, k, l;
  token_list_t *tl;

  for (i=0; i < A_LEN (BNF); i++) {
    for (j=0; j < A_LEN (BNF[i].a); j++) {
      tl = &BNF[i].a[j];
      for (k=0; k < A_LEN (tl->a)-1; k++) {
	if (tl->a[k].opt_next && tl->a[k].opt_next[walk_id].s) {
	  pp_printf (pp, "void lapply_%s_%s_%d_%d (%s *arg0", WNAME,
		     BNF[i].lhs, j, k, WCOOKIE);
	  /* grab args upto this point */
	  for (l=0; l <= k; l++) {
	    if (HAS_DATA (tl->a[l])) {
	      pp_printf(pp,", %s arg%d",production_to_ret_type(&tl->a[l]),l+1);
	    }
	  }
	  pp_puts (pp, ")");
	  pp_nl;
	  pp_puts (pp, "{");
	  print_munged_string (pp, tl->a[k].opt_next[walk_id].s,
			       tl->a[k].opt_next[walk_id].file,
			       tl->a[k].opt_next[walk_id].line);
	  pp_nl;
	  pp_puts (pp, "}");
	  pp_nl;
	}
      }
    }
  }
}
