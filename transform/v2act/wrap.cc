#include <stdio.h>
#include "v_walk_X.h"

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

