/*************************************************************************
 *
 *  Parser generator
 *
 *  Copyright (c) 2003-2008, 2019 Rajit Manohar
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

#include "act/pgen/pgen.h"

#define WNAME  WALK[walk_id]
#define WCOOKIE cookie_type[walk_id]
#define WRET    return_type[walk_id]

int walk_id;
static pp_t *pp;
static pp_t *app;

extern char *prefix;
extern char *path_prefix;

/*------------------------------------------------------------------------

   W R A P P E R     F U N C T I O N S

  ------------------------------------------------------------------------
*/

L_A_DECL(char *, wraptable);

static void clearwrap (void)
{
  A_INIT (wraptable);
}

/*
  Return type name mangling
*/
static char *return_type_name (bnf_item_t *b)
{
  static char name[1024];
  int i;

  if (!b->lhs_ret) {
    /* the default return type */
    return WRET;
  }
  else {
    strcpy (name, b->lhs_ret_base);
    for (i=0; name[i]; i++) {
      if (name[i] == ':') {
	name[i] = '_';
      }
    }
    switch (b->lhs_ret_p) {
    case 0:
      break;
    case 1:
      strcat (name, "_p");
      break;
    case 2:
      strcat (name, "_zp");
      break;
    case 3:
      strcat (name, "_lp");
      break;
    default:
      fatal_error ("What ret type is this?!");
      break;
    }
    if (strcmp (name, "float") == 0) {
      strcpy (name, "double");
    }
    else if ((strcmp (name, "char_p") == 0) || 
	     (strcmp (name, "char_zp") == 0)) {
      strcpy (name, "string");
    }
    else if ((strcmp (name, "list_t_p") == 0)) {
      strcpy (name, "list");
    }
    return name;
  }
}


/*
  Return name for function that takes the return type of b to
  the default return type
*/
static char *wrapper_function_name (bnf_item_t *b)
{
  static char name[1024];

  if (!b->lhs_ret)
    return "";
  sprintf (name, "%s_wrap_%s_%s", prefix, WNAME, return_type_name (b));
  return name;
}


/*
  Emit prototype for wraper function
*/
static void emit_wrap_fn (pp_t *pp, bnf_item_t *b)
{
  /* look for wrap function */
  char name[1024];
  int i;

  /* Only need wrapper functions for
      - non-default types
      - non-built-in types
  */
  if (!b->lhs_ret) return;
  strcpy (name, return_type_name (b));
  if (strcmp (name, "int") == 0) return;
  if (strcmp (name, "double") == 0) return;
  if (strcmp (name, "string") == 0) return;

  /* check if the function prototype has already been emitted */
  for (i=0; i < A_LEN (wraptable); i++) {
    if (strcmp (wraptable[i], name) == 0) return;
  }
  A_NEW (wraptable, char *);
  A_NEXT(wraptable) = Strdup (name);
  A_INC (wraptable);

  /* emit prototype */
  pp_printf (pp, "%s *%s (%s);", WRET, wrapper_function_name (b), b->lhs_ret);
  pp_forced (pp, 0);
}


/*
  Name of wrapper function
     Returns "" if unnecessary
*/
char *wrapper_name (int i)
{
  return wrapper_function_name (&BNF[i]);
}


/*
  Return type for the list_t<x> special construct
*/
char *special_user_ret (bnf_item_t *b)
{
  if (b->lhs_ret) {
    if (b->lhs_ret_p != 3)
      return b->lhs_ret_base;
    else {
      return b->lhs_ret;
    }
  }
  else {
    return WRET;
  }
}

char *special_user_ret_id (int i)
{
  return special_user_ret (&BNF[i]);
}

/*
  Return type for a BNF item: corresponds to what the user typed
*/
char *user_ret (bnf_item_t *b)
{
  static char buf[1024];
  if (b->lhs_ret) {
    return b->lhs_ret;
  }
  else {
    sprintf (buf, "%s *", WRET);
    return buf;
  }
}

int is_user_ret_ptr (bnf_item_t *b)
{
  if (b->lhs_ret) {
    if (b->lhs_ret_p != 0) {
      return 1;
    }
    return 0;
  }
  else {
    return 1;
  }
}

char *user_ret_id (int id)
{
  return user_ret (&BNF[id]);
}


static token_type_t *find_special_list_token (token_type_t *t)
{
  int i;

  Assert (t->type == T_LIST_SPECIAL, "What?!");
  
  for (i=0; i < A_LEN (((token_list_t *)t->toks)->a); i++) {
    if (HAS_DATA (((token_list_t*)t->toks)->a[i])) {
      return &((token_list_t*)t->toks)->a[i];
    }
  }
  Assert (0, "ERRRRR");
  return NULL;
}

char *special_wrapper_name (token_type_t *t)
{
  static char buf[1024];
  Assert (t->type == T_LIST_SPECIAL, "WHA?!");
  t = find_special_list_token (t);
  sprintf (buf, "walkmap_%s_", WNAME);
  switch (t->type) {
  case T_L_EXPR:
  case T_L_IEXPR:
  case T_L_BEXPR:
  case T_L_REXPR:
    strcat (buf, "expr");
    break;
  case T_L_ID:
  case T_L_STRING:
    strcat (buf, "string");
    break;
  case T_L_FLOAT:
    strcat (buf, "double");
    break;
  case T_L_INT:
    strcat (buf, "int");
    break;
  case T_EXTERN:
    strcat (buf, "extern");
    break;
  case T_LHS:
    strcat (buf, ((bnf_item_t *)t->toks)->lhs);
    break;
  default:
    fatal_error ("uh oh");
    break;
  }
  return buf;
}

/*
  Return a string coresponding to the return type of a token
*/
char *production_to_ret_type (token_type_t *t)
{
  static char buf[1024], buf2[1024];
  int i;

  switch (t->type) {
  case T_L_EXPR:
  case T_L_IEXPR:
  case T_L_BEXPR:
  case T_L_REXPR:
    sprintf (buf, "Expr *");
    return buf;
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
    return user_ret ((bnf_item_t *)t->toks);
    break;
  case T_OPT:
  case T_LIST:
    sprintf (buf, "list_t *");
    return buf;
    break;
  case T_LIST_SPECIAL:
    /* 
       It is list_t<x>
    */       
    sprintf (buf2, "list_t *");
    return buf2;
    break;
  default:
    fatal_error ("Uh oh");
    break;
  }
  return NULL;
}


/*
  Return the walk_id for the specified walk type
  -1 if not found.
*/
static int find_id (char *s)
{
  int i;

  for (i=0; i < A_LEN (WALK); i++)
    if (strcmp (s, WALK[i]) == 0)
      return i;
  return -1;
}

void emit_walker (void)
{
  FILE *fp;
  int i, j, k, l;
  int flag;
  int gwalk_id;
  char buf[1024];

  if (BNF[0].lhs_ret != NULL) {
    printf ("Fatal: Top-level _must_ have the default return type!\n");
    exit (1);
  }

  for (gwalk_id = 0; gwalk_id < A_LEN (GWALK); gwalk_id++) {
    /* emit walker for specified walk type */
    clearwrap ();
    walk_id = find_id (GWALK[gwalk_id]);
    if (walk_id == -1) {
      fprintf (stderr, "Walk `%s' doesn't exist!\n",GWALK[gwalk_id]);
      exit (1);
    }

    /* open files */
    sprintf (buf, "%s_walk_%s.h", prefix, WNAME);
    app = std_open (buf);
    print_header_prolog (app);

    sprintf (buf, "%s_walk_%s.c", prefix, WNAME);
    pp = std_open (buf);
    print_walker_prolog (pp);

    /* emit wrapper function headers */
    pp_printf (app, "#ifdef __cplusplus"); pp_forced (app, 0);
    pp_printf (app, "}"); pp_forced (app, 0);
    pp_printf (app, "#endif"); pp_forced (app, 0);
    for (i=0; i < A_LEN (BNF); i++) {
      emit_wrap_fn (app, &BNF[i]);
    }
    pp_printf (app, "#ifdef __cplusplus"); pp_forced (app, 0);
    pp_printf (app, "extern \"C\" {"); pp_forced (app, 0);
    pp_printf (app, "#endif"); pp_forced (app, 0);

    /* top-level walk function */
    print_walker_main (pp);

    /* print recursive call to the walk function to traverse
       the entire parse tree
    */
    print_walker_recursive (pp, app);

    print_walker_apply_fns (pp);

    print_walker_local_apply_fns (pp);

    pp_printf (app, "#ifdef __cplusplus"); pp_forced (app, 0);
    pp_printf (app, "}"); pp_forced (app, 0);
    pp_printf (app, "#endif"); pp_forced (app, 0);
    pp_printf (app, "#endif /* __WALK_%s_H__ */", WNAME); 
    pp_forced (app, 0);

    std_close (app);
    std_close (pp);
  }
}
