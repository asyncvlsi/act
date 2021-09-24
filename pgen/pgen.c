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
#include <ctype.h>
#include <string.h>
#include <common/lex.h>

#include "pgen.h"


/*
  LHS: RHS | RHS ... | RHS ;

  RHS can be:
          keyword  gets turned into a keyword
	  ID
	  INT
	  FLOAT
	  STRING
	  expr

	  EXTERN[name]
	     ... need extern parsers and walkers
	     ... prefix_is_a_<name>
	     ... prefix_parse_a_<name>
	     ... prefix_walk_X_<name>
	  
	  [ foo ]   = optional foo
	  { foo last }** = foo { last foo }*

  After any thing you can say
     {{ code }}

ONLY PROBLEM:
  for the top-level, don't have ambiguous productions.

*/

/*
  This is gross. A single file. But what the heck...
  
  - save away the productions in a table. 
  - table entry:
          char *lhs; [ lhs of the production ]
	  list_t *hd, *tl; [ list of rhs options ]
	  each rhs option is a list of token_items
	    token_item:
	          built-in
		  keyword/token
		  production id  [--- needs to be backpatched]
		  optional_production
		  sep item for list-style production
*/


char pointer_types[] = { '?', '@', '*' };


static bitset_t **sets; /* bitsets */
static int tot_options, tok_offset, ext_offset;

/* extern functions */
A_DECL(char *, EXTERN_P);

/* array of bnf items */
A_DECL(bnf_item_t, BNF);

/* aux parser values */
static bnf_item_t *curBNF;
static token_list_t *curLIST;
static token_type_t *curTOKEN;


/* token table */
A_DECL(char *, TT);

char *prefix = "std";

/* walk types */
A_DECL(char *, WALK);
A_DECL(char *, cookie_type);
A_DECL(char *, return_type);

int gen_parse;
int verbose;
int verilog_ids;
int hexdigit;
int bindigit;

A_DECL(char *, GWALK);


static int extern_add (char *s)
{
  int i;

  for (i=0; i < A_LEN (EXTERN_P); i++) {
    if (strcmp (EXTERN_P[i], s) == 0) 
      return i;
  }
  A_NEWM (EXTERN_P, char *);
  A_NEXT (EXTERN_P) = Strdup (s);
  A_INC (EXTERN_P);
  return A_LEN (EXTERN_P)-1;
}

/*
  Tokens
*/
#define TOKEN(a,b) static int a;
#include "pgen.def"

static int walk_lookup (char *s)
{
  int i;

  for (i=0; i < A_LEN (WALK); i++)
    if (strcmp (WALK[i], s) == 0) return i;
  return -1;
}

static int bnf_item_to_num (bnf_item_t *b)
{
  return (((char *)b) - (char *)BNF)/sizeof (bnf_item_t);
}  


/*
  ========================================================================
    P a r s e r
  ========================================================================
*/
void base_parse_token_list (LEX_T *l, token_list_t *t);

int found_expr;

int parse_token (LEX_T *l)
{
  token_list_t *tmp, *save;

  /* keywords */
  if (lex_have (l, EXPR)) {
    found_expr = 1;
    curTOKEN->type = T_L_EXPR;
    return 1;
  }
  else if (lex_have (l, BEXPR)) {
    found_expr = 1;
    curTOKEN->type = T_L_BEXPR;
    return 1;
  }
  else if (lex_have (l, IEXPR)) {
    found_expr = 1;
    curTOKEN->type = T_L_IEXPR;
    return 1;
  }
  else if (lex_have (l, REXPR)) {
    found_expr = 1;
    curTOKEN->type = T_L_REXPR;
    return 1;
  }
  else if (lex_have (l, ID)) {
    curTOKEN->type = T_L_ID;
    return 1;
  }
  else if (lex_have (l, STRING)) {
    curTOKEN->type = T_L_STRING;
    return 1;
  }
  else if (lex_have (l, FLOAT)) {
    curTOKEN->type = T_L_FLOAT;
    return 1;
  }
  else if (lex_have (l, INT)) {
    curTOKEN->type = T_L_INT;
    return 1;
  }
  else if (lex_have (l, EXTERN)) {
    int idx;
    curTOKEN->type = T_EXTERN;
    lex_mustbe (l, LBRACK);
    lex_mustbe (l, l_id);
    idx = extern_add (lex_prev (l));
    curTOKEN->toks = EXTERN_P[idx];
    lex_mustbe (l, RBRACK);
    return 1;
  }
  else if (lex_have (l, l_string)) {
    curTOKEN->type = T_KEYW;
    lex_prev (l)[strlen (lex_prev(l))-1] = '\0';
    curTOKEN->toks = Strdup (lex_prev (l)+1);
    if (lex_have (l, INTFLAG)) {
      curTOKEN->int_flag = 1;
    }
    else {
      curTOKEN->int_flag = 0;
    }
    if (lex_have (l, ENDANGLE)) {
      curTOKEN->end_gt = 1;
    }
    else if (lex_have (l, ENDANGLE2)) {
      curTOKEN->end_gt = -1;
    }
    else {
      curTOKEN->end_gt = 0;
    }
    return 1;
  }
  else if (lex_have (l, l_id)) {
    curTOKEN->type = T_LHS;
    curTOKEN->toks = Strdup (lex_prev (l));
    return 1;
  }
  else if (lex_have (l, LBRACE)) {
    token_type_t *x;
    NEW (tmp, token_list_t);
    x = curTOKEN;
    curTOKEN->type = T_LIST;
    curTOKEN->toks = tmp;
    save = curLIST;
    A_INIT (tmp->a);
    base_parse_token_list (l, tmp);
    if (lex_have (l, ENDSTARUNPACKED)) {
      int i, count;
      count = 0;
      for (i=0; i < A_LEN (tmp->a); i++) {
	if (HAS_DATA (tmp->a[i])) {
	  count++;
	  if (tmp->a[i].type == T_OPT || tmp->a[i].type == T_LIST ||
	      tmp->a[i].type == T_LIST_SPECIAL)
	    count++;
	}
      }
      if (count != 1) 
	fatal_error ("Special }* construct has more than one item with data; write some new code!");
      x->type = T_LIST_SPECIAL;
    }
    else {
      lex_mustbe (l, ENDSTAR);
    }
    curLIST = save;
    return 1;
  }
  else if (lex_have (l, LBRACK)) {
    NEW (tmp, token_list_t);
    curTOKEN->type = T_OPT;
    curTOKEN->toks = tmp;
    A_INIT (tmp->a);
    save = curLIST;
    base_parse_token_list (l, tmp);
    lex_mustbe (l, RBRACK);
    curLIST = save;
    return 1;
  }
  return 0;
}


void base_parse_token_list (LEX_T *l, token_list_t *t)
{
  static int nest = 0;
  int len, max;
  int id;
  token_type_t *myTOK;
  curLIST = t;

  nest++;
  A_INIT (curLIST->a);
  A_NEW (curLIST->a, token_type_t);
  curTOKEN = &A_NEXT (curLIST->a);
  curTOKEN->opt_next = NULL;

  if (nest == 1) {
    myTOK = curTOKEN;
  }
  while (parse_token (l)) {

    if (nest == 1 && lex_sym (l) == FBEGIN) {
      int k;
      MALLOC (myTOK->opt_next, struct body_info, A_LEN (WALK));
      for (k=0; k < A_LEN(WALK); k++)
	myTOK->opt_next[k].s = NULL;
    }

    while (nest == 1 && lex_sym (l) == FBEGIN) {
      lex_mustbe (l, FBEGIN);
      lex_mustbe (l, l_id);
      id = walk_lookup (lex_prev (l));
      if (id == -1) {
	fatal_error ("Unknown walk type: `%s'\n%s", lex_prev (l),
		     lex_errstring (l));
      }
      lex_begin_save (l);

      myTOK->opt_next[id].file = 
	Strdup(l->filename ? l->filename : "-unknown-");
      myTOK->opt_next[id].line = lex_linenumber (l);
      lex_mustbe (l, COLON);

      while (!lex_eof (l) && lex_sym (l) != FEND) {
	lex_getsym (l);
      }
      myTOK->opt_next[id].s = Strdup (lex_saved_string (l));
      myTOK->opt_next[id].s[strlen (myTOK->opt_next[id].s)-2] = '\0';
      lex_end_save (l);
      lex_mustbe (l, FEND);
    }
    A_INC (curLIST->a);
    A_NEW (curLIST->a, token_type_t);
    curTOKEN = &A_NEXT (curLIST->a);
    curTOKEN->opt_next = NULL;
    if (nest == 1) {
      myTOK = curTOKEN;
    }
  }
  nest--;
}


void parse_token_list (LEX_T *l)
{
  A_NEW (curBNF->a, token_list_t);
  A_INIT (A_NEXT (curBNF->a).a);
  base_parse_token_list (l, &A_NEXT (curBNF->a));
  A_INC (curBNF->a);
}

void parse_productions (LEX_T *l)
{
  A_INIT (curBNF->a);
  do {
    parse_token_list (l);
  } while (lex_have (l, OR));
}

void parse_bnf_item (LEX_T *l)
{
  A_NEW (BNF, bnf_item_t);

  lex_mustbe (l, l_id);

  A_INIT (A_NEXT (BNF).tok_opts);
  A_NEXT (BNF).lhs = Strdup (lex_prev (l));
  curBNF = &A_NEXT (BNF);
  curBNF->lhs_ret = NULL;
  curBNF->raw_tokens[0] = NULL;
  curBNF->raw_tokens[1] = NULL;

  if (lex_have (l, LBRACK)) {
    /* it's either
          foo

       or
          foo *
       _OR_
	a list_t *      --- C

      Those are the four options. Nothing else.
    */
    int len;
    int is_list = 0;

    if (lex_have_keyw (l, "list_t")) {
      lex_mustbe_keyw (l, "*");
      is_list = 1;
    }
    else {
      is_list = 0;
    }

    if (is_list == 0) {
      char *tok;
      lex_mustbe (l, l_id);
      tok = lex_prev (l);
      if (lex_sym (l) == COLON) {
	char buf[1024];
	strcpy (buf, lex_prev (l));
	strcat (buf, "::");
	lex_mustbe (l, COLON);
	lex_mustbe (l, COLON);
	lex_mustbe (l, l_id);
	strcat (buf, lex_prev (l));
	tok = Strdup (buf);
      }

      len = strlen (tok);

      curBNF->lhs_ret = malloc (strlen (tok)+10);
      strcpy (curBNF->lhs_ret, tok);
      curBNF->lhs_ret_base = Strdup (curBNF->lhs_ret);
      curBNF->lhs_ret_p = 0;

      if (tok != lex_prev (l)) {
	FREE (tok);
      }
    }
    else {
      curBNF->lhs_ret = Strdup ("list_t *");
      curBNF->lhs_ret_base = Strdup ("list_t");
      curBNF->lhs_ret_p = 1;
    }
    if (!is_list) {
      if (strcmp (lex_tokenstring (l), "*") == 0) {
	/* pointer */
	strcat (curBNF->lhs_ret, " *");
	curBNF->lhs_ret_p = 1;
	lex_getsym (l);
      }
      else if (strcmp (lex_tokenstring (l), "@") == 0) {
	/* non-NULL pointer */
	warning ("non-NULL pointer converted to normal pointer (C mode)\n%s\n", lex_errstring (l));
	strcat (curBNF->lhs_ret, " *");
	curBNF->lhs_ret_p = 1;
	lex_getsym (l);
      }
      else if (lex_sym (l) != RBRACK) {
	fatal_error ("Expecting id, or id *, or id @, got %s\n%s\n", lex_tokenstring (l), lex_errstring (l));
      }
    }
    else {
      char *tmp;
      int i = strlen (curBNF->lhs_ret);
    }
    lex_mustbe (l, RBRACK);
  }
  lex_mustbe (l, COLON);
  parse_productions (l);
  lex_mustbe (l, SEMI);
  A_INC (BNF);
  curBNF = NULL;
}

/*
  ========================================================================
    P a r s e r    e n d s
  ========================================================================
*/



/*
  ========================================================================
    P r e t t y - p r i n t   B N F
  ========================================================================
*/
void print_tok_list (pp_t *pp, token_list_t *t)
{
  int i;

  for (i=0; i < A_LEN (t->a); i++) {
    switch (t->a[i].type) {
    case T_L_EXPR:
      pp_puts (pp, " expr");
      break;
    case T_L_BEXPR:
      pp_puts (pp, " bool_expr");
      break;
    case T_L_IEXPR:
      pp_puts (pp, " int_expr");
      break;
    case T_L_REXPR:
      pp_puts (pp, " real_expr");
      break;
    case T_L_ID:
      pp_puts (pp, " ID");
      break;
    case T_L_STRING:
      pp_puts (pp, " STRING");
      break;
    case T_L_FLOAT:
      pp_puts (pp, " FLOAT");
      break;
    case T_L_INT: 
      pp_puts (pp, " INT");
      break;
    case T_TOKEN:
      pp_printf (pp, " \"%s\"", TT[(long)t->a[i].toks]);
      if (t->a[i].int_flag) {
	pp_printf (pp, " !noreal");
      }
      else if (t->a[i].end_gt == 1) {
	pp_printf (pp, " !endgt");
      }
      else if (t->a[i].end_gt == -1) {
	pp_printf (pp, " !noendgt");
      }
      break;
    case T_KEYW:
      pp_printf (pp, " \"%s\"", t->a[i].toks);
      if (t->a[i].int_flag) {
	pp_printf (pp, " !noreal");
      }
      else if (t->a[i].end_gt == 1) {
	pp_printf (pp, " !endgt");
      }
      else if (t->a[i].end_gt == -1) {
	pp_printf (pp, " !noendgt");
      }
      break;
    case T_EXTERN:
      pp_puts (pp, "EXTERN[");
      pp_printf (pp, "%s", t->a[i].toks);
      pp_puts (pp, "]");
      break;
    case T_LHS:
      pp_printf (pp, " %s", ((bnf_item_t *)t->a[i].toks)->lhs);
      break;
    case T_OPT:
      pp_lazy (pp, 2);
      pp_puts (pp, " [");
      print_tok_list (pp, t->a[i].toks);
      pp_puts (pp, " ]");
      break;
    case T_LIST:
      pp_lazy (pp, 2);
      pp_puts (pp, " {");
      print_tok_list (pp, t->a[i].toks);
      pp_puts (pp, " }**");
      break;
    case T_LIST_SPECIAL:
      pp_lazy (pp, 2);
      pp_puts (pp, " {");
      print_tok_list (pp, t->a[i].toks);
      pp_puts (pp, " }*");
      break;
    default:
      fatal_error ("Internal inconsistency");
      break;
    }
  }
}


void print_bnf (pp_t *pp)
{
  int i, j;

  pp_printf_raw (pp, "/*");
  pp_nl;
  pp_puts (pp, "   ");
  pp_setb (pp);
  
  pp_printf (pp, "BNF: %d bnf items", A_LEN (BNF)); pp_nl;
  pp_nl;
  pp_nl;

  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf (pp, "%s", BNF[i].lhs);
    pp_setb (pp);
    pp_printf (pp, ":");
    if (BNF[i].is_exclusive) {
      pp_printf (pp, " {excl}");
    }
    if (BNF[i].tail_recursive) {
      pp_printf (pp, " {t-rec}");
    }
    if (BNF[i].is_exclusive || BNF[i].tail_recursive) {
      pp_nl;
    }
    for (j=0; j < A_LEN (BNF[i].a); j++) {
      pp_setb (pp);
      print_tok_list (pp, &BNF[i].a[j]);
      pp_endb (pp);
      if (j != A_LEN (BNF[i].a)-1) {
	pp_nl;
	pp_printf (pp, "|");
      }
    }
    pp_nl;
    pp_printf (pp, ";");
    pp_endb (pp);
    pp_nl;
    pp_nl;
  }

  pp_endb (pp);
  pp_nl;
  pp_printf_raw (pp, "*/");
  pp_nl;
}

/*
  ========================================================================
    P r e t t y - p r i n t   B N F     E N D S    
  ========================================================================
*/

pp_t *std_open (char *s)
{
  FILE *fp;
  pp_t *pp;

  if ((fp = fopen (s, "r"))) {
    warning ("File `%s' exists, overwriting", s);
    fclose (fp);
  }

  fp = fopen (s, "w");
  if (!fp) {
    fatal_error ("Could not open file `%s' for writing", s);
  }

  pp = pp_init (fp, 76);

  pp_printf_text (pp, "/* Auto-generated by pgen, do not edit! */");
  pp_nl;

  return pp;
}

void std_close (pp_t *pp)
{
  pp_close (pp);
}


void emit_tokens (pp_t *pp)
{
  int i;

  for (i=0; i < A_LEN (TT); i++) {
    pp_printf (pp, "TOKEN(TOK_%d,\"%s\")", i, TT[i]);
    pp_nl;
  }
}

void emit_bexpr_for_typematch (pp_t *pp, token_list_t *tl, int idx)
{
  token_type_t *t = &tl->a[idx];

  if (A_LEN (tl->a) == 0 && idx == 0) {
    pp_printf (pp, "(1 == 1)");
    return;
  }

  switch (t->type) {
  case T_L_EXPR:
    pp_printf (pp, "(is_expr_parse_any(l))");
#if 0
    pp_printf (pp, "(is_expr_parse_int(l) || is_expr_parse_bool(l) || is_expr_parse_real(l))");
#endif
    break;
  case T_L_BEXPR:
    pp_printf (pp, "is_expr_parse_bool(l)");
    break;
  case T_L_IEXPR:
    pp_printf (pp, "is_expr_parse_int(l)");
    break;
  case T_L_REXPR:
    pp_printf (pp, "is_expr_parse_real(l)");
    break;
  case T_L_ID:
    pp_printf (pp, "file_sym (l) == f_id");
    break;
  case T_L_STRING:
    pp_printf (pp, "file_sym (l) == f_string");
    break;
  case T_L_FLOAT:
    pp_printf (pp, "file_sym (l) == f_real");
    break;
  case T_L_INT:
    pp_printf (pp, "file_sym (l) == f_integer");
    break;
  case T_KEYW:
    pp_printf (pp, "file_is_keyw (l, \"%s\")", (char *)t->toks);
    break;
  case T_TOKEN:
    pp_printf (pp, "file_sym (l) == TOK_%ld", (long)t->toks);
    break;
  case T_EXTERN:
    pp_printf (pp, "%s_is_a_%s (l)", prefix, (char *)t->toks);
    break;
  case T_LHS:
    pp_printf (pp, "is_a_%s (l)", ((bnf_item_t*)t->toks)->lhs);
    break;
  case T_OPT:
    /* [ foo ] bar
       is_a_foo() || is_a_bar()
    */
    pp_puts (pp, "(");
    emit_bexpr_for_typematch (pp, (token_list_t *)t->toks, 0);
    pp_puts (pp, ") || ");
    pp_lazy (pp, 2);

    if (idx >= A_LEN (tl->a)-1) {
      pp_puts (pp, "(1 == 1)");
    }
    else {
      pp_puts (pp, "(");
      emit_bexpr_for_typematch (pp, tl, idx+1);
      pp_puts (pp, ")");
    }
    break;
  case T_LIST:
  case T_LIST_SPECIAL:
    /* 
       { foo }**  or { foo }*
       is_a_foo ()
    */
    emit_bexpr_for_typematch (pp, (token_list_t *)t->toks, 0);
    break;
  default:
    fatal_error ("Ummm\n");
    break;
  }
}

void emit_is_functions (pp_t *pp)
{
  int i, j;

  if (A_LEN (BNF) == 0) return;

  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf_text (pp, "static int is_a_%s (LFILE *l)\n", BNF[i].lhs);
    pp_printf_text (pp, "{  ");
    BEGIN_INDENT;
    pp_printf_text (pp, "static int been_here = 0;\n");
    pp_printf_text (pp, "if (been_here) return 0;\n");
    pp_printf_text (pp, "been_here=1;\n");

    if (BNF[i].tail_recursive == 0) {
      if (BNF[i].raw_tokens[0] == NULL) {
	/*pp_printf_text (pp, "file_push_position (l);\n");*/
	for (j=0; j < A_LEN (BNF[i].a); j++) {
	  if (j != 0) {
	    pp_printf (pp, " else ");
	  }
	  pp_puts (pp, "if (");
	  emit_bexpr_for_typematch (pp, &BNF[i].a[j], 0);
	  pp_puts (pp, ") { ");
	  pp_forced (pp, 3);
	  pp_setb (pp);
	  pp_puts (pp, "been_here = 0; ");
	  pp_lazy (pp, 0);
	  /*pp_puts (pp, "file_set_position (l); ");
	    pp_lazy (pp, 0);
	    pp_puts (pp, "file_pop_position (l); "); */
	  pp_lazy (pp, 0);
	  pp_puts (pp, "return 1;");
	  pp_endb (pp);
	  pp_nl;
	  pp_puts (pp, "}");
	}
      }
      else {
	/* XXX */
	fatal_error ("Do something with raw tokens!");
      }
    }
    else {
      /*pp_printf_text (pp, "file_push_position (l);\n");*/
      pp_puts (pp, "if (");
      emit_bexpr_for_typematch (pp, &BNF[i].a[1], 0);
      pp_puts (pp, ") { ");
      pp_forced (pp, 3);
      pp_setb (pp);
      pp_puts (pp, "been_here = 0; ");
      pp_lazy (pp, 0);
      /*pp_puts (pp, "file_set_position (l); ");
      pp_lazy (pp, 0);
      pp_puts (pp, "file_pop_position (l); "); */
      pp_lazy (pp, 0);
      pp_puts (pp, "return 1;");
      pp_endb (pp);
      pp_nl;
      pp_puts (pp, "}");
    }
    pp_nl;
    /*pp_puts (pp, "file_set_position (l); file_pop_position (l);"); pp_nl;*/
    pp_printf_text (pp, "been_here = 0;\n");
    pp_printf_text (pp, "return 0;");
    END_INDENT;
    pp_printf_text (pp, "}\n\n");
  }
}

void emit_free_functions (pp_t *pp)
{
  int i, j, k;

  pp_printf_text (pp, "void %s_parse_free (%s_Token *t)\n", prefix, prefix);
  pp_printf_text (pp, "{");
  BEGIN_INDENT;
  pp_printf_text (pp, "if (!t) return;\n");
  pp_printf_text (pp, "switch (t->type) {");
  pp_printf_text (pp, "case %d: my_strfree (t->u.Tok_ID.n0); break;\n", Tok_ID_offset + A_LEN (BNF));
  pp_printf_text (pp, "case %d: my_strfree (t->u.Tok_STRING.n0); break;\n", Tok_STRING_offset + A_LEN (BNF));
  pp_printf_text (pp, "case %d: break;\n", Tok_FLOAT_offset + A_LEN (BNF));
  pp_printf_text (pp, "case %d: break;\n", Tok_INT_offset + A_LEN (BNF));

  pp_printf_text (pp, "case %d: list_apply (t->u.Tok_OptList.n0, NULL, __free_token_helper); list_free (t->u.Tok_OptList.n0); break;\n", Tok_OptList_offset + A_LEN (BNF));
  pp_printf_text (pp, "case %d: list_apply (t->u.Tok_SeqList.n0, NULL, __free_token_helper); list_free (t->u.Tok_SeqList.n0); break;\n", Tok_SeqList_offset + A_LEN (BNF));

  pp_printf_text (pp, "case %d:\n", Tok_EXTERN_offset + A_LEN (BNF));
  for (i=0; i < A_LEN (EXTERN_P); i++) {
    pp_printf_text (pp, "if (t->ext_num == %d) %s_free_a_%s (t->u.Tok_EXTERN.n0);\n", i, prefix, EXTERN_P[i]);
  }
  pp_printf_text (pp, "break;\n");
  if (found_expr) {
    pp_printf_text (pp, "case %d: expr_free (t->u.Tok_expr.n0); break;\n", Tok_expr_offset + A_LEN (BNF));
  }

  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf_text (pp, "case %d: ", i);
    pp_printf_text (pp, "free_a_%s (t->u.Tok_%s.n0);", BNF[i].lhs, BNF[i].lhs);
    pp_printf_text (pp, "break;"); pp_nl;
  }

  pp_printf_text (pp, "default: fatal_error (\"Unknown token type\"); break;\n");
  
  pp_printf_text (pp, "}\n");
  pp_printf_text (pp, "FREE (t);\n");
  END_INDENT;
  pp_printf_text (pp, "}\n\n");

  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf_text (pp, "static void free_a_%s (Node_%s *n)\n", BNF[i].lhs, BNF[i].lhs);
    pp_printf_text (pp, "{  ");
    BEGIN_INDENT;
    pp_printf_text (pp, "if (!n) return;\n");
    pp_printf_text (pp, "switch (n->type) {"); pp_nl;

    for (j=0; j < A_LEN (BNF[i].a); j++) {
      pp_printf_text (pp, "case %d: ", j); 
      BEGIN_INDENT;
      for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	/* needs a datatype if it is not a keyword */
	if (HAS_DATA (BNF[i].a[j].a[k])) {
	  switch (BNF[i].a[j].a[k].type) {
	  case T_L_EXPR:
	  case T_L_BEXPR:
	  case T_L_IEXPR:
	  case T_L_REXPR:
	    pp_printf_text (pp, "expr_free (n->u.Option_%s%d.f%d);\n", BNF[i].lhs, j, k);
	    break;
	  case T_L_ID:
	  case T_L_STRING:
	    pp_printf_text (pp, "my_strfree (n->u.Option_%s%d.f%d);\n", BNF[i].lhs, j, k);
	    break;
	  case T_L_FLOAT:
	  case T_L_INT:
	    break;
	  case T_EXTERN:
	    pp_printf_text (pp, "%s_free_a_%s (n->u.Option_%s%d.f%d);\n", prefix, (char *)BNF[i].a[j].a[k].toks, BNF[i].lhs, j, k);
	    break;
	  case T_LHS:
	    pp_printf_text (pp, "free_a_%s (n->u.Option_%s%d.f%d);\n", ((bnf_item_t *)BNF[i].a[j].a[k].toks)->lhs, BNF[i].lhs, j, k);
	    break;
	  case T_OPT:
	  case T_LIST:
	  case T_LIST_SPECIAL:
	    pp_printf_text (pp, "list_apply (n->u.Option_%s%d.f%d, NULL, __free_token_helper);\n", 
			    BNF[i].lhs, j, k);
	    pp_printf_text (pp, "list_free (n->u.Option_%s%d.f%d);\n", BNF[i].lhs, j, k);
	    break;
	  default:
	    fatal_error ("Internal inconsistency");
	    break;
	  }
	}
      }
      pp_printf_text (pp, "break;");
      END_INDENT;
    }
    pp_printf_text (pp, "}\n");
    pp_printf_text (pp, "FREE (n);\n");
    pp_printf_text (pp, "return;");
    END_INDENT;
    pp_printf_text (pp, "}\n\n");
  }
}

static char *fix_percents (char *s)
{
  static char buf[1024];
  int i = 0;

  while (*s) {
    if (i > 1022) {
      fatal_error ("Internal buffer capacity exceeded");
    }
    buf[i++] = *s;
    if (*s == '%') {
      buf[i++] = '%';
    }
    s++;
  }
  buf[i] = '\0';
  return buf;
}
  

/*
 *
 *  parse a list of tokens
 *
 *   if successful, we know where the values are;
 *   otherwise, it throws an LPF exception
 *
 *
 */
#define ERR(s)								\
  do {									\
    pp_printf (pp, "snprintf (");					\
    pp_printf (pp, "errstring, 4096, \"Expecting " s			\
	       ", got `%%s'\", file_eof (l) ? \"-eof-\" : file_tokenstring (l));"); \
    pp_nl;								\
    pp_printf (pp, "file_set_error (l, errstring);");			\
    pp_nl;								\
  } while (0)

#define ERR2(s,t)							\
  do {									\
    pp_printf (pp, "snprintf (");					\
    pp_printf (pp, "errstring, 4096, \"Expecting " s			\
	       " `%s', got `%%s'\", file_eof (l) ?  "			\
	       "\"-eof-\" : file_tokenstring (l));", fix_percents(t));	\
    pp_nl;								\
    pp_printf (pp, "file_set_error (l, errstring);");			\
    pp_nl;								\
  } while (0)


#define RETRY					\
  do {						\
    if (in_end_gt) {				\
       pp_printf (pp, "expr_endgtmode (0);");	\
    }						\
    pp_printf (pp, "THROW (EXC_LPF);");		\
  } while (0)

void emit_tmptok_wrapper (pp_t *pp, int type)
{
  pp_puts (pp, "{ Token *tmptok; NEW (tmptok, Token);");
  pp_printf_text (pp, " tmptok->type = %d;", type);
}

void emit_node_wrapper (pp_t *pp, int bnf, int opt)
{
  pp_printf_text (pp, "{ Node_%s *tmpnode; NEW (tmpnode, Node_%s); ", BNF[bnf].lhs, BNF[bnf].lhs);
  pp_nl;
  pp_printf_text (pp, "  tmpnode->type = %d; tmpnode->p = p;", opt);
}

static void emit_code_helper (pp_t *pp,
			      token_list_t *tmp, 
			      int j /* index */, int i /* offset */,
			      int nest, char *prefix)
{
  switch (tmp->a[j].type) {
  case T_L_EXPR:
  case T_L_IEXPR:
  case T_L_BEXPR:
  case T_L_REXPR:
    emit_tmptok_wrapper (pp, Tok_expr_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_expr.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;
  case T_L_ID:
    emit_tmptok_wrapper (pp, Tok_ID_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_ID.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;
  case T_L_STRING:
    emit_tmptok_wrapper (pp, Tok_STRING_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_STRING.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;
  case T_L_FLOAT:
    emit_tmptok_wrapper (pp, Tok_FLOAT_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_FLOAT.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;
  case T_L_INT: 
    emit_tmptok_wrapper (pp, Tok_INT_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_INT.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;
  case T_TOKEN:
  case T_KEYW:
    break;
  case T_LHS:
    emit_tmptok_wrapper (pp, bnf_item_to_num ((bnf_item_t *)tmp->a[j].toks));
    pp_printf (pp, "tmptok->u.Tok_%s.n0 = f_%d_%d;", 
	       ((bnf_item_t *)tmp->a[j].toks)->lhs, nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;

  case T_EXTERN:
    emit_tmptok_wrapper (pp, Tok_EXTERN_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_EXTERN.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "tmptok->ext_num = %d;", (int)extern_add ((char *)tmp->a[j].toks));
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;

  case T_OPT:
    emit_tmptok_wrapper (pp, Tok_OptList_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_OptList.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;
  case T_LIST:
  case T_LIST_SPECIAL:
    emit_tmptok_wrapper (pp, Tok_SeqList_offset + A_LEN (BNF));
    pp_printf (pp, "tmptok->u.Tok_SeqList.n0 = f_%d_%d;", nest+1, j);
    pp_printf (pp, "list_append (f_%d_%d, tmptok); }", nest, i);
    pp_nl;
    break;
  default:
    fatal_error ("Internal inconsistency");
    break;
  }
}


/*
  Returns 1 if an optional construct has no data associated with it
*/
int opt_token_no_data (token_type_t *t)
{
  int i;
  token_list_t *tx;

  if (t->type != T_OPT) return 0;
  tx = (token_list_t *)t->toks;
  for (i=0; i < A_LEN (tx->a); i++)
    if (HAS_DATA (tx->a[i]))
      return 0;
  return 1;
}

static void emit_frees_upto_curtoken (pp_t *pp, token_list_t *tl, int nest, int mypos)
{
  int i;

  for (i=0; i < mypos; i++) {
    switch (tl->a[i].type) {
    case T_L_EXPR:
    case T_L_BEXPR:
    case T_L_IEXPR:
    case T_L_REXPR:
      pp_printf (pp, "expr_free (f_%d_%d);", nest, i); pp_nl;
      break;
    case T_L_ID:
    case T_L_STRING:
      pp_printf (pp, "my_strfree (f_%d_%d);", nest, i); pp_nl;
      break;
    case T_L_FLOAT:
    case T_L_INT:
      break;
    case T_EXTERN:
      pp_printf (pp, "%s_free_a_%s (f_%d_%d);", prefix, (char *)tl->a[i].toks, nest, i); pp_nl;
      break;
    case T_KEYW:
    case T_TOKEN:
      break;
    case T_LHS:
      pp_printf (pp, "free_a_%s (f_%d_%d);", ((bnf_item_t *)tl->a[i].toks)->lhs, nest, i); pp_nl;
      break;
    case T_OPT:
    case T_LIST:
    case T_LIST_SPECIAL:
      pp_printf (pp, "list_apply (f_%d_%d, NULL, __free_token_helper);", nest, i); pp_nl;
      pp_printf (pp, "list_free (f_%d_%d);", nest, i);
      break;
    default:
      fatal_error ("Unknown type");
      break;
    }
  }
}

int emit_code_for_parsing_tokens (pp_t *pp, token_list_t *tl)
{
  int i, j;
  static int nest = 0;
  bnf_item_t *b;
  token_list_t *tmp;
  int nbraces, nret;
  static int in_end_gt = 0;
    
  nest++;
  nbraces = 0;
  for (i=0; i < A_LEN (tl->a); i++) {
    pp_printf (pp, "/* %d, %d */", nest, i);
    pp_nl;
    switch (tl->a[i].type) {
    case T_L_EXPR:
      pp_printf (pp, "{ Expr *");
      nbraces++;
      pp_printf (pp, "f_%d_%d = expr_parse_any (l);", nest, i); pp_nl;
#if 0      
      pp_printf (pp, "if (!f_%d_%d) f_%d_%d = expr_parse_bool (l);",
		 nest, i, nest, i); pp_nl;
      pp_printf (pp, "if (!f_%d_%d) f_%d_%d = expr_parse_real (l);",
		 nest, i, nest, i); pp_nl;
#endif      
      pp_printf (pp, "if (!f_%d_%d) {", nest, i);
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("expression"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_L_BEXPR:
      pp_printf (pp, "{ Expr *");
      nbraces++;
      pp_printf (pp, "f_%d_%d = expr_parse_bool (l);", nest, i); pp_nl;
      pp_printf (pp, "if (!f_%d_%d) {", nest, i);
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("boolean expression"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_L_IEXPR:
      pp_printf (pp, "{ Expr *");
      nbraces++;
      pp_printf (pp, "f_%d_%d = expr_parse_int (l);", nest, i); pp_nl;
      pp_printf (pp, "if (!f_%d_%d) {", nest, i);
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("integer expression"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_L_REXPR:
      pp_printf (pp, "{ Expr *");
      nbraces++;
      pp_printf (pp, "f_%d_%d = expr_parse_real (l);", nest, i); pp_nl;
      pp_printf (pp, "if (!f_%d_%d) {", nest, i);
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("integer expression"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_L_ID:
      nbraces++;
      pp_printf (pp, "{ const char *f_%d_%d;", nest, i); pp_nl;
      pp_printf (pp, "if (file_have (l, f_id))"); pp_nl;
      pp_printf (pp, "  f_%d_%d = strdup (file_prev (l));", nest, i); pp_nl;
      pp_printf (pp, "else {");
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("identifier"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_L_STRING:
      nbraces++;
      pp_printf (pp, "{ const char *f_%d_%d;", nest, i); pp_nl;
      pp_printf (pp, "if (file_have (l, f_string))"); pp_nl;
      pp_printf (pp, "  f_%d_%d = strdup (file_prev (l));", nest, i); pp_nl;
      pp_printf (pp, "else {");
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("string"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_L_FLOAT:
      nbraces++;
      pp_printf (pp, "{ double f_%d_%d;", nest, i); pp_nl;
      pp_printf (pp, "f_%d_%d = file_real (l);", nest, i); pp_nl;
      pp_printf (pp, "if (file_have (l, f_real))"); pp_nl;
      pp_printf (pp, "  { if (file_sym (l) != f_real) f_%d_%d = file_real (l); }", nest, i); pp_nl;
      pp_printf (pp, "else {");
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("real number"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_L_INT:
      nbraces++;
      pp_printf (pp, "{ int f_%d_%d;", nest, i); pp_nl;
      pp_printf (pp, "f_%d_%d = file_integer (l);", nest, i); pp_nl;
      pp_printf (pp, "if (file_have (l, f_integer))"); pp_nl;
      pp_printf (pp, "  { if (file_sym (l) != f_integer) f_%d_%d = file_integer (l); }", nest, i); pp_nl;
      pp_printf (pp, "else {");
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR("integer"); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_EXTERN:
      nbraces++;
      pp_printf (pp, "{ void *f_%d_%d = NULL;", nest, i); pp_nl;
      pp_printf (pp, "if (%s_is_a_%s (l)) {", prefix, (char*)tl->a[i].toks); pp_nl;
      pp_printf (pp, " f_%d_%d = %s_parse_a_%s (l);", nest, i, prefix, (char*)tl->a[i].toks); pp_nl;
      pp_printf (pp, "}"); pp_nl;
      pp_printf (pp, "else {");
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR2("extern-bnf-item", (char*)tl->a[i].toks); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      pp_printf (pp, "if (!f_%d_%d) {", nest, i); pp_nl;
      BEGIN_INDENT;
      ERR2("extern-bnf-item", (char*)tl->a[i].toks); 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      RETRY;
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      break;

    case T_KEYW:
      if (tl->a[i].int_flag) {
	pp_printf (pp, "{ int tmp_flag = file_flags (l); file_setflags (l, tmp_flag | FILE_FLAGS_NOREAL);"); 
	pp_nl;
      }
      if (tl->a[i].end_gt == 1) {
	in_end_gt++;
	pp_printf (pp, "expr_endgtmode (1);");
      }
      else if (tl->a[i].end_gt == -1) {
	in_end_gt--;
	pp_printf (pp, "expr_endgtmode (0);");
      }
      pp_printf (pp, "if (!file_have_keyw (l, \"%s\")) {", 
		 (char*)tl->a[i].toks);
      BEGIN_INDENT; 
      if (tl->a[i].int_flag) {
	pp_printf (pp, "file_setflags (l, tmp_flag);");
      }
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR2("keyword", (char*)tl->a[i].toks);
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      if (tl->a[i].int_flag) {
	pp_printf (pp, "file_setflags (l, tmp_flag); }"); pp_nl;
      }
      break;
    case T_TOKEN:
      if (tl->a[i].int_flag) {
	pp_printf (pp, "{ int tmp_flag = file_flags (l); file_setflags (l, tmp_flag | FILE_FLAGS_NOREAL);"); 
	pp_nl;
      }
      if (tl->a[i].end_gt == 1) {
	pp_printf (pp, "expr_endgtmode (1);");
	in_end_gt++;
      }
      else if (tl->a[i].end_gt == -1) {
	pp_printf (pp, "expr_endgtmode (0);");
	in_end_gt--;
      }
      pp_printf (pp, "if (!file_have (l, TOK_%ld)) {", (long)tl->a[i].toks);
      BEGIN_INDENT; 
      if (tl->a[i].int_flag) {
	pp_printf (pp, "file_setflags (l, tmp_flag);");
      }
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR2("token",TT[(long)tl->a[i].toks]); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      if (tl->a[i].int_flag) {
	pp_printf (pp, "file_setflags (l, tmp_flag); }"); pp_nl;
      }
      break;
    case T_LHS:
      nbraces++;
      b = (bnf_item_t *)tl->a[i].toks;
      pp_printf (pp, "{ Node_%s *f_%d_%d = NULL;", b->lhs, nest, i); pp_nl;
      pp_printf (pp, "int c_%d_%d = 0;", nest, i); pp_nl;
      pp_printf (pp, "if (is_a_%s (l)) {", b->lhs); pp_nl;
      pp_printf (pp, "   while (c_%d_%d != -1 && !f_%d_%d)", nest, i, nest, i);
      pp_nl;
      pp_printf (pp, "      f_%d_%d = parse_a_%s (l,&c_%d_%d);", 
		 nest, i, b->lhs, nest, i); pp_nl;
      pp_printf (pp, "}");
      pp_printf (pp, "else {");
      BEGIN_INDENT; 
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR2("bnf-item", b->lhs); 
      RETRY; 
      END_INDENT;
      pp_puts (pp, "}"); pp_nl;
      pp_printf (pp, "if (!f_%d_%d) {", nest, i); pp_nl;
      emit_frees_upto_curtoken (pp, tl, nest, i);
      ERR2("bnf-item", b->lhs); 
      RETRY;
      pp_printf (pp, "}"); pp_nl;
      break;
    case T_OPT:
      tmp = (token_list_t *)tl->a[i].toks;
      nbraces++;
      pp_printf (pp, "{ list_t *");
      pp_printf (pp, "f_%d_%d = NULL;", nest, i); pp_nl;
      pp_printf (pp, "f_%d_%d = list_new ();", nest, i); pp_nl;
      pp_printf (pp, "TRY {");
      BEGIN_INDENT;
      pp_printf (pp, "file_push_position (l);"); pp_nl;
      nret = emit_code_for_parsing_tokens (pp, tmp);
      for (j=0; j < A_LEN (tmp->a); j++) {
	emit_code_helper (pp, tmp, j, i, nest, prefix);
      }
      pp_nl;
      pp_printf_text (pp, "file_pop_position (l);");
      END_INDENT;
      /* if there's NO DATA, add a NULL pointer! */
      if (opt_token_no_data (&tl->a[i])) {
	pp_printf (pp, "  /* no data! */ list_append (f_%d_%d, NULL);", nest, i);
	pp_nl;
      }
      pp_puts (pp, "}");
      while (nret--) {
	pp_puts (pp, "}");
      }
      pp_nl;
      pp_puts (pp, "CATCH { EXCEPT_SWITCH {"); pp_nl;
      pp_puts (pp, "   case EXC_LPF:"); pp_nl;
      pp_printf_text (pp, "        file_set_position (l);\n");
      pp_printf_text (pp, "        file_pop_position (l);\n");
      pp_printf_text (pp, "        if (f_%d_%d) { list_apply (f_%d_%d, NULL, __free_token_helper); list_free (f_%d_%d); }\n", nest, i, nest, i, nest, i);
      pp_printf_text (pp, "        f_%d_%d = list_new ();\n", nest, i);
      pp_printf_text (pp, "        break;");
      pp_puts (pp, "   DEFAULT_CASE;"); pp_nl;
      pp_puts (pp, "}");
      pp_puts (pp, "}"); pp_nl;
      break;
    case T_LIST:
    case T_LIST_SPECIAL:
      tmp = (token_list_t *)tl->a[i].toks;
      pp_printf (pp, "{ list_t *f_%d_%d = NULL;", nest, i); pp_nl;
      nbraces++;
      pp_printf (pp, "int fskip_%d_%d = 0;", nest, i); pp_nl;
      pp_printf (pp, "f_%d_%d = list_new ();", nest, i); pp_nl;
      pp_printf (pp, "file_push_position (l);"); pp_nl;
      pp_printf (pp, "do {");
      BEGIN_INDENT;
      /* parse body, add it to a list */
      pp_printf (pp, "TRY { ");
      pp_nl;
      A_LEN_RAW (tmp->a)--;
      nret = emit_code_for_parsing_tokens (pp, tmp);
      A_LEN_RAW (tmp->a)++;
      for (j=0; j < A_LEN (tmp->a); j++) {
	emit_code_helper (pp, tmp, j, i, nest, prefix);
      }
      if (tmp->a[A_LEN(tmp->a)-1].type == T_KEYW) {
	ERR2("token", tmp->a[A_LEN(tmp->a)-1].toks);
      }
      else {
	ERR2("token", TT[(long)tmp->a[A_LEN(tmp->a)-1].toks]);
      }
      pp_printf (pp, "file_pop_position (l);");  pp_nl;
      pp_printf (pp, "file_push_position (l);"); 
      while (nret--) {
	pp_puts (pp, "}");
      }
      pp_nl;
      pp_printf (pp, "} CATCH { EXCEPT_SWITCH {"); pp_nl;
      pp_printf (pp, " case EXC_LPF:"); pp_nl;
      pp_printf (pp, " if (!list_isempty (f_%d_%d)) {", nest, i); pp_nl;
      pp_printf (pp, "     file_set_position (l);"); pp_nl;
      pp_printf (pp, "     fskip_%d_%d = 1;", nest, i); pp_nl;
      pp_printf (pp, "     break;"); pp_nl;
      pp_printf (pp, " } else {"); pp_nl;
      pp_printf (pp, "   list_free (f_%d_%d);", nest, i); pp_nl;
      emit_frees_upto_curtoken (pp, tl, nest, i);
      pp_printf (pp, "   file_pop_position (l);"); pp_nl;
      pp_printf (pp, "   THROW (EXC_LPF);"); pp_nl;
      pp_puts (pp, "}"); pp_nl;
      pp_puts (pp, "break;"); pp_nl;
      pp_puts (pp, "   DEFAULT_CASE;"); pp_nl;
      pp_printf (pp, " }"); pp_nl;
      pp_printf (pp, "}"); pp_nl;
      END_INDENT;
      pp_printf (pp, "} ");
      if (tmp->a[A_LEN(tmp->a)-1].type == T_KEYW) {
	pp_printf (pp, "while (!fskip_%d_%d && file_have_keyw (l, \"%s\"));",
		   nest, i,
		   tmp->a[A_LEN(tmp->a)-1].toks);
      }
      else {
	pp_printf (pp, "while (!fskip_%d_%d && file_have (l, TOK_%ld));", 
		   nest, i,
		   (long)tmp->a[A_LEN(tmp->a)-1].toks);
      }
      pp_nl;
      pp_printf (pp, "file_pop_position (l);");
      pp_nl;
      break;
    default:
      fatal_error ("Unknown type\n");
      break;
    }
  }
  nest--;
  return nbraces;
}


/*
  Return tree, or NULL (and have no allocated storage)
*/
void emit_parse_functions (pp_t *pp)
{
  int i, j, k;
  int flag;
  int nret;

  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf_text (pp, "static Node_%s *parse_a_%s (LFILE *l, int *opt)\n", 
		    BNF[i].lhs, BNF[i].lhs);
    pp_printf_text (pp, "{  ");
    BEGIN_INDENT;
    pp_printf_text (pp, "struct %s_position p;\n", prefix);
    pp_printf_text (pp, "file_get_position (l, &p.l, &p.c, &p.f);\n");



    pp_printf (pp, "if (*opt == %d) { *opt = -1; return NULL; }", 
	       A_LEN (BNF[i].a)); pp_nl;
    pp_printf_text (pp, "file_push_position (l);\n");

    j = 0;
    if (BNF[i].tail_recursive != 0) {
      j = 1;
    }
    for (; j < A_LEN (BNF[i].a); j++) {
      /* check for the jth production */

      if (BNF[i].tail_recursive) {
	pp_printf (pp, "{ Node_%s *retnode = NULL, *rettail = NULL;", BNF[i].lhs);
	BEGIN_INDENT;
	/* while this is still possible */
	pp_printf (pp, "while (");
	emit_bexpr_for_typematch (pp, &BNF[i].a[j], 0);
	pp_puts (pp, ") { ");
      }
      else {
	if (BNF[i].is_exclusive) {
	  pp_printf (pp, "/*EXCL*/");
	  pp_printf (pp, "if ((");
	}
	else {
	  pp_printf (pp, "if ((*opt <= %d) && (", j);
	}
	emit_bexpr_for_typematch (pp, &BNF[i].a[j], 0);
	pp_puts (pp, ")) { ");
      }
      BEGIN_INDENT;

      pp_printf (pp, "TRY {");
      BEGIN_INDENT;
      nret = emit_code_for_parsing_tokens (pp, &BNF[i].a[j]);
      pp_nl;
      pp_printf (pp, "file_pop_position (l);"); pp_nl;
      if (BNF[i].is_exclusive) {
	pp_printf (pp, "*opt = -1;/*EXCL*/");
      }
      else {
	pp_printf (pp, "*opt = %d;", j+1);
      }
      pp_nl;

      /* finished parsing; construct return value */

      /* this allocates "tmpnode" */
      emit_node_wrapper (pp, i, j);
#define PFX pp_puts (pp, ",")

      for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	switch (BNF[i].a[j].a[k].type) {
	case T_L_EXPR:
	case T_L_IEXPR:
	case T_L_BEXPR:
	case T_L_REXPR:
	case T_L_ID:
	case T_L_STRING:
	case T_L_FLOAT:
	case T_L_INT:
	case T_LHS:
	case T_OPT:
	case T_LIST:
	case T_LIST_SPECIAL:
	case T_EXTERN:
	  pp_printf (pp, "tmpnode->u.Option_%s%d.f%d = f_1_%d;",
		     BNF[i].lhs, j, k, k);
	  break;
	case T_KEYW:
	case T_TOKEN:
	  break;
	default:
	  fatal_error ("argh\n");
	}
      }
      if (BNF[i].tail_recursive) {
	pp_printf (pp, " file_push_position (l);");
	pp_nl;
	pp_printf (pp, " if (!retnode) { retnode = tmpnode; rettail = tmpnode; } else {");
	pp_nl;
	if (BNF[i].tail_recursive == 1) {
	  /* foo LHS | foo */
	  pp_printf (pp, " Node_%s tmp2 = *rettail;", BNF[i].lhs);
	  pp_printf (pp, " rettail->type = 0;");
	  pp_nl;
	  for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	    switch (BNF[i].a[j].a[k].type) {
	    case T_L_EXPR:
	    case T_L_IEXPR:
	    case T_L_BEXPR:
	    case T_L_REXPR:
	    case T_L_ID:
	    case T_L_STRING:
	    case T_L_FLOAT:
	    case T_L_INT:
	    case T_LHS:
	    case T_OPT:
	    case T_LIST:
	    case T_LIST_SPECIAL:
	    case T_EXTERN:
	      /* XXX: no cyclone */
	      pp_printf (pp, "rettail->u.Option_%s0.f%d = tmp2.u.Option_%s1.f%d;", BNF[i].lhs, k, BNF[i].lhs, k);
	      pp_nl;
	      break;
	    case T_KEYW:
	    case T_TOKEN:
	      break;
	    default:
	      fatal_error ("argh\n");
	    }
	  }
	  pp_printf (pp, "rettail->u.Option_%s0.f%d = tmpnode; rettail = tmpnode; ", BNF[i].lhs, k);
	  pp_nl;
	}
	else {
	  pp_printf (pp, " Node_%s tmp2 = *tmpnode;", BNF[i].lhs);
	  pp_printf (pp, " tmpnode->type = 0;");
	  pp_nl;
	    
	  for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	    switch (BNF[i].a[j].a[k].type) {
	    case T_L_EXPR:
	    case T_L_IEXPR:
	    case T_L_BEXPR:
	    case T_L_ID:
	    case T_L_STRING:
	    case T_L_FLOAT:
	    case T_L_INT:
	    case T_LHS:
	    case T_OPT:
	    case T_LIST:
	    case T_LIST_SPECIAL:
	    case T_EXTERN:
	      /* XXX: no cyclone */
	      pp_printf (pp, "tmpnode->u.Option_%s0.f%d = tmp2.u.Option_%s1.f%d;", BNF[i].lhs, k+1, BNF[i].lhs, k);
	      pp_nl;
	      break;
	    case T_KEYW:
	    case T_TOKEN:
	      break;
	    default:
	      fatal_error ("argh\n");
	    }
	  }
	  pp_printf (pp, "tmpnode->u.Option_%s0.f0 = retnode; retnode = tmpnode; ", BNF[i].lhs);
	  pp_nl;
	}
	END_INDENT;
	pp_printf (pp, "} }");
      }
      else {
	pp_printf (pp, " except_done (); return tmpnode; }");
      }
      pp_nl;
      END_INDENT;

      while (nret--) {
	pp_puts (pp, "}");
      }

      pp_printf (pp, "} CATCH { EXCEPT_SWITCH {"); pp_nl;
      pp_printf (pp, "   case EXC_LPF:"); pp_nl;
      if (BNF[i].tail_recursive) {
	pp_printf (pp, " file_set_position (l); file_pop_position (l); *opt = 2; return retnode; break; ");
	pp_nl;
      }
      else {
	/* XXX: release temp storage for the option you've had to deal with so far */
	if (BNF[i].is_exclusive) {
	  pp_printf (pp, "      *opt = -1 /* release EXCL */;"); pp_nl;
	}
	else {
	  pp_printf (pp, "      *opt = %d /* release */;", j+1); pp_nl;
	}
	pp_printf (pp, "      file_set_position (l); break;");
      }
      pp_nl;
      pp_puts (pp, "   DEFAULT_CASE;"); pp_nl;
      pp_printf (pp, "}");
      pp_printf (pp, "}");
      END_INDENT;
      pp_printf (pp, "}"); pp_nl;

      /* here's the fall-through */
      if (BNF[i].tail_recursive) {
	pp_printf (pp, " if (retnode) { file_pop_position (l); return retnode; } }");
      }
      else {
	pp_printf (pp, "else { *opt = %d; }", j+1);
      }
      pp_nl;
    }
    pp_printf (pp, "file_set_position (l);"); pp_nl;
    pp_printf (pp, "file_pop_position (l);"); pp_nl;
    if (BNF[i].tail_recursive) {
      pp_printf (pp, "*opt = -1;"); pp_nl;
    }
    pp_printf (pp, "return NULL;");
    END_INDENT;
    pp_printf (pp, "}");
    pp_nl;
  }
}

char *tok_type_to_parser_type (token_type_t *t)
{
  static char buf[1024];

  switch (t->type) {
  case T_L_EXPR:
  case T_L_BEXPR:
  case T_L_IEXPR:
  case T_L_REXPR:
    return "Expr *";
    break;
  case T_L_ID:
  case T_L_STRING:
    return "const char *";
    break;
  case T_L_FLOAT:
    return "double";
    break;
  case T_L_INT:
    return "int";
    break;
  case T_EXTERN:
    return "void *";
    break;
  case T_LHS:
    sprintf (buf, "Node_%s *",
	     ((bnf_item_t *)t->toks)->lhs);
    return buf;
    break;
  case T_OPT:
  case T_LIST:
  case T_LIST_SPECIAL:
    return "list_t *";
    break;
  default:
    fatal_error ("Internal inconsistency");
    break;
  }
  return NULL;
}

	
void emit_parser (void)
{
  pp_t *pp;
  int i, j, k;
  int flag, flag2;
  char buf[512];

  if (!gen_parse)
    return;

  if (A_LEN (BNF) == 0) {
    warning ("No productions in the grammer; bailing out");
    return;
  }

  /* token definitions */
  sprintf (buf, "%s_parse.def", prefix);
  pp = std_open (buf);
  emit_tokens (pp);
  pp_printf_text (pp, "#undef TOKEN"); pp_nl;
  std_close (pp);

  /*-- parse tree data structure --*/
  sprintf (buf, "%s_parse.h", prefix);
  pp = std_open (buf);
  pp_printf_text (pp, "#ifndef __%s_PARSE_EXT_H__\n", prefix);
  pp_printf_text (pp, "#define __%s_PARSE_EXT_H__\n\n", prefix);
  pp_printf_text (pp, "#include <common/list.h>\n");
  pp_printf_text (pp, "#include <common/misc.h>\n");
  if (found_expr) {
    pp_printf_text (pp, "#include \"expr.h\"\n", prefix);
  }
  pp_printf_text (pp, "#ifdef __cplusplus\n");
  pp_printf_text (pp, "extern \"C\" {\n");
  pp_printf_text (pp, "#endif\n");
  pp_printf_text (pp, "struct %s_DefToken;\n", prefix);
  pp_printf_text (pp, "typedef struct %s_DefToken %s_Token;\n", prefix, prefix);
  pp_printf_text (pp, "%s_Token *%s_parse (const char *);\n", prefix, prefix);
  pp_printf_text (pp, "void %s_parse_free (%s_Token *);\n", prefix, prefix);
  pp_printf_text (pp, "#ifdef __cplusplus\n");
  pp_printf_text (pp, "}\n");
  pp_printf_text (pp, "#endif\n");
  pp_printf_text (pp, "#endif /* __PARSE_EXT_H__ */\n");
  std_close (pp);

  sprintf (buf, "%s_parse_int.h", prefix);

  pp = std_open (buf);
  pp_printf_text (pp, "#ifndef __%s_PARSE_H__\n", prefix);
  pp_printf_text (pp, "#define __%s_PARSE_H__\n\n", prefix);
  pp_printf_text (pp, "#include <common/list.h>\n");
  pp_printf_text (pp, "#include <common/file.h>\n");
  if (found_expr) {
    pp_printf_text (pp, "#include \"expr.h\"\n");
  }
  pp_nl;
  pp_printf_text (pp, "#include \"%s_parse.h\"\n", prefix);
  pp_printf_text (pp, "#ifdef __cplusplus\n");
  pp_printf_text (pp, "extern \"C\" {\n");
  pp_printf_text (pp, "#endif\n");

  /* declare a position */
  
  pp_printf_text (pp, "struct %s_position {", prefix); pp_nl;
  pp_printf_text (pp, "  int l, c;"); pp_nl;
  pp_printf_text (pp, "  char *f;"); pp_nl;
  pp_printf_text (pp, "};"); pp_nl; pp_nl;

  pp_printf_text (pp, "void %s_parse_err (struct %s_position *p, const char *fmt, ...);\n", prefix, prefix);
  pp_printf_text (pp, "void %s_parse_warn (struct %s_position *p, const char *fmt, ...);\n", prefix, prefix);
  pp_printf_text (pp, "void %s_parse_msg (struct %s_position *p, const char *fmt, ...);\n", prefix, prefix);
  pp_printf_text (pp, "void %s_parse_seterr (LFILE *l, const char *fmt, ...);\n", prefix, prefix);

  /* create:
     1. a datatype for each LHS node
     2. fields in the LHS are simply constructors per production
     3. keywords don't come into play at all

     tok := x | a | b | c
     
     datatype Node_tok {
           constructor_x
	   constructor_a
	   constructor_b
	   constructor_c
     }

     x = const_list id_list const_list id_list

     const_list = ignore
     id_list = part of the constructor is a pointer to the datatype

     [ x ] =   list<x>
     { x }** = list<x>

     ===============

     C code: 

      Datatypes look like this:

       struct Node_tok {
         int type;
	 union {
	   struct {
	      fields of x
	   } Name_x;
	   ...
	 } u;
       };
       enum Node_tok_type {
         Type_Node_x, ....
       };

       Each field list has as its first paramter the item pos, which is
       a struct prefix_pos.
  */

  /*
    Part 1: declare all the datatypes
  */

  pp_printf_text (pp, "/* datatype declarations */\n");
  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf_text (pp, "typedef struct DefNode_%s Node_%s;\n",
		    BNF[i].lhs, BNF[i].lhs);
  }

  /* Generic token datatype */
  pp_printf_text (pp, "struct %s_DefToken {", prefix);

  BEGIN_INDENT;
  pp_printf_text (pp, "int type;\n");
  pp_printf_text (pp, "int ext_num;\n");
  pp_printf_text (pp, "union {");
  BEGIN_INDENT;
  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf_text (pp, "struct { Node_%s *n0; } Tok_%s;",
		    BNF[i].lhs, BNF[i].lhs);
    pp_nl;
  }
  /* -- builtins -- */
  /* this ORDER MATTERS---must match the offsets shown above! */
  pp_printf_text (pp, "struct { const char *n0; } Tok_ID;\n");
  pp_printf_text (pp, "struct { const char *n0; } Tok_STRING;\n");
  pp_printf_text (pp, "struct { double n0; } Tok_FLOAT;\n");
  pp_printf_text (pp, "struct { int n0; } Tok_INT;\n");
  pp_printf_text (pp, "struct { list_t *n0; } Tok_OptList;\n");
  pp_printf_text (pp, "struct { list_t *n0; } Tok_SeqList;");
  pp_printf_text (pp, "struct { void *n0; } Tok_EXTERN;\n");

  if (found_expr) {
    pp_printf_text (pp, "\nstruct { Expr *n0; } Tok_expr;");
  }
  END_INDENT;
  pp_printf_text (pp, "} u;");
  END_INDENT;
  pp_printf_text (pp, "};\n");
  
  /*
     Part 2: 
         Add fields to each datatype
  */
  pp_nl;
  for (i=0; i < A_LEN (BNF); i++) {
    int union_or_not = 0;
    pp_printf_text (pp, "struct DefNode_%s {", BNF[i].lhs);
    BEGIN_INDENT;
    pp_printf_text (pp, "int type;"); pp_nl;
    pp_printf_text (pp, "struct %s_position p;", prefix); pp_nl;
    for (j=0; j < A_LEN (BNF[i].a); j++) {
      int mm = 0;
      for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	if (HAS_DATA (BNF[i].a[j].a[k])) {
	  mm = 1;
	  break;
	}
      }
      if (mm && union_or_not == 0) {
	pp_printf_text (pp, "union {");
	flag2 = 0;
	BEGIN_INDENT;
	union_or_not = 1;
      }
      
      if (flag2) {
	pp_nl;
      }
      flag2 = 1;

      if (mm) {
	pp_printf_text (pp, "struct {");
	BEGIN_INDENT;
	for (k=0; k < A_LEN (BNF[i].a[j].a); k++) {
	  /* needs a datatype if it is not a keyword */
	  if (HAS_DATA (BNF[i].a[j].a[k])) {
	    pp_lazy (pp, 3);
	    switch (BNF[i].a[j].a[k].type) {
	    case T_L_EXPR:
	    case T_L_BEXPR:
	    case T_L_IEXPR:
	    case T_L_REXPR:
	      pp_printf_text (pp, "Expr *f%d; ", k);
	      break;
	    case T_L_ID:
	    case T_L_STRING:
	      pp_printf_text (pp, "const char *f%d; ", k);
	      break;
	    case T_L_FLOAT:
	      pp_printf_text (pp, "double f%d;", k);
	      break;
	    case T_L_INT:
	      pp_printf_text (pp, "int f%d;", k);
	      break;
	    case T_EXTERN:
	      pp_printf_text (pp, "void *f%d;", k);
	      break;
	    case T_LHS:
	      pp_printf_text (pp, "Node_%s *f%d; ",
			      ((bnf_item_t *)BNF[i].a[j].a[k].toks)->lhs, k);
	      break;
	    case T_OPT:
	    case T_LIST:
	    case T_LIST_SPECIAL:
	      pp_printf_text (pp, "list_t *f%d; ", k);
	      break;
	    default:
	      fatal_error ("Internal inconsistency");
	      break;
	    }
	  }
	}
	END_INDENT;
	pp_printf_text (pp, "} Option_%s%d;", BNF[i].lhs, j);
      }
    }
    if (union_or_not) {
      END_INDENT;
      pp_printf_text (pp, "} u;");
    }
    END_INDENT;
    pp_printf_text (pp, "};\n\n");
  }

  pp_nl; pp_nl;
  
  pp_printf_text (pp, "#ifdef __cplusplus\n");
  pp_printf_text (pp, "}\n");
  pp_printf_text (pp, "#endif\n");
  pp_printf_text (pp, "#endif /* __PARSE_H__ */\n");
  std_close (pp);

  if (found_expr) {
    sprintf (buf, "%s_parse_id.h", prefix);
    pp = std_open (buf);
    pp_printf_text (pp, "typedef struct DefNode_expr_id pId;\n");
    std_close (pp);
  }

  sprintf (buf, "%s_parse.c", prefix);
  pp = std_open (buf);
  pp_printf_text (pp, "#include <stdio.h>\n");
  pp_printf (pp, "#include <stdlib.h>"); pp_nl;
  pp_printf_text (pp, "#include <common/file.h>\n"); 
  pp_printf_text (pp, "#include <common/mstring.h>\n"); 
  pp_printf_text (pp, "#include \"%s_parse.h\"\n", prefix);
  pp_printf_text (pp, "#include \"%s_parse_int.h\"\n", prefix);
  pp_printf (pp, "typedef struct %s_DefToken Token;", prefix);
  pp_nl;
  pp_nl;

  pp_printf_text (pp, "#include <stdarg.h>\n");
  pp_printf_text (pp, "#include <common/except.h>\n");
  pp_printf_text (pp, "#include <common/misc.h>\n");
  pp_printf_text (pp, "#define EXC_LPF 2\n");
  pp_nl;

  pp_printf_text (pp, "static char *errstring;\n\n");

  pp_printf_text (pp, "#define TOKEN(a,b)  static int a;"); pp_nl;
  pp_printf_text (pp, "#include \"%s_parse.def\"", prefix); pp_nl; pp_nl;

  pp_puts (pp, "static const char *strdup (const char *s)");
  pp_puts (pp, "{");
  pp_puts (pp, "return string_char (string_create (s));");
  pp_puts (pp, "}");
  pp_nl; pp_nl;

  pp_puts (pp, "static void my_strfree (const char *s)");
  pp_puts (pp, "{");
  pp_puts (pp, "   mstring_t *xstr; xstr = string_create (s); string_free (xstr); string_free (xstr);");
  pp_puts (pp, "}");
  pp_nl; pp_nl;

  pp_printf (pp, "void %s_parse_warn (struct %s_position *p, const char *fmt, ...)", prefix, prefix);
  pp_nl;
  pp_puts (pp, "{"); pp_nl;

  pp_puts (pp, " va_list ap;"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"WARNING: \");"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"File `%s', line %d, col %d\\n\\t\", p->f, p->l, p->c);"); pp_nl;
  pp_puts (pp, " va_start (ap, fmt);"); pp_nl;
  pp_puts (pp, " vfprintf (stderr, fmt, ap);"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"\\n\");"); pp_nl;
  pp_puts (pp, " va_end (ap);"); pp_nl;
  pp_puts (pp, "}"); pp_nl; pp_nl;

  pp_printf_text (pp, "static void __free_token_helper (void *x, const void *t) { %s_parse_free ((%s_Token *)t); }\n\n", prefix, prefix);

  pp_printf (pp, "void %s_parse_msg (struct %s_position *p, const char *fmt, ...)", prefix, prefix);
  pp_nl;
  pp_puts (pp, "{"); pp_nl;
  pp_puts (pp, " va_list ap;"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"ERROR: \");"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"File `%s', line %d, col %d\\n\\t\", p->f, p->l, p->c);"); pp_nl;
  pp_puts (pp, " va_start (ap, fmt);"); pp_nl;
  pp_puts (pp, " vfprintf (stderr, fmt, ap);"); pp_nl;
  pp_puts (pp, " va_end (ap);"); pp_nl;
  pp_puts (pp, "}"); pp_nl; pp_nl;

  pp_printf (pp, "void %s_parse_err (struct %s_position *p, const char *fmt, ...)", prefix, prefix);
  pp_nl;
  pp_puts (pp, "{"); pp_nl;
  pp_puts (pp, " va_list ap;"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"ERROR: \");"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"File `%s', line %d, col %d\\n\\t\", p->f, p->l, p->c);"); pp_nl;
  pp_puts (pp, " va_start (ap, fmt);"); pp_nl;
  pp_puts (pp, " vfprintf (stderr, fmt, ap);"); pp_nl;
  pp_puts (pp, " va_end (ap);"); pp_nl;
  pp_puts (pp, " fprintf (stderr, \"\\n\");"); pp_nl;
  pp_puts (pp, " exit (1);"); pp_nl;
  pp_puts (pp, "}"); pp_nl; pp_nl;

  pp_printf (pp, "void %s_parse_seterr (LFILE *l, const char *fmt, ...)", prefix, prefix);
  pp_nl;
  pp_puts (pp, "{"); pp_nl;
  pp_puts (pp, " va_list ap;"); pp_nl;
  pp_puts (pp, " va_start (ap, fmt);"); pp_nl;
  pp_puts (pp, " vsnprintf (errstring, 4096, fmt, ap);"); pp_nl;
  pp_puts (pp, " va_end (ap);"); pp_nl;
  pp_puts (pp, "file_set_error (l, errstring);"); pp_nl;
  pp_puts (pp, "}"); pp_nl; pp_nl;

  /* Emit declarations for all the functions that we will implement */
  for (i=0; i < A_LEN (BNF); i++) {
    pp_printf_text (pp, "static int is_a_%s (LFILE *l);\n", BNF[i].lhs);
    pp_printf_text (pp, "static Node_%s *parse_a_%s (LFILE *l, int *opt);\n", BNF[i].lhs, BNF[i].lhs);
    pp_printf_text (pp, "static void free_a_%s (Node_%s *);\n", BNF[i].lhs, BNF[i].lhs);
  }
  if (A_LEN (EXTERN_P) > 0) {
    pp_printf_text (pp, "#ifdef __cplusplus\n");
    pp_printf_text (pp, "extern \"C\" {\n");
    pp_printf_text (pp, "#endif\n");
  }
  for (i=0; i < A_LEN (EXTERN_P); i++) {
    pp_printf_text (pp, "int %s_is_a_%s (LFILE *l);\n", prefix, EXTERN_P[i]);
    pp_printf_text (pp, "void *%s_parse_a_%s (LFILE *l);\n", prefix,
		    EXTERN_P[i]);
    pp_printf_text (pp, "void %s_free_a_%s (void *);\n", prefix, EXTERN_P[i]);
    pp_printf_text (pp, "void %s_init_%s (LFILE *l);\n", prefix, EXTERN_P[i]);
  }
  if (A_LEN (EXTERN_P) > 0) {
    pp_printf_text (pp, "#ifdef __cplusplus\n");
    pp_printf_text (pp, "}\n");
    pp_printf_text (pp, "#endif\n");
  }
  pp_nl;
  pp_nl;
  if (found_expr) {
    pp_puts (pp, "static int is_expr_parse_any (LFILE *l) { return expr_parse_isany (l); }");
    pp_nl;
    pp_puts (pp, "static int is_expr_parse_bool (LFILE *l) { return expr_parse_isany (l); }");
    pp_nl;
    pp_puts (pp, "static int is_expr_parse_int (LFILE *l) { return expr_parse_isany (l); }");
    pp_nl;
    pp_puts (pp, "static int is_expr_parse_real (LFILE *l) { return expr_parse_isany (l); }");
    pp_nl;
#if 0
    pp_puts (pp, "static int is_expr_parse_bool (LFILE *l) { Expr *e; file_push_position (l); if ((e = expr_parse_bool (l))) { expr_free (e); file_set_position (l); file_pop_position (l); return 1; } else { file_set_position (l); file_pop_position (l); return 0; } }");
    pp_nl;
    pp_puts (pp, "static int is_expr_parse_int (LFILE *l) { Expr *e; file_push_position (l); if ((e = expr_parse_int (l))) { expr_free (e); file_set_position (l); file_pop_position (l); return 1; } else { file_set_position (l); file_pop_position (l); return 0; } }");
    pp_nl;
    pp_puts (pp, "static int is_expr_parse_real (LFILE *l) { Expr *e; file_push_position (l); if ((e = expr_parse_real (l))) { expr_free (e); file_set_position (l); file_pop_position (l); return 1; } else { file_set_position (l); file_pop_position (l); return 0; } }");
    pp_nl;
    pp_puts (pp, "static int is_expr_parse_any (LFILE *l) { Expr *e; file_push_position (l); if ((e = expr_parse_any (l))) { expr_free (e); file_set_position (l); file_pop_position (l); return 1; } else { file_set_position (l); file_pop_position (l); return 0; } }");
    pp_nl;
#endif
    pp_puts (pp, "static Node_expr_id *parse_a_expr__id (LFILE *l) { int opt = 0; Node_expr_id *e = NULL; while (opt != -1 && !e) { e = parse_a_expr_id (l, &opt); } return e; }");
    pp_nl;
    pp_puts (pp, "void free_a_expr__id (void *v) { free_a_expr_id ((Node_expr_id *)v); }"); 
  }
  pp_nl;
  pp_nl;

  pp_printf_text (pp, "void %s_lex_init (LFILE *l)", prefix);

  pp_nl; pp_printf (pp, "{ ");
  pp_nl;
  pp_printf_text (pp, "#define TOKEN(a,b)  a = file_addtoken (l, b);");
  pp_nl;
  pp_printf_text (pp, "#include \"%s_parse.def\"", prefix); pp_nl;
  if (verilog_ids) {
    pp_printf_text (pp, "  file_setflags (l, file_flags(l)|FILE_FLAGS_ESCAPEID|FILE_FLAGS_PARENCOM);");
    pp_nl;
  }
  if (hexdigit) {
    pp_printf_text (pp, "  file_setflags (l, file_flags (l)|FILE_FLAGS_HEXINT);");
    pp_nl;
  }
  if (bindigit) {
    pp_printf_text (pp, "  file_setflags (l, file_flags(l)|FILE_FLAGS_BININT);");
    pp_nl;
  }
  pp_printf_text (pp, "   file_setflags (l, FILE_FLAGS_PARSELINE|file_flags(l));");
  pp_nl;
  if (found_expr) {
    pp_printf_text (pp, "  expr_init (l);");
    pp_nl;
    pp_printf_text (pp, "  expr_parse_id = parse_a_expr__id;\n");
    pp_printf_text (pp, "  expr_free_id = free_a_expr__id;\n");
  }
  for (i=0; i < A_LEN (EXTERN_P); i++) {
    pp_printf_text (pp, "%s_init_%s (l);\n", prefix, EXTERN_P[i]);
  }
  pp_printf_text (pp, "   errstring = (char *)malloc (4096*sizeof (char));");pp_nl;
  pp_printf_text (pp, "   if (!errstring) { fatal_error "); 
  pp_nl; pp_puts (pp, "(\"out of memory\"); }"); pp_nl;
  pp_printf_text (pp, "   errstring[0] = '\\0';"); pp_nl;
  pp_printf_text (pp, "   file_getsym (l);");
  pp_nl; pp_puts (pp, "}"); pp_nl; pp_nl;

  pp_printf_text (pp, "Token * %s_parse (const char *s)", prefix);
  pp_nl; pp_printf (pp, "{ ");
  pp_setb (pp);
  pp_nl;
  pp_printf_text (pp, "LFILE *l = file_open (s);\n");
  pp_printf_text (pp, "Token * t;\n");
  pp_printf_text (pp, "%s_lex_init (l);\n", prefix);
  pp_printf_raw (pp, "snprintf (errstring,");
  pp_printf_raw (pp, " 4096, \"Expecting `%s'\\n\");\n", BNF[0].lhs);

  pp_printf_text (pp, "file_set_error (l, errstring);\n");
  pp_printf_text (pp, "TRY {\n");
  pp_printf_text (pp, "  Node_%s *rv = NULL;\n", BNF[0].lhs);
  pp_printf_text (pp, "  int rv_opt = 0;\n");
  pp_printf_text (pp, "  while (rv_opt != -1 && !rv) {\n");
  pp_printf_text (pp, "     rv = parse_a_%s (l, &rv_opt);\n", BNF[0].lhs);
  pp_printf_text (pp, "   }\n");
  pp_printf_text (pp, " if (!rv) { THROW (EXC_NULL_EXCEPTION); }\n");
  pp_printf_text (pp, " NEW (t, Token); t->type = %d; t->u.Tok_%s.n0 = rv;\n", 0, BNF[0].lhs);
  pp_printf_text (pp, "} CATCH { EXCEPT_SWITCH {\n");
  pp_printf (pp, " case EXC_NULL_EXCEPTION:"); pp_nl;
  pp_printf (pp, " fprintf (stderr, \"Parse error: %%s\\n\", file_errstring (l)); THROW(EXC_NULL_EXCEPTION);"); pp_nl;
  pp_puts (pp, "   DEFAULT_CASE;"); pp_nl;
  pp_printf (pp, "}");
  pp_printf_text (pp, "}\n");

  pp_printf_text (pp, "  if (!file_eof (l)) {");
  BEGIN_INDENT;
  ERR("end-of-file");
  pp_puts (pp, "fprintf (stderr, \"Parse error: Could not parse entire file.\\n%s.\\n\", file_errstring (l));"); pp_nl;

  pp_printf_text (pp, "THROW (EXC_NULL_EXCEPTION);");
  END_INDENT;
  pp_puts (pp, "}"); pp_nl;
  pp_printf_text (pp, "  list_cleanup();\n");
  pp_printf_text (pp, "  return t;\n");
  pp_endb (pp); pp_nl;
  pp_puts (pp, "}"); pp_nl; pp_nl;

  
  pp_puts (pp, "/* --- the parser --- */"); pp_nl;

  
  /* emit the "is-a-foo" functions */
  emit_is_functions (pp);

  /* emit the "parse-a-foo" functions */
  emit_parse_functions (pp);

  /* emit the "free-a" functions */
  emit_free_functions (pp);

  std_close (pp);
}


/*
  ========================================================================
   Check consistency
  ========================================================================
*/
long add_to_tokens (char *s)
{
  int i;

  /* O(n^2) here; can be turned into O(n) with a hash table,
     but why bother... n is small */

  for (i=0; i < A_LEN (TT); i++) {
    if (strcmp (s, TT[i]) == 0)
      return i;
  }
  A_NEW (TT, char *);
  A_NEXT (TT) = Strdup (s);
  A_INC (TT);
  return A_LEN (TT)-1;
}

void check_tok_list (token_list_t *t)
{
  int i, j;
  char *s;
  int l;
  token_list_t *tmp;

  for (i=0; i < A_LEN (t->a); i++) {
    switch (t->a[i].type) {
    case T_L_EXPR:
    case T_L_IEXPR:
    case T_L_BEXPR:
    case T_L_REXPR:
    case T_L_ID:
    case T_L_STRING:
    case T_L_FLOAT:
    case T_L_INT: 
    case T_EXTERN:
      break;
    case T_KEYW:
      /* check this here!! */
#if 0
      s = (char *) t->a[i].toks;
      l = strlen (s);
      for (j=0; j < l; j++) {
	if (!isalpha (s[j]))
	  break;
      }
      if (j != l) {
#endif
	t->a[i].type = T_TOKEN;
	t->a[i].toks = (void *) add_to_tokens (t->a[i].toks);
#if 0
	FREE (s);
      }
#endif
      break;
    case T_LHS:
      for (j=0; j < A_LEN (BNF); j++) {
	if (strcmp ((char *)t->a[i].toks, BNF[j].lhs) == 0) {
	  t->a[i].toks = &BNF[j];
	  break;
	}
      }
      if (j == A_LEN (BNF)) {
	fatal_error ("RHS uses `%s' that isn't recognized", t->a[i].toks);
      }
      break;
    case T_OPT:
      tmp = (token_list_t *)t->a[i].toks;
      if (A_LEN (tmp->a) == 0) {
	fatal_error ("Optional item has nothing in it!!!");
      }
      check_tok_list (tmp);
      break;
    case T_LIST:
    case T_LIST_SPECIAL:
      tmp = (token_list_t *)t->a[i].toks;
      if (A_LEN (tmp->a) < 2) {
	fatal_error ("list item needs at least two things in it!!");
      }
      if (HAS_DATA (tmp->a[A_LEN(tmp->a)-1])) {
	fatal_error ("list must end in a keyword or token");
      }
      check_tok_list (tmp);
      break;
    default:
      fatal_error ("Internal inconsistency");
      break;
    }
  }
}

void check_consistency_patch (void)
{
  int i, j, flag;

  flag = 0;
  for (i=0; i < A_LEN (BNF); i++) {
    if (strcmp (BNF[i].lhs, "expr_id") == 0)
      flag = 1;
    for (j=i+1; j < A_LEN (BNF); j++) {
      if (strcmp (BNF[i].lhs, BNF[j].lhs) == 0) {
	fatal_error ("LHS `%s' multiply defined!", BNF[i].lhs);
      }
    }
    for (j=0; j < A_LEN (BNF[i].a); j++)
      check_tok_list (&BNF[i].a[j]);
  }
  if (!flag && found_expr)
    fatal_error ("`expr_id' not defined, but `expr' is used!\n");

  /* at the end of this, either the BNF is consistent and all
     toks fields for T_LHS have been replaced with a BNF pointer,
     
     all token fields have been replaced by an index into the token
     table.

     _or_ an error has been reported
  */
}

/*
  returns 1 if this is an "optional" token type
*/
static int bitset_add_token_type (bitset_t *s, token_type_t *t)
{
  token_list_t *tl;
  int i;

  Assert (t, "huh?");
  
  switch (t->type) {
  case T_L_EXPR:
  case T_L_IEXPR:
  case T_L_BEXPR:
  case T_L_REXPR:
    /* expression */
    bitset_set (s, 4);
    break;

  case T_L_ID:
    bitset_set (s, 0);
    break;

  case T_L_STRING:
    bitset_set (s, 3);
    break;

  case T_L_FLOAT:
    bitset_set (s, 2);
    break;

  case T_L_INT:
    bitset_set (s, 1);
    break;
	    
  case T_KEYW:
    fatal_error ("T_KEYW?");
    break;
    /* string */

  case T_TOKEN:
    bitset_set (s, (long)t->toks + tok_offset);
    break;
    /* token */

  case T_LHS:
    bitset_or (s,
	       sets[(((unsigned long)t->toks)-((unsigned long)&BNF[0]))/sizeof (BNF[0])]);
    break;
	    
  case T_OPT:
    tl = (token_list_t *)t->toks;
    i = 0;
    while (i < A_LEN (tl->a) && bitset_add_token_type (s, &tl->a[i])) {
      i++;
    }
    return 1;
    break;

  case T_LIST:
  case T_LIST_SPECIAL:
    /* first item required */
    tl = (token_list_t *)t->toks;
    i = 0;
    while (i < A_LEN (tl->a) && bitset_add_token_type (s, &tl->a[i])) {
      i++;
    }
    break;
	    
  case T_EXTERN:
    /* external IS function */
    bitset_set (s, ext_offset + extern_add ((char *)t->toks));
    break;

  default:
    fatal_error ("Unknown token type");
    break;
  }
  return 0;
}

static void fprint_option (int j)
{
  if (j >= tok_offset) {
    printf (" t[%s]", TT[j-tok_offset]);
  }
  else if (j >= ext_offset) {
    printf (" e[%s]", EXTERN_P[j - ext_offset]);
  }
  else if (j == 0) {
    printf (" ID");
  }
  else if (j == 1) {
    printf (" INT");
  }
  else if (j == 2) {
    printf (" FLOAT");
  }
  else if (j == 3) {
    printf (" STRING");
  }
  else if (j == 4) {
    printf (" expr");
  }
  else if (j == 5) {
    printf (" <e>");
  }
}

/*
  Compute the list of possible starter tokens for each BNF item
*/
static void compute_token_options (void)
{
  int i, j, k;
  bnf_item_t *bi;
  bitset_t *tmp, *tmp2, *tmp3;
  int change;

  change = 1;

  MALLOC (sets, bitset_t *, sizeof (bitset_t *)*A_LEN (BNF));

  tot_options = 6 + A_LEN (EXTERN_P) + A_LEN (TT);
  ext_offset = 6;
  tok_offset = ext_offset + A_LEN (EXTERN_P);

  for (i=0; i < A_LEN (BNF); i++) {
    sets[i] = bitset_new (tot_options);
    bitset_clear (sets[i]);
  }
  tmp = bitset_new (tot_options);
  tmp2 = bitset_new (tot_options);
  tmp3 = bitset_new (tot_options);

  /* 
     tokens:

     0 = ID
     1 = INT
     2 = FLOAT
     3 = STRING
     4 = expr
     5 = <empty>
     6 = EXTERN[0]
     ... ...
     n = TOK_0
     ...
  */

  while (change) {
    change = 0;

    for (i=0; i < A_LEN (BNF); i++) {
      bi = &BNF[i];

      /* tmp is used to compute the bitsets */
      bitset_clear (tmp);
      
      /* foreach option for this BNF item */
      for (j=0; j < A_LEN (bi->a); j++) {
	k = 0;
	while (k < A_LEN (bi->a[j].a) && bitset_add_token_type (tmp, &bi->a[j].a[k])) {
	  k++;
	}
	if (k == A_LEN (bi->a[j].a)) {
	  bitset_set (tmp, 5);
	}
      }
      if (!bitset_equal (tmp, sets[i])) {
	change = 1;
	bitset_or (sets[i], tmp);
      }
    }
  }

  for (i=0; i < A_LEN (BNF); i++) {
    bi = &BNF[i];
    for (j=0; j < A_LEN (bi->a); j++) {
      bi->a[j].s = bitset_new (tot_options);
      bitset_clear (bi->a[j].s);
      k = 0;
      while (k < A_LEN (bi->a[j].a) && 
	     bitset_add_token_type (bi->a[j].s, &bi->a[j].a[k])) {
	k++;
      }
      if (k == A_LEN (bi->a[j].a)) {
	bitset_set (bi->a[j].s, 5);
      }
    }
  }
   
#if 0
#define PRINT_BITSETS
#endif

  /* DEBUG */
  for (i=0; i < A_LEN (BNF); i++) {
#ifdef PRINT_BITSETS
    printf ("%s:", BNF[i].lhs);
    for (j=0; j < tot_options; j++) {
      if (bitset_tst (sets[i], j)) {
	printf (" ");
	fprint_option (j);
      }
    }
    printf ("\n");
#endif
    bitset_clear (tmp);
    bitset_clear (tmp2);
    for (j=0; j < A_LEN (BNF[i].a); j++) {
      bitset_clear (tmp3);
      bitset_or (tmp3, tmp);
      bitset_and (tmp3, BNF[i].a[j].s);
      if (!bitset_isclear (tmp3)) {
	bitset_or (tmp2, tmp3);
      }
      bitset_or (tmp, BNF[i].a[j].s);
#ifdef PRINT_BITSETS
      printf ("  opt %d:", j);
      for (k=0; k < tot_options; k++) {
	if (bitset_tst (BNF[i].a[j].s, k)) {
	  printf (" ");
	  fprint_option (k);
	}
      }
      printf ("\n");
#endif
    }
    BNF[i].is_exclusive = 0;
    if (!BNF[i].tail_recursive && j > 1) {
      if (bitset_isclear (tmp2) && (bitset_tst (tmp, 5) == 0)) {
	BNF[i].is_exclusive = 1;
#ifdef PRINT_BITSETS
	printf (" --> disjoint!\n");
#endif
      }
      else {
#ifdef PRINT_BITSETS
	printf (" **overlap: ");
	for (k=0; k < tot_options; k++) {
	  if (bitset_tst (tmp2, k)) {
	    printf (" ");
	    fprint_option (k);
	  }
	}
	printf ("\n");
#endif
      }
    }
#ifdef PRINT_BITSETS
    printf ("\n");
#endif
  }
}





static int count_occurences (token_list_t *t, bnf_item_t *b)
{
  int i;
  int count = 0;

  for (i=0; i < A_LEN (t->a); i++) {
    switch (t->a[i].type) {
    case T_LHS:
      if (t->a[i].toks == (void *)b)
	count++;
      break;
    case T_LIST:
    case T_LIST_SPECIAL:
    case T_OPT:
      count += count_occurences ((token_list_t *)t->a[i].toks, b);
      break;
    default:
      break;
    }
  }
  return count;
}


static int equal_toks (token_type_t *t1, token_type_t *t2, int len)
{
  int i;
  token_list_t *tl1, *tl2;

  for (i=0; i < len; i++) {
    if (t1[i].type != t2[i].type) return 0;
    switch (t1[i].type) {
    case T_L_EXPR:
    case T_L_BEXPR:
    case T_L_IEXPR:
    case T_L_REXPR:
    case T_L_ID:
    case T_L_STRING:
    case T_L_FLOAT:
    case T_L_INT: 
      break;
    case T_TOKEN:
    case T_EXTERN:
    case T_KEYW:
    case T_LHS:
      if (t1[i].toks != t2[i].toks) return 0;
      break;
    case T_OPT:
    case T_LIST:
    case T_LIST_SPECIAL:
      tl1 = (token_list_t *)t1[i].toks;
      tl2 = (token_list_t *)t2[i].toks;
      if (A_LEN (tl1->a) != A_LEN(tl2->a)) return 0;
      if (!equal_toks (tl1->a, tl2->a, A_LEN (tl1->a))) {
	return 0;
      }
      break;
    default:
      fatal_error ("Internal inconsistency");
      break;
    }
  }
  return 1;
}

/*
  Search for tail recursion in each call
*/
static void find_tail_recursion (void)
{
  int i, j;
  int p, q;
  token_type_t *x;
  int pos, count;

  for (i=0; i < A_LEN (BNF); i++) {
    BNF[i].tail_recursive = 0;

    if (A_LEN (BNF[i].a) == 2) {
      /* a possibility;
	 now check structure */
      p = 0; 
      q = 0;
      if (A_LEN (BNF[i].a[0].a) == 1 + A_LEN (BNF[i].a[1].a)) {
	p = 0;
	q = 1;
      }
      else if (A_LEN (BNF[i].a[0].a) + 1 == A_LEN (BNF[i].a[1].a)) {
	p = 1;
	q = 0;
      }
      if (p || q) {
	/* BNF[i].a[p] is the longer clause;
	   BNF[i].a[q] is the shorter one
	*/
	pos = 0;
	count = 0;
	for (j=0; j < A_LEN (BNF[i].a[p].a); j++) {
	  x = &BNF[i].a[p].a[j];
	  if (x->type == T_LHS) {
	    if (x->toks == (void *)&BNF[i]) {
	      pos = j;
	      count++;
	    }
	  }
	  else if (x->type == T_OPT || x->type == T_LIST ||
		   x->type == T_LIST_SPECIAL) {
	    count += count_occurences ((token_list_t *)x->toks, &BNF[i]);
	  }
	}
	if (count == 1 && (pos == 0 || pos == A_LEN (BNF[i].a[p].a)-1)) {
	  if (pos == 0) {
	    /* lhs : lhs <foo> or <foo> 
	       check foo!
	    */
	    if (equal_toks (&BNF[i].a[p].a[1], BNF[i].a[q].a, 
			    A_LEN (BNF[i].a[q].a))) {
	      BNF[i].tail_recursive = 2;
	      printf ("Head-recursive call for %s\n", BNF[i].lhs);
	      /* swap to make a[0] the shorter longer */
	      if (p == 1) {
		A_DECL (token_type_t, tmp);
		A_INIT (tmp);
		
		A_ASSIGN (tmp, BNF[i].a[1].a);
		A_ASSIGN (BNF[i].a[1].a, BNF[i].a[0].a);
		A_ASSIGN (BNF[i].a[0].a, tmp);
	      }
	    }
	  }
	  else {
	    if (equal_toks (BNF[i].a[p].a, BNF[i].a[q].a,
			    A_LEN (BNF[i].a[q].a))) {
	      BNF[i].tail_recursive = 1;
	      /*printf ("Tail-recursive call for %s\n", BNF[i].lhs);*/

	      if (p == 1) {
		A_DECL (token_type_t, tmp);
		A_INIT (tmp);
		
		A_ASSIGN (tmp, BNF[i].a[1].a);
		A_ASSIGN (BNF[i].a[1].a, BNF[i].a[0].a);
		A_ASSIGN (BNF[i].a[0].a, tmp);
	      }
	    }
	  }
	}
      }
    }
  }
}




/*
No options: 
  generate everything

  -v : be verbose  with warnings
  -p : generate parser
  -g : only print out grammar in the .gram file
  -w <walk> : specify walker that should be generated
  -n <name> : prefix used (default std)
  -c : emit cyclone code instead of C
*/
static void usage (char *s)
{
  fprintf (stderr, "Usage: %s <grammar> [-vpgcVbh] [-n prefix] { -w walk }*\n", s);
  fprintf (stderr, "  -v : verbose warnings\n");
  fprintf (stderr, "  -p : generate parser\n");
  fprintf (stderr, "  -g : only print out grammar in the .gram file\n");
  fprintf (stderr, "  -c : emit cyclone code instead of C\n");
  fprintf (stderr, "  -h : support hex constants\n");
  fprintf (stderr, "  -b : support binary constants\n");
  fprintf (stderr, "  -V : support Verilog escaped IDs\n");
  fprintf (stderr, "  -w <walk> : specify walker that should be generated\n");
  fprintf (stderr, "  -n <name> : prefix used (default: std)\n");
  exit (1);
}



/*
  ========================================================================
  MAIN PROGRAM
  ========================================================================
*/
int main (int argc, char **argv)
{
  LEX_T *l;
  int gen_allwalk;
  int gram_only;

  A_INIT (EXTERN_P);
  A_INIT (BNF);
  A_INIT (TT);
  A_INIT (GWALK);

  if (argc < 2) {
    usage (argv[0]);
  }
  l = lex_fopen (argv[1]);
#define TOKEN(a,b)  a = lex_addtoken (l,b);
#include "pgen.def"

  verbose = 0;
  verilog_ids = 0;
  gen_parse = 1;
  gen_allwalk = 1;
  gram_only = 0;
  hexdigit = 0;
  bindigit = 0;

  if (argc > 2) {
    int i;

    gen_parse = 0;
    gen_allwalk = 0;

    for (i=2; i < argc; i++) {
      if (strcmp (argv[i], "-v") == 0) {
	verbose++;
      }
      if (strcmp (argv[i], "-V") == 0) {
	verilog_ids = 1;
      }
      else if (strcmp (argv[i], "-b") == 0) {
	bindigit = 1;
      }
      else if (strcmp (argv[i], "-h") == 0) {
	hexdigit = 1;
      }
      else if (strcmp (argv[i], "-g") == 0) {
	gram_only = 1;
      }
      else if (strcmp (argv[i], "-p") == 0) {
	gen_parse = 1;
      }
      else if (strcmp (argv[i], "-n") == 0) {
	if (i == argc -1) {
	  usage (argv[0]);
	}
	else {
	  i++;
	  prefix = Strdup (argv[i]);
	}
      }
      else if (strcmp (argv[i], "-w") == 0) {
	if (i == argc - 1) 
	  usage (argv[0]);
	else {
	  i++;
	  A_NEW (GWALK, char *);
	  A_NEXT (GWALK) = Strdup (argv[i]);
	  A_INC (GWALK);
	}
      }
      else {
	usage (argv[0]);
      }
    }
  }

  
  found_expr = 0;

  lex_setflags (l, LEX_FLAGS_NOREAL|LEX_FLAGS_PARSELINE);
  lex_getsym (l);

  A_INIT (WALK);
  A_INIT (cookie_type);
  A_INIT (return_type);

  do {
    lex_mustbe (l, TYPE);
    lex_mustbe (l, LBRACK);
    lex_mustbe (l, l_id);
    
    A_NEW (WALK, char *);
    A_NEW (cookie_type, char *);
    A_NEW (return_type, char *);

    A_NEXT (WALK) = Strdup (lex_prev (l));
    if (gen_allwalk) {
      A_NEW (GWALK, char *);
      A_NEXT (GWALK) = Strdup (lex_prev (l));
      A_INC (GWALK);
    }
    
    lex_mustbe (l, RBRACK);

    lex_mustbe (l, FBEGIN);
    lex_mustbe (l, l_id);
    A_NEXT (cookie_type) = Strdup (lex_prev (l));
    lex_mustbe (l, l_id);
    A_NEXT (return_type) = Strdup (lex_prev (l));
    lex_mustbe (l, FEND);
    lex_mustbe (l, SEMI);
    
    A_INC (WALK);
    A_INC (cookie_type);
    A_INC (return_type);

  } while (lex_sym (l) == TYPE);
  
  while (!lex_eof (l) && lex_sym (l) != l_err) {
    parse_bnf_item (l);
  }
  lex_free (l);
  check_consistency_patch ();

  find_tail_recursion ();

  compute_token_options ();
  
  {
    char buf[1024];
    pp_t *pp;
    sprintf (buf, "%s_parse.gram", prefix);
    pp = std_open (buf);
    print_bnf (pp);
    pp_close (pp);
  }

  if (gram_only) {
    return 0;
  }


  emit_parser ();
  emit_walker ();
  return 0;
}

/*---
    pgen grammar

body: production
    | body production
    ;

production: ID ':' right_hand_side_list ';'
          ;

right_hand_side_list: token_list
                    | right_hand_side_list '|' token_list
                    ;

token_list: token_item 
          | token_item token_list
          ;

token_item: L_EXPR
          | L_ID
          | L_STRING
          | L_FLOAT
          | L_INT
          | STRING
          | ID
          | '[' token_list ']'
          | '{' token_list ENDSTAR
          ;

*/
