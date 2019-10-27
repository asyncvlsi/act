/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <config.h>
#include <misc.h>
#include <array.h>
#include <act/lang.h>
#include <act/passes/netlist.h>
#include "tech.h"

#define BUF_SZ 10240
static char buf[BUF_SZ];

Technology *Technology::T = NULL;

static void tech_strname (const char *prefix, const char *s)
{
  if (prefix) {
    snprintf (buf, BUF_SZ, "%s.%s", prefix, s);
  }
  else {
    snprintf (buf, BUF_SZ, "%s", s);
  }
  if (buf[BUF_SZ-1] != '\0') {
    fatal_error ("String too long");
  }
}

static void verify_range_table (const char *nm)
{
  int i;
  int *tab;
  int sz;

  sz = config_get_table_size (nm);
  if ((sz % 2) == 0) {
    printf ("Size = %d\n", sz);
    fatal_error ("Table `%s' is not a range table (even # of entries).", nm);
  }
  tab = config_get_table_int (nm);
  for (i=2; i < sz; i += 2) {
    if (tab[i] <= tab[i-2]) {
      fatal_error ("Table `%s': ranges must be in increasing order!", nm);
    }
  }
}

#define PTYPE EDGE_PFET
#define NTYPE EDGE_NFET

/*------------------------------------------------------------------------
 *
 *   Read in the technology file and run some sanity checks, and
 *   create all the materials needed.
 *
 *------------------------------------------------------------------------
 */
void Technology::Init (const char *s)
{
  int i, j, k, sz;
  const char *tables[] = { "diff.ntype", "diff.ptype", "diff.nfet", "diff.pfet",
			   "diff.nfet_well", "diff.pfet_well" };
  const int well_start = 4;
  
  char **diff;
  const char *prefix = "layout";

  Assert (NTYPE == 0 && PTYPE == 1, "Hmm");

  Technology::T = new Technology();

  A_DECL (char *, contacts);
  A_INIT (contacts);

  config_std_path ("layout");
  if (prefix) {
    config_push_prefix (prefix);
  }

  config_read (s);
  if (prefix) {
    config_pop_prefix ();
  }

  tech_strname (prefix, "info.name");
  /* run sanity checks */
  if (!config_exists (buf) || !config_get_string (buf)) {
    fatal_error ("Technology file: missing `%s'!", buf);
  }
  T->name = config_get_string (buf);
  
  tech_strname (prefix, "info.date");
  if (!config_exists (buf) || !config_get_string (buf)) {
    fatal_error ("Technology file: missing `%s'!", buf);
  }
  T->date = config_get_string (buf);
  
  tech_strname (prefix, "general.scale");
  T->scale = config_get_real (buf);
  
  tech_strname (prefix, "general.metals");
  T->nmetals = config_get_int (buf);
  if (T->nmetals < 1) {
    fatal_error ("Need at least one metal layer!");
  }

  tech_strname (prefix, "diff.types");
  sz = config_get_table_size (buf);
  if (sz < 1) {
    fatal_error ("No diffusion types?");
  }
  if (sz != config_get_table_size ("act.dev_flavors")) {
    fatal_error ("The number of diffusion types in technology file (%d)\ndoesn't match the number of ACT device flavors (%d).\n", sz,
		 config_get_table_size ("act.dev_flavors"));
  }
  T->num_devs = sz;

  char **table_tst = config_get_table_string (buf);
  for (i=0; i < sz; i++) {
    if (strcmp (table_tst[i], act_dev_value_to_string (i)) != 0) {
      fatal_error ("Device type %d mismatch: %s v/s %s (act)\n",
		   i, table_tst[i], act_dev_value_to_string (i));
    }
  }

  /* allocate space for all the diffusion/fet stuff */
  for (i=0; i < 2; i++) {
    Assert (i == PTYPE || i == NTYPE, "Eh");
    MALLOC (T->diff[i], DiffMat *, sz);
    MALLOC (T->well[i], WellMat *, sz);
    MALLOC (T->fet[i], FetMat *, sz);
    MALLOC (T->welldiff[i], DiffMat *, sz);
  }
  
  for (i=0; i < sizeof(tables)/sizeof (tables[0]); i++) {
    tech_strname (prefix, tables[i]);
    if (sz != config_get_table_size (buf)) {
      fatal_error ("Table `%s' is not the same size is the type table", buf);
    }
    config_get_table_string (buf);
  }


  /* now: check materials! */
  for (i=0; i < 6; i++) {
    tech_strname (prefix, tables[i]);
    diff = config_get_table_string (buf);
    tech_strname (prefix, "materials.");
    k = strlen (buf);
    buf[BUF_SZ-1] = '\0';

    /* selects, etc. */
    for (j = 0; j < T->num_devs; j++) {
      if (i < 2) {
	/* ptype/ntype: diffusion */
	
	DiffMat *mat;
	mat = T->diff[i][j] = new DiffMat (diff[j]);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.width", diff[j]);
	mat->width = config_get_int (buf);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]);
	if (config_get_table_size (buf) != sz) {
	  fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	}
	mat->spacing = config_get_table_int (buf);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.oppspacing", diff[j]);
	if (config_get_table_size (buf) != sz) {
	  fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	}
	mat->oppspacing = config_get_table_int (buf);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.polyspacing", diff[j]);
	mat->polyspacing = config_get_int (buf);

	snprintf (buf+k, BUF_SZ-k-1, "%s.notchspacing", diff[j]);
	mat->notchspacing = config_get_int (buf);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.overhang", diff[j]);
	verify_range_table (buf);
	mat->overhang = new RangeTable (config_get_table_size (buf),
					config_get_table_int (buf));
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.via.edge", diff[j]);
	mat->via_edge = config_get_int (buf);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.via.fet", diff[j]);
	mat->via_fet = config_get_int (buf);

	snprintf (buf+k, BUF_SZ-k-1, "%s.minarea", diff[j]);
	if (config_exists (buf)) {
	  if (config_get_int (buf) < 0) {
	    fatal_error ("%s: has to be non-negative", buf);
	  }
	  mat->minarea = config_get_int (buf);
	}

	/* contacts */
	A_NEW (contacts, char *);
	A_NEXT (contacts) = diff[j];
	A_INC (contacts);
      }
      else if (i < 4) {
	FetMat *mat;
	mat = T->fet[i-2][j] = new FetMat (diff[j]);
	
	/* pfet/nfet: transistors */
	snprintf (buf+k, BUF_SZ-k-1, "%s.width", diff[j]);
	if (config_get_int (buf) < 1) {
	  fatal_error ("`%s': minimum width has to be at least 1", buf);
	}
	mat->width = config_get_int (buf);

	snprintf (buf+k, BUF_SZ-k-1, "%s.dummy_poly", diff[j]);
	if (config_exists (buf)) {
	  if (config_get_int (buf) < 0) {
	    fatal_error ("`%s': dummy poly has to be non-negative", buf);
	  }
	  mat->num_dummy = config_get_int (buf);
	}
	else {
	  mat->num_dummy = 0;
	}
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]);
	verify_range_table (buf);
	mat->spacing = new RangeTable (config_get_table_size (buf),
				       config_get_table_int (buf));
      }
      else if (i < 6) {
	/* wells */

	if (strcmp (diff[j], "") != 0) {
	  /* there is a well/welldiff */
	  char ldiff[1024];
	  int ik;

	  ldiff[0] = '\0';
	  for (ik=0; diff[j][ik]; ik++) {
	    ldiff[ik] = diff[j][ik];
	    ldiff[ik+1] = '\0';
	    if (diff[j][ik] == ':') {
	      ldiff[ik] = '\0';
	      break;
	    }
	  }

	  if (diff[j][ik] == ':') {
	    if (diff[j][ik+1] == '\0') {
	      T->welldiff[i-4][j] = NULL;
	    }
	    else {
	      DiffMat *mat;
	      mat = T->welldiff[i-4][j] = new DiffMat (diff[j]+ik+1);
	      snprintf (buf+k, BUF_SZ-k+1, "%s.width", diff[j]+ik+1);
	      mat->width = config_get_int (buf);
	      
	      snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]+ik+1);
	      if (config_get_table_size (buf) != sz) {
		fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	      }
	      mat->spacing = config_get_table_int (buf);

	      snprintf (buf+k, BUF_SZ-k-1, "%s.oppspacing", diff[j]+ik+1);
	      if (config_get_table_size (buf) != sz) {
		fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	      }
	      mat->oppspacing = config_get_table_int (buf);
	
	      snprintf (buf+k, BUF_SZ-k-1, "%s.polyspacing", diff[j]+ik+1);
	      mat->polyspacing = config_get_int (buf);

	      snprintf (buf+k, BUF_SZ-k-1, "%s.minarea", diff[j]+ik+1);
	      if (config_exists (buf)) {
		if (config_get_int (buf) < 0) {
		  fatal_error ("%s: has to be non-negative", buf);
		}
		mat->minarea = config_get_int (buf);
	      }
	      
	      A_NEW (contacts, char *);
	      A_NEXT (contacts) = Strdup (diff[j]+ik+1);
	      A_INC (contacts);
	    }
	  }

	  if (ldiff[0] != '\0') {

	    WellMat *mat;
	    mat = T->well[i-4][j] = new WellMat (Strdup (ldiff));

	    snprintf (buf+k, BUF_SZ-k-1, "%s.width", ldiff);
	    if (config_get_int (buf) < 1) {
	      fatal_error ("`%s': minimum width has to be at least 1", buf);
	    }
	    mat->width = config_get_int (buf);
	  
	    snprintf (buf+k, BUF_SZ-k-1, "%s.overhang", ldiff);
	    if (config_get_int (buf) < 1) {
	      fatal_error ("`%s': minimum overhang has to be at least 1", buf);
	    }
	    mat->overhang = config_get_int (buf);
	  
	    snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", ldiff);
	    if (config_get_table_size (buf) != sz) {
	      fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	    }
	    mat->spacing = config_get_table_int (buf);
	  
	    snprintf (buf+k, BUF_SZ-k-1, "%s.oppspacing", ldiff);
	    if (config_get_table_size (buf) != sz) {
	      fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	    }
	    mat->oppspacing = config_get_table_int (buf);

	    snprintf (buf+k, BUF_SZ-k-1, "%s.minarea", ldiff);
	    if (config_exists (buf)) {
	      if (config_get_int (buf) < 0) {
		fatal_error ("%s: has to be non-negative", buf);
	      }
	      mat->minarea = config_get_int (buf);
	    }

	    A_NEW (contacts, char *);
	    A_NEXT (contacts) = Strdup (ldiff);
	    A_INC (contacts);
	  }
	  else {
	    T->well[i-4][j] = NULL;
	  }
	}
	else {
	  T->well[i-4][j] = NULL; /* no well */
	  T->welldiff[i-4][j] = NULL; /* no welldiff */
	}
      }
    }
  }

  /* polysilicon */
  tech_strname (prefix, "materials.polysilicon.");
  k = strlen (buf);
  buf[BUF_SZ-1] = '\0';

  PolyMat *pmat = new PolyMat (Strdup ("polysilicon"));
  T->poly = pmat;

  snprintf (buf+k, BUF_SZ-k-1, "width");
  if (config_get_int (buf) < 1) {
    fatal_error ("%s: minimum width has to be at least 1", buf);
  }
  pmat->width = config_get_int (buf);
  
  snprintf (buf+k, BUF_SZ-k-1, "spacing");
  verify_range_table (buf);
  pmat->spacing = new RangeTable (config_get_table_size (buf),
				  config_get_table_int (buf));

#if 0
  /* not sure this makes sense */
  snprintf (buf+k, BUF_SZ-k-1, "pitch");
  if (config_exists (buf)) {
    if (config_get_int (buf) < 1) {
      fatal_error ("%s: minimum pitch has to be at least 1", buf);
    }
  }
#endif
  
  snprintf (buf+k, BUF_SZ-k-1, "direction");
  pmat->r.routex = 1;
  pmat->r.routey = 1;
  if (config_exists (buf)) {
    i = config_get_int (buf);
    if (i == 0) {
      pmat->r.routex = 1;
      pmat->r.routey = 1;
    }
    else if (i == 1) {
      pmat->r.routex = 0;
      pmat->r.routey = 1;
    }
    else if (i == 2) {
      pmat->r.routex = 1;
      pmat->r.routey = 0;
    }
    else {
      fatal_error ("%s: direction is either 0, 1, or 2", buf);
    }
  }
  
  snprintf (buf+k, BUF_SZ-k-1, "minarea");
  if (config_exists (buf)) {
    if (config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }
    pmat->minarea = config_get_int (buf);
  }

  snprintf (buf+k, BUF_SZ-k-1, "minjog");
  if (config_exists (buf)) {
    if (config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }
    pmat->r.minjog = config_get_int (buf);
  }
  else {
    pmat->r.minjog = 0;
  }
  
  snprintf (buf+k, BUF_SZ-k-1, "endofline");
  if (config_exists (buf) && config_get_int (buf) != 0) {
    if (config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }
    pmat->r.endofline = config_get_int (buf);

    snprintf (buf+k, BUF_SZ-k-1, "endofline_width");
    if (config_exists (buf)) {
      pmat->r.endofline_width = config_get_int (buf);
      Assert (pmat->r.endofline_width >= pmat->width, "What?");
    }
    else {
      pmat->r.endofline_width = pmat->width;
    }
  }
  else {
    pmat->r.endofline = 0;
  }
  
  snprintf (buf+k, BUF_SZ-k-1, "overhang");
  verify_range_table (buf);
  pmat->overhang = new RangeTable (config_get_table_size (buf),
				   config_get_table_int (buf));
    
  snprintf (buf+k, BUF_SZ-k-1, "notch_overhang");
  verify_range_table (buf);
  pmat->notch_overhang = new RangeTable (config_get_table_size (buf),
					 config_get_table_int (buf));
    
  
  snprintf (buf+k, BUF_SZ-k-1, "via.nspacing");
  if (config_get_table_size (buf) != sz) {
    fatal_error ("%s: table size must match number of types", sz);
  }
  pmat->via_n = config_get_table_int (buf);
  
  snprintf (buf+k, BUF_SZ-k-1, "via.pspacing");
  if (config_get_table_size (buf) != sz) {
    fatal_error ("%s: table size must match number of types", sz);
  }
  pmat->via_p = config_get_table_int (buf);
  
  A_NEW (contacts, char *);
  A_NEXT (contacts) = Strdup ("polysilicon");
  A_INC (contacts);


  /* metals */
  tech_strname (prefix, "materials.metal.");
  k = strlen (buf);
  buf[BUF_SZ-1] = '\0';

  MALLOC (T->metal, RoutingMat *, T->nmetals);
    
  for (i=1; i <= T->nmetals; i++) {
    char *t;
    RoutingMat *mat;
    
    snprintf (buf+k, BUF_SZ-k-1, "m%d", i);
    t = config_get_string (buf);

    mat = new RoutingMat (Strdup (buf+k));
    T->metal[i-1] = mat;

    if (i != T->nmetals) {
      A_NEW (contacts, char *);
      A_NEXT (contacts) = Strdup (buf+k);
      A_INC (contacts);
    }

    /* now look for materials.metal.t */
    snprintf (buf+k, BUF_SZ-k-1, "%s.", t);
    j = strlen (buf);
    
    snprintf (buf+j, BUF_SZ-j-1, "width");
    verify_range_table (buf);
    mat->width = new RangeTable (config_get_table_size (buf),
				 config_get_table_int (buf));
    
    snprintf (buf+j, BUF_SZ-j-1, "spacing");
    verify_range_table (buf);
    mat->spacing = new RangeTable (config_get_table_size (buf),
				   config_get_table_int (buf));

    snprintf (buf+j, BUF_SZ-j-1, "pitch");
    if (config_exists (buf) && config_get_int (buf) < 1) {
      fatal_error ("%s: minimum pitch has to be at least 1", buf);
    }
    if (config_exists (buf)) {
      mat->pitch = config_get_int (buf);
    }
    else {
      mat->pitch = mat->width->min() + mat->spacing->min();
    }
    
    snprintf (buf+j, BUF_SZ-j-1, "direction");
    mat->r.routex = 1;
    mat->r.routey = 1;
    if (config_exists (buf)) {
      int tmp;
      tmp = config_get_int (buf);
      if (tmp == 0) {
	mat->r.routex = 1;
	mat->r.routey = 1;
      }
      else if (tmp == 1) {
	mat->r.routex = 0;
	mat->r.routey = 1;
      }
      else if (tmp == 2) {
	mat->r.routex = 1;
	mat->r.routey = 0;
      }
      else {
	fatal_error ("%s: direction is either 0, 1, or 2", buf);
      }
    }
    
    snprintf (buf+j, BUF_SZ-j-1, "minarea");
    if (config_exists (buf)) {
      if (config_get_int (buf) < 0) {
	fatal_error ("%s: has to be non-negative", buf);
      }
      mat->minarea = config_get_int (buf);
    }

    snprintf (buf+j, BUF_SZ-j-1, "minjog");
    if (config_exists (buf)) {
      if (config_get_int (buf) < 0) {
	fatal_error ("%s: has to be non-negative", buf);
      }
      mat->r.minjog = config_get_int (buf);
    }
    else {
      mat->r.minjog = 0;
    }
    
    snprintf (buf+j, BUF_SZ-j-1, "endofline");
    if (config_exists (buf) && config_get_int (buf) != 0) {
      if (config_get_int (buf) < 0) {
	fatal_error ("%s: has to be non-negative", buf);
      }
      mat->r.endofline = config_get_int (buf);
      snprintf (buf+j, BUF_SZ-j-1, "endofline_width");
      mat->r.endofline_width = config_get_int (buf);
      Assert (mat->r.endofline_width >= mat->width->min(), "What?");
    }
    else {
      mat->r.endofline = 0;
    }
  }

  /* contacts */
  tech_strname (prefix, "vias.");
  k = strlen (buf);
  buf[BUF_SZ-1] = '\0';
  for (i=0; i < A_LEN (contacts); i++) {
    char *t;

    snprintf (buf+k, BUF_SZ-k-1, "%s", contacts[i]);
    t = config_get_string (buf);

    snprintf (buf+k, BUF_SZ-k-1, "%s.name", t);
    if (!config_get_string (buf)) {
      fatal_error ("%s: needs a name field", buf);
    }

    char vname[1024];
    sprintf (vname, "%s_%s", config_get_string (buf), contacts[i]);
    Contact *cmat = new Contact (vname);

    cmat->lower = NULL;
    cmat->upper = NULL;

    for (int j=0; j < T->num_devs; j++) {
      for (int k=0; k < 2; k++) {
	if (strcmp (T->diff[k][j]->getName(), contacts[i]) == 0) {
	  cmat->lower = T->diff[k][j];
	  break;
	}
	if (T->well[k][j] &&
	    (strcmp (T->well[k][j]->getName(), contacts[i]) == 0)) {
	  cmat->lower = T->well[k][j];
	  break;
	}
	if (strcmp (T->fet[k][j]->getName(), contacts[i]) == 0) {
	  cmat->lower = T->fet[k][j];
	  break;
	}
	if (T->welldiff[k][j] &&
	    (strcmp (T->welldiff[k][j]->getName(), contacts[i]) == 0)) {
	  cmat->lower = T->welldiff[k][j];
	  break;
	}
      }
    }
    if (cmat->lower) {
      cmat->upper = T->metal[0];
    }
    else {
      if (strcmp (contacts[i], T->poly->getName()) == 0) {
	cmat->lower = T->poly;
	cmat->upper = T->metal[0];
      }
    }
    if (!cmat->lower) {
      for (int j=0; j < T->nmetals-1; j++) {
	if (strcmp (contacts[i], T->metal[j]->getName()) == 0) {
	  cmat->lower = T->metal[j];
	  cmat->upper = T->metal[j+1];
	  break;
	}
      }
    }
    if (!cmat->lower || !cmat->upper) {
      fatal_error ("Not sure what this contact `%s' is for!", contacts[i]);
    }

    
    if (cmat->upper != T->metal[0]) {
      Assert (cmat->upper->viadn == NULL, "Hmm");
      Assert (cmat->lower->viaup == NULL, "Hmm");
      cmat->lower->viaup = cmat;
      cmat->upper->viadn = cmat;
    }
    else {
      Assert (cmat->lower->viaup == NULL, "Hmm");
      cmat->lower->viaup = cmat;
      cmat->upper->viadn = cmat;
    }

    snprintf (buf+k, BUF_SZ-k-1, "%s.width", t);
    if (config_get_int (buf) < 1) {
      fatal_error ("%s: has to be at least 1", buf);
    }
    cmat->width_int = config_get_int (buf);
    
    snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", t);
    if (config_get_int (buf) < 1) {
      fatal_error ("%s: has to be at least 1", buf);
    }
    cmat->spacing = config_get_int (buf);
    
    snprintf (buf+k, BUF_SZ-k-1, "%s.surround.up", t);
    if (config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }
    cmat->sym_surround_up = config_get_int (buf);
    snprintf (buf+k, BUF_SZ-k-1, "%s.surround.dn", t);
    if (config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }
    cmat->sym_surround_dn = config_get_int (buf);
    
    snprintf (buf+k, BUF_SZ-k-1, "%s.surround.asym_up", t);
    if (config_exists (buf)) {
      if (config_get_int (buf) < 0) {
	fatal_error ("%s: has to be non-negative", buf);
      }
      cmat->asym_surround_up = config_get_int (buf);
    }
    
    snprintf (buf+k, BUF_SZ-k-1, "%s.surround.asym_dn", t);
    if (config_exists (buf)) {
      if (config_get_int (buf) < 0) {
	fatal_error ("%s: has to be non-negative", buf);
      }
      cmat->asym_surround_dn = config_get_int (buf);
    }
  }
  A_FREE (contacts);
}


int RangeTable::operator[](int idx)
{
  int i;

  if (sz == 1) {
    return table[0];
  }
  
  for (i=0; i < sz; i += 2) {
    Assert (i+1 < sz, "Hmm");
    if (idx <= table[i]) {
      return table[i+1];
    }
  }
  return table[sz-1];
}


/*--- derived rules ---*/
int DiffMat::viaSpaceEdge ()
{
  Assert (viaup, "Hmm");
  return via_edge + via_fet + viaup->getWidth();
}

int DiffMat::viaSpaceMid ()
{
  Assert (viaup, "Hmm");
  return 2*via_fet + viaup->getWidth();
}

int DiffMat::effOverhang (int w, int hasvia)
{
  int t;

  Assert (viaup, "Hmm");

  t = (*overhang)[w];
  if (!hasvia) {
    return t;
  }
  int s = viaSpaceEdge ();
  if (s > t) {
    return s;
  }
  else {
    return t;
  }
}
