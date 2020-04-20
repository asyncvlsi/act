#include <stdio.h>
#include "v_walk_X.h"
#include "v_walk.extra.h"

VRet *v_wrap_X_double (double d)
{
  return NULL;
}

VRet *v_wrap_X_int (int i)
{
  VRet *v;

  NEW (v, VRet);
  v->type = V_INT;
  v->u.i = i;

  return v;
}

VRet *v_wrap_X_list (list_t *l)
{
  return NULL;
}

VRet *v_wrap_X_string (const char *s)
{
  return NULL;
}

VRet *v_wrap_X_id_info_t_p (id_info_t *i)
{
  VRet *v;

  NEW (v, VRet);

  v->type = V_ID;
  v->u.id = i;

  return v;
}

VRet *v_wrap_X_id_deref_t_p (id_deref_t *i)
{
  return NULL;
}

VRet *v_wrap_X_conn_rhs_t_p (conn_rhs_t *c)
{
  return NULL;
}

VRet *v_wrap_X_conn_info_t_p (conn_info_t *c)
{
  return NULL;
}

/*------------------------------------------------------------------------
 *
 *  verilog_gen_id --
 *
 *   Generate new id
 *
 *------------------------------------------------------------------------
 */
id_info_t *verilog_find_id (VNet *v, const char *s)
{
  hash_bucket_t *b;
  b = hash_lookup (CURMOD(v)->H, s);
  if (!b) {
    return NULL;
  }
  else {
    return (id_info_t *)b->v;
  }
}

id_info_t *verilog_alloc_id (char *name)
{
  id_info_t *id;

  NEW (id, id_info_t);

  id->myname = name;
  id->isinput = 0;
  id->isoutput = 0;
  id->isport = 0;
  id->isinst = 0;
  id->ismodname = 0;
  id->isclk = 0;
  id->fanout = 0;
  id->curnum = 0;
  id->nm = NULL;
  id->d = NULL;
  id->next = NULL;
  id->nxt = NULL;
  id->nused = 0;
  id->conn_start = -1;
  id->conn_end = -1;
  id->mod = NULL;

  A_INIT (id->a);
  id->fa = NULL;
  id->fcur = NULL;
  return id;
}

id_info_t *verilog_gen_id (VNet *v, const char *s)
{
  hash_bucket_t *b;
  id_info_t *id;

  b = hash_lookup (CURMOD(v)->H, s);
  if (!b) {
    b = hash_add (CURMOD(v)->H, s);
    id = verilog_alloc_id (b->key);
    b->v = id;
  }
  return (id_info_t *)b->v;
}

void verilog_delete_id (VNet *v, const  char *s)
{
  hash_delete (CURMOD(v)->H, s);
}

