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
#include <stdio.h>
#include <string.h>
#include "misc.h"
#include "args.h"

/*------------------------------------------------------------------------

  Standard getopt-style parsing

  Returns a hash table with each option that exists.
  For options that are flags, it returns a count.
  For options that are strings, pointer is the string.
  Missing entry in hash table is unspecified option.

  Returns NULL on error.

  *argc is updated to optind, the remaining args.

--------------------------------------------------------------------------*/
struct Hashtable *std_getopt (int *argc, char **argv, const char *opts)
{
  int ch;
  struct Hashtable *H;
  hash_bucket_t *b;
  unsigned char tab[256];
  
  H = hash_new (4);

  for (int i=0; i < 256; i++) {
    tab[i] = 2;
  }
  for (int i=0; opts[i]; i++) {
    if (opts[i+1] == ':') {
      tab[(int)opts[i]] = 1; // string
    }
    else {
      tab[(int)opts[i]] = 0; // not string
    }
  }

  int i;
  char str[2];
  for (i=1; i < *argc; i++) {
    if (argv[i][0] == '-') {
      int pos;
      if (argv[i][1] == '\0' || argv[i][1] == '-') {
	// end of options
	break;
      }
      pos = 1;
      while (argv[i][pos]) {
	bool err_flag = false;
	str[0] = argv[i][pos];
	str[1] = '\0';
	if (tab[(int)argv[i][pos]] == 1) {
	  // string
	  b = hash_lookup (H, str);
	  if (b) {
	    FREE (b->v);
	  }
	  else {
	    b = hash_add (H, str);
	  }
	  if (argv[i][pos+1] == '\0') {
	    if (i != (*argc)-1) {
	      b->v = Strdup (argv[i+1]);
	      i++;
	      pos = strlen (argv[i])-1;
	    }
	    else {
	      b->v = NULL;
	      err_flag = true;
	      printf ("Error in option parsing: missing argument to `-%c'\n",
		      argv[i][pos]);
	    }
	  }
	  else {
	    b->v = Strdup (argv[i]+pos+1);
	    pos += strlen (argv[i]+pos);
	  }
	}
	else if (tab[(int)argv[i][pos]] == 0) {
	  b = hash_lookup (H, str);
	  if (b) {
	    b->i++;
	  }
	  else {
	    b = hash_add (H, str);
	    b->i = 1;
	  }
	}
	else {
	  printf ("Error in option parsing: unknown option `-%c'\n",
		  argv[i][pos]);
	  err_flag = true;
	}
	if (err_flag) {
	  // error in option parsing
	  for (int j=0; j < 256; j++) {
	    if (tab[j] == 1) {
	      str[0] = (char) j;
	      str[1] = '\0';
	      b = hash_lookup (H, str);
	      if (b && b->v) {
		FREE (b->v);
	      }
	    }
	  }
	  hash_free (H);
	  return NULL;
	}
	pos++;
      }
    }
  }
  *argc = i;
  return H;
}


void std_opt_free (struct Hashtable *H, const char *opts)
{
  int tab[256];
  
  for (int i=0; i < 256; i++) {
    tab[i] = 2;
  }
  for (int i=0; opts[i]; i++) {
    if (opts[i+1] == ':') {
      tab[(int)opts[i]] = 1; // string
    }
    else {
      tab[(int)opts[i]] = 0; // not string
    }
  }

  hash_iter_t it;
  hash_bucket_t *b;
  hash_iter_init (H, &it);
  while ((b = hash_iter_next (H, &it))) {
    if (tab[(int)b->key[0]] == 1) {
      if (b->v) {
	FREE (b->v);
      }
    }
  }
  hash_free (H);
}

