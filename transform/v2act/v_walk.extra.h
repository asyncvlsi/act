/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2007, 2018-2019 Rajit Manohar
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
#ifndef __V2ACT_EXTRA_H__
#define __V2ACT_EXTRA_H__

#include <string.h>
#include "v2act.h"

#define OPT_EXISTS(x)    !list_isempty (x)
#define OPT_EMPTY(x)     list_isempty (x)
#define LIST_VALUE(x)     ((VRet *)list_value (x))
#define OPT_VALUE(x)     LIST_VALUE (list_first (x))


/* parser */
id_info_t *verilog_gen_id (VNet *, const char *);
id_info_t *verilog_find_id (VNet *v, const char *s);
id_info_t *verilog_alloc_id (char *name);
void verilog_delete_id (VNet *v, const char *s);

#endif /* __V2ACT_EXTRA_H__ */
