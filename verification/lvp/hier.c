/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

#include <stdio.h>
#include <string.h>

#include "hier.h"
#include <common/misc.h>

/*------------------------------------------------------------------------*/


char *
hier_subcell_node (VAR_T *V, char *node, var_t **hcell, char sep)
{
  char *s;
  var_t *v;

  for (s = node; *s; s++) {
    if (*s == sep) { 
      *s = '\0';
      v = var_locate (V, node);
      if (v && v->hcell) {
	*s = sep;
	if (hcell)
	  *hcell = v;
	return s+1;
      }
      *s = sep;
    }
  }
  return NULL;
}


/*------------------------------------------------------------------------*/


int
hier_notinput_subcell_node (VAR_T *V, char *node, char sep)
{
  char *s;
  var_t *v;
  hash_bucket_t *hc;
  
  for (s = node; *s; s++) {
    if (*s == sep) {
      *s = '\0';
      v = var_locate (V, node);
      if (v && v->hcell) {
	*s = sep;
	hc = hash_lookup ((struct Hashtable *)v->hc, s+1);
	if (!hc) return 1;
	if (IS_INPUT(ROOT(hc))) return 0;
	return 1;
      }
      *s = sep;
    }
  }
  return 0;
}
