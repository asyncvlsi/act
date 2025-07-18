/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#include <act/types.h>
#include <act/body.h>
#include <act/lang.h>
#include <act/value.h>
#include <act/act.h>
#include <string.h>

int _act_inc_rec_depth ();
void _act_dec_rec_depth();

/*------------------------------------------------------------------------
 *
 *
 *  Process methods
 *
 *
 *------------------------------------------------------------------------
 */
Process::Process (UserDef *u) : UserDef (u)
{
  is_cell = 0;
  b = NULL;
  ifaces = NULL;
  bufcnt = 0;
  used_globals = NULL;
}

Process::~Process ()
{
  if (b) {
    delete b;
  }
  if (ifaces) {
    list_free (ifaces);
    ifaces = NULL;
  }
}


Process *Process::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Process *xp;
  UserDef *ux;
  int cache_hit;

  // This can happen during dynamic instance addition!
  if (expanded) {
    return this;
  }

  if (!ns->CurScope()->isExpanded()) {
    ActNamespace::Global()->Expand ();
  }
  Assert (ns->CurScope()->isExpanded(), "Hmm...");

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit, 1);

  if (cache_hit) {
    return dynamic_cast<Process *> (ux);
  }

  xp = new Process (ux);
  ActNamespace::Act()->updateType (ux, xp);
  delete ux;

  Assert (_ns->EditType (xp->name, xp) == 1, "What?");
  xp->is_cell = is_cell;

  if (ifaces) {
    listitem_t *li;
    xp->ifaces = list_new ();
    for (li = list_first (ifaces); li; li = list_next (li)) {
      InstType *iface = (InstType *)list_value (li);
      Assert (list_next (li), "What?");
      list_t *lmap = (list_t *)list_value (list_next (li));
      li = list_next (li);
      list_append (xp->ifaces, iface->Expand (ns, xp->I));
      list_append (xp->ifaces, lmap);
    }
  }
  else {
    xp->ifaces = NULL;
  }

  int recval = _act_inc_rec_depth ();

  if (recval >= Act::max_recurse_depth) {
    act_error_ctxt (stderr);
    fatal_error ("Exceeded maximum recursion depth of %d\n", Act::max_recurse_depth);
  }

  /*-- expand macros --*/
  for (int i=0; i < A_LEN (um); i++) {
    A_NEW (xp->um, UserMacro *);
    A_NEXT (xp->um) = um[i]->Expand (xp, ns, xp->I, 1);
    A_INC (xp->um);
  }

  const char *ret = xp->validateInterfaces ();
  if (ret) {
    act_error_ctxt (stderr);
    fatal_error ("Inconsistent/missing method\n\t%s", ret);
  }

  _act_dec_rec_depth ();

  return xp;
}

int Process::isBlackBox ()
{
  Process *p = this;
  if (isExpanded()) {
    Assert (unexpanded, "What?");
    p = dynamic_cast<Process *> (unexpanded);
    Assert (p, "What?!");
  }
  if (p->isDefined ()) {
    if (p->getBody () == NULL) return 1;
    for (ActBody *tmp = p->getBody(); tmp; tmp = tmp->Next()) {
      ActBody_Inst *bi = dynamic_cast<ActBody_Inst *>(tmp);
      if (!bi) {
	return 0;
      }
      if (!TypeFactory::isBoolType (bi->getType())) {
	return 0;
      }
    }
    return 1;
  }
  return 0;
}

int Process::isLowLevelBlackBox ()
{
  if (isBlackBox()) {
    return 1;
  }
  if (!isLeaf()) {
    return 0;
  }
  if (getlang() && getlang()->hasCktLang() && !getlang()->hasNetlistLang()) {
    return 1;
  }
  return 0;
}

void Process::Print (FILE *fp)
{
  if (isCell()) {
    PrintHeader (fp, "defcell");
  }
  else {
    PrintHeader (fp, "defproc");
  }
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }

  /* print language bodies */
  lang->Print (fp);

  if (A_LEN (um) > 0) {
    fprintf (fp, " methods {\n");
    for (int i=0; i < A_LEN (um); i++) {
      um[i]->Print (fp);
    }
    fprintf (fp, " }\n");
  }

  fprintf (fp, "}\n\n");
}


void Process::addIface (InstType *iface, list_t *lmap)
{
  if (!ifaces) {
    ifaces = list_new ();
  }
  list_append (ifaces, iface);
  list_append (ifaces, lmap);
}

int Process::hasIface (InstType *x, int weak)
{
  listitem_t *li;
  if (!ifaces) return 0;
  for (li = list_first (ifaces); li; li = list_next (li)) {
    InstType *itmp = (InstType *)list_value (li);
    Assert (itmp, "What?");
    Interface *tmp = dynamic_cast <Interface *>(itmp->BaseType());
    Assert (tmp, "What?");
    if (itmp->isEqual (x, weak)) {
      return 1;
    }
    li = list_next (li);
  }
  return 0;
}

list_t *Process::findMap (InstType *x)
{
  listitem_t *li;

  if (!ifaces) return NULL;

  Array *xtmp = x->arrayInfo();
  x->clrArray();

  for (li = list_first (ifaces); li; li = list_next (li)) {
    InstType *itmp = (InstType *)list_value (li);
    Assert (itmp, "What?");
    Interface *tmp = dynamic_cast <Interface *>(itmp->BaseType());
    Assert (tmp, "What?");

    if (itmp->isEqual (x)) {
      x->MkArray (xtmp);
      return (list_t *)list_value (list_next (li));
    }
    li = list_next (li);
  }
  x->MkArray (xtmp);
  return NULL;
}


/*-- edit APIs --*/

bool Process::updateInst (char *name, Process *t)
{
  InstType *x;
  if (!t || !name || !t->isExpanded()) {
    return false;
  }

  if (!t->isCell()) {
    return false;
  }
  
  x = I->Lookup (name);
  if (x == NULL) {
    return false;
  }

  if (!TypeFactory::isProcessType (x)) {
    return false;
  }

  Process *orig = dynamic_cast<Process *> (x->BaseType ());
  Assert (orig, "How did we get here?");

  /* check that the number of ports match */
  if (orig->getNumPorts() != t->getNumPorts()) {
    return false;
  }

  /* check that the port names match, and the port types match */
  for (int i=0; i < orig->getNumPorts(); i++) {
    if (strcmp (orig->getPortName (i),
		t->getPortName (i)) != 0) {
      return false;
    }
    if (!orig->getPortType (i)->isEqual (t->getPortType (i), 1)) {
      return false;
    }
  }

  ValueIdx *vx = I->LookupVal (name);
  Assert (vx, "How did the normal lookup succeed?");

  vx->t = vx->t->refineBaseType (t);

  return true;
}


const char *Process::addBuffer (char *name, ActId *port, Process *buf,
				bool assume_input)
{
  if (!name || !port || !buf) {
    warning ("Process::addBuffer() failed: missing argument?");
    return NULL;
  }
  if (!buf->isExpanded ()) {
    warning ("Process::addBuffer() failed: %s is not expanded", buf->getName());
    return NULL;
  }
  if (port->Rest()) {
    warning ("Process::addBuffer() failed: complex ports not supported");
    fprintf (stderr, "  > ");
    port->Print (stderr);
    fprintf (stderr, "\n");
    return NULL;
  }

  if (buf->getNumPorts() != 2) {
    warning ("Process::addBuffer() failed: %s: number of ports != 2",
	     buf->getName());
    return NULL;
  }

#if 0
  /*-- allow multi-cell buffers by removing this block --*/
  if (!buf->isCell()) {
    return NULL;
  }
#endif
  
  InstType *x[2];
  x[0] = buf->getPortType (0);
  x[1] = buf->getPortType (1);

  int iport, oport;

  if (!TypeFactory::isBoolType (x[0]) ||
      !TypeFactory::isBoolType (x[1])) {
    warning ("Process::addBuffer() failed: %s: ports are not of bool type",
	     buf->getName());
    return NULL;
  }
  if ((x[0]->arrayInfo() && x[0]->arrayInfo()->size() != 1)
      || (x[1]->arrayInfo() && x[1]->arrayInfo()->size() != 1)) {
    warning ("Process::addBuffer() failed: %s: arrays of size > 1 not permitted in ports",
	     buf->getName());
    return NULL;
  }

  if (x[0]->getDir() == Type::IN) {
    iport = 0;
    oport = 1;
  }
  else {
    iport = 1;
    iport = 0;
  }

  if (x[iport]->getDir () != Type::IN ||
      x[oport]->getDir () != Type::OUT) {
    warning ("Process::addBuffer() failed: %s ports need direction flags",
	     buf->getName());
    return NULL;
  }

  ValueIdx *vx = I->LookupVal (name);
  if (!vx) {
    warning ("Process::addBuffer() failed: %s not found", name);
    return NULL;
  }
  if (vx->t->arrayInfo()) {
    warning ("Process::addBuffer() failed: %s is an array", name);
    return NULL;
  }
  if (!TypeFactory::isProcessType (vx->t)) {
    warning ("Process::addBuffer() failed: %s is not a process", name);
    return NULL;
  }

  Process *orig = dynamic_cast<Process *> (vx->t->BaseType());
  Assert (orig, "How did we get here?");

  int pos = orig->FindPort (port->getName());
  if (pos <= 0) {
    warning ("Process::addBuffer() failed: %s is not a port for %s",
	     port->getName(), name);
    return NULL;
  }
  pos--;

  InstType *it = orig->getPortType (pos);
  if (!TypeFactory::isBoolType (it)) {
    warning ("Process::addBuffer() failed: %s.%s is not a bool port",
	     name, port->getName());
    return NULL;
  }

  int is_orig_input;

  if (it->getDir() == Type::IN) {
    is_orig_input = 1;
  }
  else if (it->getDir() == Type::OUT) {
    is_orig_input = 0;
  }
  else {
    /* we don't know if this is an input pin or output pin */
    if (assume_input) {
      is_orig_input = 1;
    }
    else {
      warning ("Process::addBuffer() failed: %s.%s has no direction flag",
	       name, port->getName());
      return NULL;
    }
  }

  int array_idx = -1;

  if (it->arrayInfo() && port->arrayInfo()) {
    if (!port->arrayInfo()->isDeref()) {
      return NULL;
    }
    array_idx = it->arrayInfo()->Offset (port->arrayInfo());
    if (array_idx == -1) {
      return NULL;
    }
  }
  else if (it->arrayInfo() || port->arrayInfo()) {
    return NULL;
  }

  /*
     Step 1: Disconnect name.pos
  */
  ActId *tmp = new ActId (name);
  tmp->Append (port);
  act_connection *orig_c = tmp->Canonical (CurScope());
  act_connection *c = tmp->myConnection (CurScope());
  c->disconnect ();
  delete tmp;

  /*
     Step 2: add instance
  */
  char bufnm[100];
  do {
    snprintf (bufnm, 100, "cxb%d", bufcnt++);
  } while (I->Lookup (bufnm));
  InstType *nit = new InstType (CurScope(), buf, 0);

  nit = nit->Expand (getns(), CurScope());

  Assert (CurScope()->Add (bufnm, nit), "What?");

  /*
    Step 3: wire it up

    instance . inport = c   [c corresponds to name]
                            [need connection pointers for the new instance]

    instance . outport = orig_c
                            [orig_c corresponds to something else]
			    [need connection pointers for new
                            instance]
  */
  tmp = orig_c->toid();

  ActId *tmpport, *tmpport2;

  tmpport = new ActId (bufnm);
  tmpport->Append (new ActId (buf->getPortName (iport)));

  if (x[iport]->arrayInfo()) {
    ActId *rest = tmpport->Rest();
    Array *tmpa = x[iport]->arrayInfo()->unOffset (0);
    rest->setArray (tmpa);
  }
  act_connection *in_conn = tmpport->Canonical (CurScope());

  tmpport2 = new ActId (bufnm);
  tmpport2->Append (new ActId (buf->getPortName (oport)));

  if (x[oport]->arrayInfo()) {
    ActId *rest = tmpport2->Rest();
    Array *tmpa = x[oport]->arrayInfo()->unOffset (0);
    rest->setArray (tmpa);
  }
  act_connection *out_conn = tmpport2->Canonical (CurScope());

  if (is_orig_input) {
    ActId *tmpc = c->toid();
    act_mk_connection (this, tmpport, in_conn, tmp, orig_c);
    act_mk_connection (this, tmpport2, out_conn, tmpc, c);
    delete tmpc;
  }
  else {
    ActId *tmpc = c->toid();
    act_mk_connection (this, tmpport, in_conn, tmpc, c);
    act_mk_connection (this, tmpport2, out_conn, tmp, orig_c);
    delete tmpc;
  }
  delete tmpport;
  delete tmpport2;
  delete tmp;

  vx = I->LookupVal (bufnm);
  Assert (vx, "What?");
  return vx->u.obj.name;
}


const char *Process::addBuffer (Process *buf, list_t *inst_ports)
{
  if (!inst_ports || list_isempty (inst_ports) ||(list_length (inst_ports) % 2)
      ||  !buf || !buf->isExpanded ()) {
    return NULL;
  }

  if (buf->getNumPorts() != 2) {
    return NULL;
  }

  if (!buf->isCell()) {
    return NULL;
  }

  InstType *x[2];
  x[0] = buf->getPortType (0);
  x[1] = buf->getPortType (1);

  int iport, oport;

  if (!TypeFactory::isBoolType (x[0]) ||
      !TypeFactory::isBoolType (x[1])) {
    return NULL;
  }
  if (x[0]->arrayInfo() || x[1]->arrayInfo()) {
    return NULL;
  }

  if (x[0]->getDir() == Type::IN) {
    iport = 0;
    oport = 1;
  }
  else {
    iport = 1;
    iport = 0;
  }

  if (x[iport]->getDir () != Type::IN ||
      x[oport]->getDir () != Type::OUT) {
    return NULL;
  }

  /* check ports */
  listitem_t *li;
  li = list_first (inst_ports);
  while (li) {
    ActId *port;
    char *name;

    name = (char *) list_value (li);

    li = list_next (li);
    port = (ActId *) list_value (li);
    if (port->Rest()) {
      return NULL;
    }

    ValueIdx *vx = I->LookupVal (name);
    if (!vx) {
      return NULL;
    }
    if (vx->t->arrayInfo()) {
      return NULL;
    }
    if (!TypeFactory::isProcessType (vx->t)) {
      return NULL;
    }
    Process *orig = dynamic_cast<Process *> (vx->t->BaseType());
    Assert (orig, "How did we get here?");

    int pos = orig->FindPort (port->getName());
    if (pos <= 0) {
      return NULL;
    }
    pos--;

    InstType *it = orig->getPortType (pos);
    if (!TypeFactory::isBoolType (it)) {
      return NULL;
    }

    if (it->getDir() == Type::IN) {
      /* okay! */
    }
    else {
      return NULL;
    }

    int array_idx = -1;

    if (it->arrayInfo() && port->arrayInfo()) {
      if (!port->arrayInfo()->isDeref()) {
	return NULL;
      }
      array_idx = it->arrayInfo()->Offset (port->arrayInfo());
      if (array_idx == -1) {
	return NULL;
      }
    }
    else if (it->arrayInfo() || port->arrayInfo()) {
      return NULL;
    }
    li = list_next (li);
  }

  li = list_first (inst_ports);
  act_connection *xp = NULL;
  while (li) {
    ActId *port;
    char *name;

    name = (char *) list_value (li);
    li = list_next (li);
    port = (ActId *) list_value (li);
    li  = list_next (li);

    /*
      Step 1: Disconnect name.pos
    */
    ActId *tmp = new ActId (name);
    tmp->Append (port);
    act_connection *orig_c = tmp->Canonical (CurScope());
    act_connection *c = tmp->myConnection (CurScope());
    tmp->prune();
    delete tmp;
    if (!c->disconnectable()) {
      return NULL;
    }
    if (xp) {
      if (orig_c != xp) {
	return NULL;
      }
    }
    else {
      xp = orig_c;
    }
  }
  Assert (xp, "What?");

  /*
     Step 2: add instance
  */
  char bufnm[100];
  do {
    snprintf (bufnm, 100, "cxb%d", bufcnt++);
  } while (I->Lookup (bufnm));
  InstType *nit = new InstType (CurScope(), buf, 0);
  nit = nit->Expand (NULL, CurScope());

  Assert (CurScope()->Add (bufnm, nit), "What?");

  /*
    Step 3: wire it up
  */

  ActId *tmp = xp->toid();

  ActId *tmpport, *tmpport2;

  tmpport = new ActId (bufnm);
  tmpport->Append (new ActId (buf->getPortName (iport)));
  act_connection *in_conn = tmpport->Canonical (CurScope());

  tmpport2 = new ActId (bufnm);
  tmpport2->Append (new ActId (buf->getPortName (oport)));
  act_connection *out_conn = tmpport2->Canonical (CurScope());

  /* connect the input of the buffer */
  act_mk_connection (this, tmpport, in_conn, tmp, xp);
  delete tmp;
  delete tmpport;

  /* disconnect all the parts, and connect them to the output */
  li = list_first (inst_ports);
  while (li) {
    ActId *port;
    char *name;

    name = (char *) list_value (li);
    li = list_next (li);
    port = (ActId *) list_value (li);
    li  = list_next (li);

    /*
      Disconnect name.pos and connect it to the buffer output
    */
    ActId *ltmp = new ActId (name);
    ltmp->Append (port);
    act_connection *c = ltmp->myConnection (CurScope());

    c->disconnect ();
    act_mk_connection (this, tmpport2, out_conn, ltmp, c);
    delete ltmp;
  }
  delete tmpport2;

  ValueIdx *vx = I->LookupVal (bufnm);
  Assert (vx, "What?");
  return vx->u.obj.name;
}



int Process::findGlobal (ActId *id)
{
  listitem_t *li;
  if (!used_globals) {
    return 0;
  }
  for (li = list_first (used_globals); li; li = list_next (li)) {
    ActId *tmp = (ActId *)list_value (li);
    if (tmp->isEqual (id)) {
      return 1;
    }
  }
  return 0;
}

int Process::findGlobal (const char *s)
{
  listitem_t *li;
  if (!used_globals) {
    return 0;
  }
  for (li = list_first (used_globals); li; li = list_next (li)) {
    ActId *tmp = (ActId *)list_value (li);
    if (strcmp (s, tmp->getName()) == 0) {
      return 1;
    }
  }
  return 0;
}


void Process::recordGlobal (ActId *id)
{
  if (!used_globals) {
    used_globals = list_new ();
  }
  if (findGlobal (id)) return;
  list_append (used_globals, id->Clone());
}


const char *Process::validateInterfaces ()
{
  if (!ifaces) return NULL;
  listitem_t *li;
  for (li = list_first (ifaces); li; li = list_next (li)) {
    InstType *x = (InstType *) list_value (li);
    Interface *ix = dynamic_cast<Interface *> (x->BaseType());
    Assert (ix, "What?!");

    // now typecheck macros
    const char *ret = ix->validateMacros (this, um, A_LEN (um));
    if (ret) {
      return ret;
    }

    li = list_next (li);
  }
  return NULL;
}
