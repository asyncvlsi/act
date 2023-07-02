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
  id->ispassthru = 0;
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

void lapply_X_one_instance_0_1 (VNet *, id_info_t *, id_info_t *);

id_info_t *verilog_gen_const (VNet *v, int zero_or_one)
{
  const char *cellname, *pinname, *sig;
  int threshold;
  if (zero_or_one) {
    cellname = "v2act.tie.hi.cell";
    pinname = "v2act.tie.hi.pin";
    sig = "Vdd";
  }
  else {
    cellname = "v2act.tie.lo.cell";
    pinname = "v2act.tie.lo.pin";
    sig = "GND";
  }
  
  if ((zero_or_one == 0 && CURMOD(v)->tielo) ||
      (zero_or_one == 1 && CURMOD(v)->tiehi) || config_exists (cellname)) {
    threshold = config_get_int ("v2act.tie.fanout_limit");
    if (!(zero_or_one ? CURMOD(v)->tiehi : CURMOD(v)->tielo) ||
	(threshold > 0 &&
	 ((zero_or_one ? CURMOD(v)->tiehi_cnt : CURMOD(v)->tielo_cnt)
	  >= threshold))) {
      conn_info_t *ci;
      id_info_t *id, *mod;
      char buf[100];
      id_info_t *newsig;
      id_info_t *tmp_prefix;
	
      // create instance
      snprintf (buf, 100, "_nref_%d", v->flop_count++);
      id = verilog_gen_id (v, buf);
      id->ismodname = 0;
      mod = verilog_find_id (v, config_get_string (cellname));
      if (mod) {
	v->flag = 0;
      }
      else {
	v->flag = 1;
	mod = verilog_gen_id (v, config_get_string (cellname));
      }
      mod->ismodname = 1;

      tmp_prefix = v->prefix;
      lapply_X_one_instance_0_1 (v, mod, id);
      
      snprintf (buf, 100, "_nsig_%d", v->flop_count++);
      
      newsig = verilog_gen_id (v, buf);
      
      if (zero_or_one) {
	CURMOD(v)->tiehi = newsig;
	CURMOD(v)->tiehi_cnt = 0;
      }
      else {
	CURMOD(v)->tielo = newsig;
	CURMOD(v)->tielo_cnt = 0;
      }

      NEW (ci, conn_info_t);
      ci->l = NULL;
      ci->isclk = 0;
      NEW (ci->r, conn_rhs_t);
      ci->r->id.id = newsig;
      ci->r->id.isderef = 0;
      ci->r->id.cnt = 0;
      ci->r->issubrange = 0;
      ci->prefix = v->prefix;
      ci->id.id = verilog_alloc_id (config_get_string (pinname));
      ci->id.cnt = 0;
      ci->id.isderef = 0;
      A_NEW (CURMOD(v)->conn, conn_info_t *);
      A_NEXT (CURMOD(v)->conn) = ci;
      A_INC (CURMOD(v)->conn);
      
      v->prefix = tmp_prefix;
    }
    if (zero_or_one) {
      CURMOD(v)->tiehi_cnt++;
      return CURMOD(v)->tiehi;
    }
    else {
      CURMOD(v)->tielo_cnt++;
      return CURMOD(v)->tielo;
    }
  }
  else {
    id_info_t *id = verilog_gen_id (v, sig);
    id->isport = 1;
    return id;
  }
}

int array_length (conn_info_t *c)
{
  conn_rhs_t *r;
  int x;
  listitem_t *li;

  if (c->r) {
    /* simple */
    r = c->r;
    if (r->id.isderef) {
      /* not an array */
      return -1;
    }
    else {
      if (r->issubrange) {
	return (r->hi - r->lo + 1);
      }
      else {
	return -1;
      }
    }
  }
  else {
    x = 0;
    for (li = list_first (c->l); li; li = list_next (li)) {
      r = (conn_rhs_t *)list_value (li);
      if (r->id.isderef) {
	x += 1;
      }
      else {
	if (r->issubrange) {
	  x += (r->hi - r->lo + 1);
	}
	else {
	  x += 1;
	}
      }
    }
  }
  return x;
}


/*
 * update ID info
 *
 */
void update_id_info (id_info_t *id)
{
  if (id->m) {
    id->m->inst_exists = 1;
    id->nused = A_LEN (id->m->port_list);
    id->p = NULL;
  }
  if (id->nused > 0) {
    MALLOC (id->used, int, id->nused);
    for (int i=0; i < id->nused; i++) {
      id->used[i] = 0;
    }
  }
}

void update_conn_info (id_info_t *id)
{
  if (!id->m) return;
  if (id->mod && id->conn_start != -1) {
    for (int i=id->conn_start; i <= id->conn_end; i++) {
      int k;
      //printf ("mod len = %d\n", A_LEN (id->m->port_list));
      if (id->mod->conn[i]->id.id) {
	for (k=0; k < A_LEN (id->m->port_list); k++) {
	  if (strcmp (id->mod->conn[i]->id.id->myname,
		      id->m->port_list[k]->myname) == 0) {
	    id->used[k] = 1;
	    break;
	  }
	}
	if (k == A_LEN (id->m->port_list)) {
	  fatal_error ("Connection to unknown port `%s' for %s?", id->mod->conn[i]->id.id->myname, id->m->b->key);
	}
      }
      else {
	k = id->mod->conn[i]->id.cnt;
	if (k >= A_LEN (id->m->port_list))
	  fatal_error ("Not enough ports in definition for `%s'?",
		       id->m->b->key);
	id->used[k] = 1;
	id->mod->conn[i]->id.id = verilog_alloc_id (id->m->port_list[k]->myname);
      }
    }
  }
}


Process *verilog_find_lib (Act *a, const char *nm)
{
  char buf[10240];

  snprintf (buf, 10240, "%s::%s", lib_namespace, nm);
  return a->findProcess (buf);
}
