/*************************************************************************
 *
 *  Simulation objects
 *
 *  Copyright (c) 1999, 2019 Rajit Manohar
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
#include "act/common/sim.h"

/* globals for sim object */
int Sim::Number = 0;
Sim *Sim::obj_list = NULL;

/* create and destroy */
Sim::Sim (int ready)
{
  my_id = Number++;		/* assign unique id */
  _ready = ready;

  /* insert into simulation object global list */
  if (!obj_list) {
    obj_list = this;
    next = prev = NULL;
  }
  else {
    prev = NULL;
    next = obj_list;
    obj_list->prev = this;
    obj_list = this;
  }
}

Sim::~Sim ()  
{
  /* delete myself from object list */
  Assert (obj_list, "something terrible happened!");
  if (obj_list == this) {
    Assert (prev == NULL, "obj_list == current and I'm not the head?");
    if (obj_list->next)
      obj_list->next->prev = prev;
    obj_list = obj_list->next;
  }
  else {
    Assert (prev, "not at head of list, prev == NULL?");
    prev->next = next;
    if (next)
      next->prev = prev;
  }
}


int Sim::CheckDeadlock (void)
{
  Sim *s;
  if (obj_list == NULL) return 0;

  for (s = obj_list; s; s = s->next)
    if (s->_ready == 1) break;

  if (s == NULL) return 0;

  printf ("The following tasks are suspended:\n");
  for (s = obj_list; s; s = s->next) {
    if (s->_ready == 0) continue;
    if (s->Name()) {
      printf ("\t[%d] %s\n", s->my_id, s->Name());
    }
    else {
      printf ("\t[%d] NULL\n", s->my_id);
    }
  }
  return 1;
}


void Sim::DeadlockedDumpStats (void)
{
  Sim *s;
  if (obj_list == NULL) return;

  for (s = obj_list; s; s = s->next)
    if (s->_ready == 1) break;

  if (s == NULL) return;

  for (s = obj_list; s; s = s->next) {
    if (s->_ready == 0) continue;
    s->DumpStats ();
  }
}


Time_t Sim::Now()
{
  if (obj_list) {
    return obj_list->CurTime();
  }
  else {
    return 0;
  }
}
