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
#include <string.h>


/*------------------------------------------------------------------------
 *
 *
 *  Process methods
 *
 *
 *------------------------------------------------------------------------
 */

Process::Process (UserDef *u) : UserDef (*u)
{
  is_cell = 0;
  b = NULL;
  ifaces = NULL;
  has_refinement = 0;
  changelist = NULL;
  bufcnt = 0;

  /* copy over userdef */
  MkCopy (u);
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

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Process *> (ux);
  }

  xp = new Process (ux);
  delete ux;

  Assert (ns->EditType (xp->name, xp) == 1, "What?");
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
  return xp;
}

int Process::isBlackBox ()
{
  if (isExpanded()) {
    Assert (unexpanded, "What?");
    return unexpanded->isDefined() && (unexpanded->getBody() == NULL);
  }
  else {
    return isDefined () && (getBody() == NULL);
  }
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


const char *Process::addBuffer (char *name, ActId *port, Process *buf)
{
  if (!name || !port || !buf || !buf->isExpanded ()) {
    return NULL;
  }
  if (port->Rest()) {
    return NULL;
  }

  if (buf->getNumPorts() != 2) {
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

  int is_orig_input;

  if (it->getDir() == Type::IN) {
    is_orig_input = 1;
  }
  else if (it->getDir() == Type::OUT) {
    is_orig_input = 0;
  }
  else {
    /* we don't know if this is an input pin or output pin */
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
  nit = nit->Expand (NULL, CurScope());

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

  ActId *tmpport;

  tmpport = new ActId (bufnm);
  tmpport->Append (new ActId (buf->getPortName (iport)));
  act_connection *in_conn = tmpport->Canonical (CurScope());
  delete tmpport;

  tmpport = new ActId (bufnm);
  tmpport->Append (new ActId (buf->getPortName (oport)));
  act_connection *out_conn = tmpport->Canonical (CurScope());
  delete tmpport;

  if (is_orig_input) {
    act_mk_connection (this, bufnm, in_conn, tmp->getName(), orig_c);
    act_mk_connection (this, bufnm, out_conn, name, c);
  }
  else {
    act_mk_connection (this, bufnm, in_conn, name, c);
    act_mk_connection (this, bufnm, out_conn, tmp->getName(), orig_c);
  }

  delete tmp;

  vx = I->LookupVal (bufnm);
  Assert (vx, "What?");
  return vx->u.obj.name;
}
