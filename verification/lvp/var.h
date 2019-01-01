/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __VAR_H__
#define __VAR_H__

#include "hash.h"
#include "bool.h"
#include "dots.h"
#include "ext.h"

#define VAR_INIT_SIZE 128

typedef struct edgelist edgelist_t;

#define N_TYPE EXT_FET_NTYPE
#define P_TYPE EXT_FET_PTYPE

#define VAR_OUTPUT   0x1	/* node is the output of an operator */
#define VAR_INPUT    0x2	/* node is an input to an operator */
#define VAR_VISITED  0x4	/* visited flag, for various traversals */

#define VAR_HAVEVAR  0x8	/* set when the node has an associated */
				/* bool_var_t */

#define VAR_IO      (VAR_INPUT|VAR_OUTPUT)	/* VAR_INPUT|VAR_OUTPUT */

#define VAR_WEAKUP   0x10	/* ^weak on a p transistor */
#define VAR_WEAKDN   0x20	/* ^weak on an n transistor */

#define VAR_PRUNEN    0x40	/* sneak path pruning, n network */
#define VAR_PRUNEP    0x80	/* sneak path pruning, p network */

#define VAR_PRUNE(type) (VAR_PRUNEN << (type))
#define VAR_PRUNED  (VAR_PRUNEN|VAR_PRUNEP)

#define VAR_MARKED   0x100	/* marked flag to prevent excllo/exclhi */
				/* list recomputation in local invariant */
				/* calculation */

#define VAR_SNEAKEDP  0x200	/* mark subnet for sneak path check */
#define VAR_SNEAKEDN  0x400	/* (n network) */

#define VAR_SKIPSNEAKP 0x800	/* skip sneak path check because node */
#define VAR_SKIPSNEAKN 0x1000	/* had a prs error */

#define VAR_RESET_SUPPLY 0x2000  
   /* node has an edge to vdd/gnd through reset transistor which will be
      always on after reset goes away
    */

#define VAR_DRIVENOUTPUT 0x4000	/* node is driven by transistors */

#define VAR_HIER_NODE    0x8000	/* node is in a .hxt subcell */

#define VAR_EXPORT       0x10000 /* export this node to the .hxt file */

#define VAR_HOUTPUT      0x20000 /* flag to propagate hxt output nodes */

#define VAR_DUMPEDL      0x40000 /* already dumped node to hxt file */

#define VAR_PRSOUTPUT    0x80000 /* marked as output in prs file */

#define VAR_GLOBALNODE   0x100000 /* ! */

#define VAR_STATEHOLDING 0x200000 /* node is state-holding */

#define VAR_UNSTAT       0x400000 /* node should not be staticized */

#define VAR_COUNTED      0x800000 /* node encountered already */

#define VAR_VISITED2     0x1000000 /* an input to the operator currently */
				   /* being analyzed for charge-sharing */

#define VAR_ON           0x2000000 /* charge sharing case, the gate */
				   /* connected to this node is on */



/**------------------------------------------------------------------------**/
/* 
   Flags from the flags2 field
*/

#define VAR_PCHG         EXT_ATTR_PCHG	/* precharged internal nodes */
#define VAR_PCHG_NUP     EXT_ATTR_NUP
#define VAR_PCHG_NDN     EXT_ATTR_NDN
#define VAR_PCHG_PUP     EXT_ATTR_PUP
#define VAR_PCHG_PDN     EXT_ATTR_PDN
#define VAR_VOLTAGE_CONV EXT_ATTR_VC   /* n->vdd path for voltage converter; skip */

#define VAR_PCHG_ALL (VAR_PCHG|VAR_PCHG_NUP|VAR_PCHG_NDN|VAR_PCHG_PUP|VAR_PCHG_PDN)

#define VAR_CHECK_NUP   0x40	/* seen through vdd path */
#define VAR_CHECK_PDN   0x80	/* seen through gnd path */

/* combinational feedback for n/p network */
#define VAR_COMBFEEDBACK_N 0x100
#define VAR_COMBFEEDBACK_P 0x200

#define PRSFILE 0
#define STRONG 1
#define WEAK 2

struct excl {
  struct var *v;
  struct excl *next;
};

struct excllists {
  struct excl *l;
  struct excllists *next;
};

struct coupling_list {
  double cap;
  struct var *node;
  struct coupling_list *next;
};

enum fetcap_type {
  DIFF_VDD = 0, DIFF_GND = 1, DIFF_OUT = 2, DIFF_INT = 3
  /* diffusion value is vdd, gnd, output, or an internal node */
};

struct fetcap {
  double A, P;			/* area, perim */
  unsigned int S:2, D:2;
  struct fetcap *next;
};

struct capacitance {
  double p_perim, p_area;	/* area, perim of pdiffusion */
  double n_perim, n_area;	/* area, perim of ndiffusion */
				/* (in m^2) */

  struct fetcap *f[2];		/* P_TYPE, N_TYPE */
  int p_gA, p_gP;		/* area, perim of pgate */
  int n_gA, n_gP;		/* area, perim of ngate */

  struct coupling_list *cap;	/* cap to other nodes */
  double badcap;		/* lumped cap to other nodes */
  double goodcap;		/* cap to globals */
};
  
typedef struct var {
  char *s;			/* name: REQUIRED FIELD */

  /* USER DEFINED FIELDS */
  unsigned int hcell:1;		/* hierarchical cell name:
				   If true, this means we use
				   hc to point to the hierarchical
				   structure.
				 */

  unsigned int hname:1;		/* hierarchical name. If true,
				   hc points to hcell
			         */

  unsigned int inprs:1;		/* is name in prs file? */
  unsigned int inlayout:1;	/* is name in layout? */
  unsigned int checkedprs:1;	/* checked already */
  unsigned int checkedlayout:1;

  unsigned int checkedio:1;	/* checked io */

  unsigned int must_have_wf:1;	/* must have weak feedback */

  unsigned int done:1;		/* in the check, this variable is set
				 if the node has already been processed
				 because it is part of the inverter in a
				 staticizer */

  struct var *exclhi;		/* channel information */
  struct var *excllo;		/* channel information */

  struct var *worklist;		/* worklist */
  struct var *alias_prs;	/* up pointer in alias tree */
  struct var *alias;		/* up pointer in alias tree from ext file */
  struct var *alias_ring;	/* alias ring */
  struct var *alias_ring_prs;	/* alias prs ring */
  unsigned int flags;		/* 1 if output variable */
  unsigned int flags2;		/* more flags!!! */
  void *up[3], *dn[3];		/* production rules.
				   0 = from file
				   1 = from sim file, strong
				   2 = from sim file, weak 
				   */

  bool_var_t v;			/* name */
  edgelist_t *edges;		/* list of edges in netlist */

  dots_t name_convert;		/* info about position of dots */

  hash_bucket_t *hc;		/* hash cell */
  
#ifndef DIGITAL_ONLY
  struct capacitance c;		/* capacitance */
  double pthresh, nthresh;	/* thresholds for p and n type chargesharing*/
  void *space;			/* available */
#endif

} var_t;			/* type of a single variable */

struct edgelist {
  var_t *gate, *t1;
  int isweak;			/* weak transistor */
  int length, width;
  int type;			/* 0=n, 1=p */
  struct edgelist *next;
};

typedef struct {
  struct Hashtable *H;
  hash_bucket_t *e;
  int step;
} VAR_T;			/* variable table */


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                             FUNCTIONS 
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

VAR_T *var_init (void);
   /*
      Returns a fresh variable table.
   */

void var_free (VAR_T *);
   /*
      Free variable table.
   */

var_t *var_enter (VAR_T *, char *s);
   /*
      Insert "s" into the variable table checking for duplicates.
   */

var_t *var_locate (VAR_T *, char *s);
   /*
     Return variable entry associated with "s", NULL if it doesn't
     exist. Canonical names returned
   */

var_t *var_find (VAR_T *, char *s);
   /*
     Return variable entry associated with "s", NULL if it doesn't
     exist.
   */

int var_number (VAR_T *);
   /*
      Number of variables in the variable table.
    */

var_t *var_step_first (VAR_T *);
var_t *var_step_next (VAR_T *);
   /* 
      Use to iterate over all variables in the variable table.
      "var_step_first" returns NULL if the table is empty;
      "var_step_next" returns NULL when the end of table is reached.

       Example: 

         for (v = var_step_first (V); v; v = var_step_next (V)) {
	     process (v);
         }
   */

var_t *var_nice_alias (var_t *);
char *var_name(var_t *);
char *var_prs_name (var_t *);

#define var_string(v) ((v)->s)
   /*
      Given a var_t *, returns a string corresponding to the
      identifier name corresponding to the variable.
   */

#endif /* __VAR_H__ */
