/*************************************************************************
 *
 *  Macros for queue operations
 *
 *  Copyright (c) 1998, 2018, 2019 Rajit Manohar
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
#ifndef __QOPS_H__
#define __QOPS_H__

/*
 *  Queue format:
 *
 *    Two pointers: head (hd) and tail (tl)
 *    Elements have a "next" field for the queue.
 *
 *    Queue empty when hd == NULL
 *    When hd == NULL, "tl" the value of tl can be anything
 *  
 *
 */

#define q_ins(hd,tl,p)				\
  p->next = NULL;				\
  if (hd == NULL) {				\
    hd = p;					\
  }						\
  else {					\
    tl->next = p;				\
  }						\
  tl = p;


#define q_del(hd, tl, t)						 \
  if (hd) {								 \
    t = hd;								 \
    hd = hd->next;							 \
  }									 \
  else									 \
    t = NULL;

#define q_delete_item(hd,tl,prev,cur)		\
   if (!prev) {					\
     q_del(hd,tl,cur);				\
   }						\
   else {					\
     prev->next = cur->next;			\
     if (!cur->next)				\
       tl = prev;				\
   }

#define l_ins(x,p)				\
   do {						\
     (p)->next = (x);				\
     (x) = p;					\
   } while (0)

#define l_del(x,p)				\
   do {						\
     (p) = (x);					\
     (x) = (x)->next;				\
   } while (0)

#define l_step(x)				\
     (x) = (x)->next


#define q_step(x)   l_step(x)

#endif /* __QOPS_H__ */
