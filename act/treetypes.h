/*************************************************************************
 *
 *  Copyright (c) 2011-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __STD_TREE_TYPES_H__
#define __STD_TREE_TYPES_H__

#include <act/namespaces.h>
#include <act/types.h>
#include <act/expr.h>

/**
 * This type is used to store "wrapped" return values for the
 * auto-generated code that walks the parse tree
 */


enum act_ret_type_type {
  R_INT,
  R_EXPR,
  R_AEXPR,
  R_DIR,
  R_USERDEF,
  R_LIST,
  R_ARRAY,
  R_STRING,
  R_ID,
  R_ATTR,
  R_PRS_LANG,
  R_CHP_LANG,
  R_SPEC_LANG,
  R_CHP_GC,
  R_CHP_FUNC,
  R_INST_TYPE,
  R_ACT_BODY
};

typedef struct {
  enum act_ret_type_type type;
  union {
    int ival;
    Expr *exp;
    AExpr *ae;
    Array *array;
    Type::direction dir;
    UserDef *u;
    list_t *l;
    const char *str;
    ActId *id;
    act_attr_t *attr;
    act_prs_lang_t *prs;
    act_chp_lang_t *chp;
    act_spec *spec;
    act_chp_gc *gc;
    act_func_arguments_t *func;
    InstType *inst;
    ActBody *body;
  } u;
} ActRet;

/**
 * Top-down information when the parse tree is being traversed 
 */
typedef struct {
  /** 
   * Global namespace
   */
  ActNamespace *global;
  
  /**
   * Current namespace
   */
  ActNamespace *curns;

  /**
   * Current scope
   */
  Scope *scope;

  /**
   * Namespace search paths and permissions in the current context
   */
  ActOpen *os;

  /**
   * Type allocation mechanism used
   */
  TypeFactory *tf;

  /**
   * User-defined type that is currently being defined
   */
  UserDef *u;
  Process *u_p;			/* process */
  Data *u_d;			/* data */
  Channel *u_c;			/* user-defined channel */
  Function *u_f;		/* function */

  InstType *t;			/* for instances */

  ActBody *t_inst;		/* temp instance */

  /* insttype/id to pass down */
  InstType *i_t;
  const char *i_id;
  ActId *a_id;

  /* power supplies */
  struct {
    ActId *vdd, *gnd, *psc, *nsc;
  } supply;

  /* set to 1 if expressions must be strict types: used to check that
     template parameters can be only used in port lists */
  int strict_checking;

  /* used for nested subckt/tree in prs bodies. No nesting allowed */
  int in_tree;
  int in_subckt;

  /* err */
  int line;
  int column;
  char *file;

  /* attributes */
  int attr_num;
  char **attr_table;

} ActTree;

#ifdef __cplusplus
extern "C" {
#endif
int act_fet_string_to_value (const char *s);
#ifdef __cplusplus
}
#endif


#endif /* __STD_TREE_TYPES_H__ */
