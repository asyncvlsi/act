/*************************************************************************
 *
 *  Copyright (c) 2026 Rajit Manohar
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
#ifndef __ACT_ARGS_H__
#define __ACT_ARGS_H__

#include <common/hash.h>

/*
 * A simple implementation of getopt that doesn't use any static
 * variables.
 *
 * The opts string is the usual argument to the standard C getopt()
 * function. The hashtable returns the options parsed, and the integer
 * argc is updated to the place in the argv array that has arguments
 * that were not parsed.
 *
 * The returned hash table has an entry for each option that exists.
 * For options that are flags, it returns a count.
 * For options that are strings, pointer is the string.
 * Missing entry in hash table is unspecified option.
 * Returns NULL on error.
 *
 */

struct Hashtable *std_getopt (int *argc, char **argv, const char *opts);

/*
 * Frees the hash table and its entries. We need the options used to
 * construct the hash table so we can release string storage correctly.
 */
void std_opt_free (struct Hashtable *H, const char *opts);


#endif /* __ACT_ARGS_H__ */
