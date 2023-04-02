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
#include <common/config.h>
#include <common/misc.h>
#include <common/array.h>
#include <act/act.h>
#include <act/lang.h>
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
  for (i=2; i < sz-1; i += 2) {
    if (tab[i] <= tab[i-2]) {
      fatal_error ("Table `%s': ranges must be in increasing order!", nm);
    }
  }
}

#define EDGE_PFET 1
#define EDGE_NFET 0

#define PTYPE EDGE_PFET
#define NTYPE EDGE_NFET

/*------------------------------------------------------------------------
 *
 *   Read in the technology file and run some sanity checks, and
 *   create all the materials needed.
 *
 *------------------------------------------------------------------------
 */
void Technology::Init ()
{
  int i, j, k, sz;
  const char *tables[] = { "diff.ntype", "diff.ptype", "diff.nfet", "diff.pfet",
			   "diff.nfet_well", "diff.pfet_well",
			   "diff.nselect", "diff.pselect" };
  
  char **diff;
  const char *prefix = "layout";

  Assert (NTYPE == 0 && PTYPE == 1, "Hmm");

  if (Technology::T) {
    /* -- already initialized -- */
    return;
  }

  Technology::T = new Technology();

  A_DECL (char *, contacts);
  A_INIT (contacts);

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

  tech_strname (prefix, "general.dummy_poly");
  if (config_exists (buf)) {
    T->dummy_poly = config_get_int (buf);
    if (T->dummy_poly < 0) {
      fatal_error ("Can't have negative dummy poly");
    }
  }
  else {
    T->dummy_poly = 0;
  }

  tech_strname (prefix, "general.welltap_adjust");
  if (config_exists (buf)) {
    T->welltap_adjust = config_get_int (buf);
  }
  else {
    T->welltap_adjust = 0;
  }

  T->gdsH = NULL;
  /* -- read GDS layers -- */
  tech_strname (prefix, "gds.layers");
  if (config_exists (buf)) {
    T->gdsH = hash_new (16);
    char **layers = config_get_table_string (buf);
    int sz = config_get_table_size (buf);
    
    tech_strname (prefix, "gds.major");
    int *maj = config_get_table_int (buf);

    if (config_get_table_size (buf) != sz) {
      fatal_error ("gds.major table size should match gds.layers");
    }
    
    tech_strname (prefix, "gds.minor");
    int *min = config_get_table_int (buf);

    if (config_get_table_size (buf) != sz) {
      fatal_error ("gds.minor table size should match gds.layers");
    }

    
    for (int i=0; i < sz; i++) {
      hash_bucket_t *b;
      b = hash_lookup (T->gdsH, layers[i]);
      if (b) {
	fatal_error ("Duplicate GDS layer name `%s'", layers[i]);
      }
      b = hash_add (T->gdsH, layers[i]);
      b->v = new GDSLayer (layers[i], maj[i], min[i]);
    }
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
    MALLOC (T->sel[i], Material *, sz);
  }
  
  for (i=0; i < sizeof(tables)/sizeof (tables[0]); i++) {
    tech_strname (prefix, tables[i]);
    if (i == 6 || i == 7)  { /* selects are optional */
      if (!config_exists (buf)) {
	continue;
      }
    }
    if (sz != config_get_table_size (buf)) {
      fatal_error ("Table `%s' is not the same size is the type table", buf);
    }
    config_get_table_string (buf);
  }

#define ADDGDS_TEMPL(mat)					\
  do {								\
      if (config_exists (buf)) {				\
	mat->addGDS (config_get_table_string (buf),		\
		     config_get_table_size (buf));		\
      }								\
      else {							\
	warning ("No GDS information: `%s'", buf);	        \
      }								\
  } while (0)

#define ADDGDSBL_TEMPL(mat)					\
  do {								\
      if (config_exists (buf)) {				\
	mat->addGDSBloat (config_get_table_int (buf),		\
			  config_get_table_size (buf));		\
      }								\
      else {							\
	warning ("No GDS bloat information: `%s'", buf);	\
      }								\
  } while (0)
  
#define ADDGDS(mat,name)				\
  do {							\
    if (T->gdsH) {					\
      if (name) {					\
	snprintf (buf+k, BUF_SZ-k-1, "%s.gds", name);	\
      }							\
      else {						\
	snprintf (buf+k, BUF_SZ-k-1, "gds");		\
      }							\
      ADDGDS_TEMPL(mat);				\
    }							\
  } while (0)

#define ADDGDSBL(mat,name)					\
  do {								\
    if (T->gdsH) {						\
      if (name) {						\
	snprintf (buf+k, BUF_SZ-k-1, "%s.gds_bloat", name);	\
      }								\
      else {							\
	snprintf (buf+k, BUF_SZ-k-1, "gds_bloat");		\
      }								\
      ADDGDSBL_TEMPL(mat);					\
    }								\
  } while (0)
  
  /* now: check materials! */
  for (i=0; i < 8; i++) {
    tech_strname (prefix, tables[i]);
    if (i == 6 || i == 7) {
      if (!config_exists (buf)) {
	for (j=0; j < T->num_devs; j++) {
	  T->sel[i-6][j] = NULL;
	}
	continue;
      }
    }

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
	ADDGDS(mat, diff[j]);
	ADDGDSBL(mat, diff[j]);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.width", diff[j]);
	int *wt;
	wt = new int[1];
	wt[0] = config_get_int (buf);
	mat->width = new RangeTable (1, wt);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]);
	if (config_get_table_size (buf) != sz) {
	  fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	}
	mat->spacing = config_get_table_int (buf);

	wt = new int[1];
	wt[0] = mat->spacing[j];
	mat->spacing_w = new RangeTable (1, wt);
	
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.oppspacing", diff[j]);
	if (config_get_table_size (buf) != sz) {
	  fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	}
	mat->oppspacing = config_get_table_int (buf);
	
	snprintf (buf+k, BUF_SZ-k-1, "%s.polyspacing", diff[j]);
	mat->polyspacing = config_get_int (buf);

	snprintf (buf+k, BUF_SZ-k-1, "%s.notchspacing", diff[j]);
	mat->notchspacing = config_get_int (buf);

	if (mat->notchspacing < mat->polyspacing) {
	  warning ("`%s': notch spacing (%d) is less than the poly spacing (%d).  Hope that is okay.",
		   diff[j], mat->notchspacing, mat->polyspacing);
	}
	
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
	ADDGDS(mat, diff[j]);
	ADDGDSBL(mat, diff[j]);
	
	/* pfet/nfet: transistors */
	snprintf (buf+k, BUF_SZ-k-1, "%s.width", diff[j]);
	if (config_get_int (buf) < 1) {
	  fatal_error ("`%s': minimum width has to be at least 1", buf);
	}
	int *wt = new int[1];
	wt[0] = config_get_int (buf);
	mat->width = new  RangeTable (1, wt);

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
	mat->spacing_w = new RangeTable (config_get_table_size (buf),
				       config_get_table_int (buf));
      }
      else if (i < 6) {
	/* wells */

	T->welldiff[i-4][j] = NULL;
	
	if (strcmp (diff[j], "") != 0) {
	  /* there is a well/welldiff */
	  char ldiff[1024];
	  int ik;
	  int has_welldiff;

	  has_welldiff = 0;
	  
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

	      has_welldiff = 1;
	      
	      mat = T->welldiff[i-4][j] = new DiffMat (diff[j]+ik+1);
	      ADDGDS(mat, diff[j]+ik+1);
	      ADDGDSBL(mat, diff[j]+ik+1);
	      
	      snprintf (buf+k, BUF_SZ-k+1, "%s.width", diff[j]+ik+1);
	      int *wt = new int[1];
	      wt[0] = config_get_int (buf);
	      mat->width = new RangeTable (1, wt);
	      
	      snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]+ik+1);
	      if (config_get_table_size (buf) != sz) {
		fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	      }
	      mat->spacing = config_get_table_int (buf);
	      wt = new int[1];
	      wt[0] = mat->spacing[j];
	      mat->spacing_w = new RangeTable (1, wt);

	      snprintf (buf+k, BUF_SZ-k-1, "%s.oppspacing", diff[j]+ik+1);
	      if (config_get_table_size (buf) != sz) {
		fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	      }
	      mat->oppspacing = config_get_table_int (buf);
	
	      snprintf (buf+k, BUF_SZ-k-1, "%s.polyspacing", diff[j]+ik+1);
	      mat->polyspacing = config_get_int (buf);

	      snprintf (buf+k, BUF_SZ-k-1, "%s.diffspacing", diff[j]+ik+1);
	      mat->diffspacing = config_get_int (buf);

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
	    ADDGDS (mat, ldiff);
	    ADDGDSBL (mat, ldiff);

	    snprintf (buf+k, BUF_SZ-k-1, "%s.width", ldiff);
	    if (config_get_int (buf) < 1) {
	      fatal_error ("`%s': minimum width has to be at least 1", buf);
	    }
	    int *wt = new int[1];
	    wt[0] = config_get_int (buf);
	    mat->width = new RangeTable (1, wt);
	  
	    snprintf (buf+k, BUF_SZ-k-1, "%s.overhang", ldiff);
	    if (config_get_int (buf) < 1) {
	      fatal_error ("`%s': minimum overhang has to be at least 1", buf);
	    }
	    mat->overhang = config_get_int (buf);

	    if (has_welldiff) {
	      snprintf (buf+k, BUF_SZ-k-1, "%s.overhang_welldiff", ldiff);
	      if (config_get_int (buf) < 1) {
		fatal_error ("`%s': minimum overhang has to be at least 1", buf);
	      }
	      mat->overhang_welldiff = config_get_int (buf);
	    }

	  
	    snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", ldiff);
	    if (config_get_table_size (buf) != sz) {
	      fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	    }
	    mat->spacing = config_get_table_int (buf);
	    wt = new int[1];
	    wt[0] = mat->spacing[j];
	    mat->spacing_w = new RangeTable (1, wt);
	  
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

	    snprintf (buf+k, BUF_SZ-k-1, "%s.plug_dist", ldiff);
	    if (config_exists (buf)) {
	      if (config_get_int (buf) <= 0) {
		fatal_error ("%s: has to be positive", buf);
	      }
	      mat->plug_dist = config_get_int (buf);
	    }
	    else {
	      mat->plug_dist = -1;
	    }

	    //XXX: no well contact; contact is through welldiff
	    //
	    //A_NEW (contacts, char *);
	    //A_NEXT (contacts) = Strdup (ldiff);
	    //A_INC (contacts);
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
      else {
	if (diff) {
	  /* selects */
	  T->sel[i-6][j] = new Material (diff[j]);
	  ADDGDS (T->sel[i-6][j], diff[j]);
	  ADDGDSBL (T->sel[i-6][j], diff[j]);
	}
	else {
	  T->sel[i-6][j] = NULL;
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
  ADDGDS (T->poly, (char *)NULL);
  ADDGDSBL (T->poly, (char *)NULL);
  
  snprintf (buf+k, BUF_SZ-k-1, "width");
  if (config_get_int (buf) < 1) {
    fatal_error ("%s: minimum width has to be at least 1", buf);
  }
  int *wt = new int[1];
  wt[0] = config_get_int (buf);
  pmat->width = new RangeTable (1, wt);
  
  snprintf (buf+k, BUF_SZ-k-1, "spacing");
  verify_range_table (buf);
  pmat->spacing_w = new RangeTable (config_get_table_size (buf),
				  config_get_table_int (buf));

  /* might need this for gridded technologies */
  snprintf (buf+k, BUF_SZ-k-1, "lef_width");
  if (config_exists (buf) && config_get_int (buf) < pmat->width->min()) {
    fatal_error ("%s: minimum lef_width has to be at least", buf);
  }
  if (config_exists (buf)) {
    pmat->r.lef_width = config_get_int (buf);
  }
  else {
    pmat->r.lef_width = pmat->width->min();
  }

  snprintf (buf+k, BUF_SZ-k-1, "pitch");
  if (config_exists (buf)) {
    if (config_get_int (buf) < 1) {
      fatal_error ("%s: minimum pitch has to be at least 1", buf);
    }
    pmat->r.pitch = config_get_int (buf);
  }
  else {
    pmat->r.pitch = pmat->minSpacing() + pmat->minWidth();
  }
  
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
      Assert (pmat->r.endofline_width >= pmat->minWidth(), "What?");
    }
    else {
      pmat->r.endofline_width = pmat->minWidth();
    }
  }
  else {
    pmat->r.endofline = 0;
  }

  snprintf (buf+k, BUF_SZ-k-1, "antenna.ratio");
  if (config_exists (buf)) {
    pmat->r.antenna_ratio = config_get_real (buf);
  }
  else {
    pmat->r.antenna_ratio = 0;
  }
  snprintf (buf+k, BUF_SZ-k-1, "antenna.diffratio");
  if (config_exists (buf)) {
    pmat->r.antenna_diff_ratio = config_get_real (buf);
  }
  else {
    pmat->r.antenna_diff_ratio = 0;
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
    fatal_error ("%s: table size must match number of types (%d)", buf, sz);
  }
  pmat->via_n = config_get_table_int (buf);
  
  snprintf (buf+k, BUF_SZ-k-1, "via.pspacing");
  if (config_get_table_size (buf) != sz) {
    fatal_error ("%s: table size must match number of types (%d)", buf, sz);
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

    snprintf (buf+k, BUF_SZ-k-1, "m%d_lefname", i);
    if (config_exists (buf)) {
      mat->setLEFName (config_get_string (buf));
    }
    snprintf (buf+k, BUF_SZ-k-1, "m%d", i);

    if (i != T->nmetals) {
      A_NEW (contacts, char *);
      A_NEXT (contacts) = Strdup (buf+k);
      A_INC (contacts);
    }

    if (T->gdsH) {
      snprintf (buf+k, BUF_SZ-k-1, "m%d_gds", i);
      ADDGDS_TEMPL(mat);
      snprintf (buf+k, BUF_SZ-k-1, "m%d_gds_bloat", i);
      ADDGDSBL_TEMPL(mat);
    }
    
    /* now look for materials.metal.t */
    snprintf (buf+k, BUF_SZ-k-1, "%s.", t);
    j = strlen (buf);
    
    snprintf (buf+j, BUF_SZ-j-1, "width");
    verify_range_table (buf);
    mat->width = new RangeTable (config_get_table_size (buf),
				 config_get_table_int (buf));

    /* spacing */
    snprintf (buf+j, BUF_SZ-j-1, "spacing");
    verify_range_table (buf);
    mat->spacing_w = new RangeTable (config_get_table_size (buf),
				   config_get_table_int (buf));

    /* extra spacing rules */
    snprintf (buf+j, BUF_SZ-j-1, "runlength");
    if (config_exists (buf)) {
      mat->r.runlength_mode = 0;
      mat->r.runlength = config_get_table_size (buf);
      mat->r.parallelrunlength = config_get_table_int (buf);

      snprintf (buf+j, BUF_SZ-j-1, "runlength_mode");
      if (config_exists (buf)) {
	mat->r.runlength_mode = config_get_int (buf);
      }
      int num_extra = mat->r.runlength;
      if (mat->r.runlength_mode == 1) {
	num_extra--;
      }
      MALLOC (mat->r.spacing_aux, RangeTable *, num_extra);
      for (int k=0; k < num_extra; k++) {
	snprintf (buf+j, BUF_SZ-j-1, "spacing%d", k+1);
	verify_range_table (buf);
	mat->r.spacing_aux[k] = new RangeTable (config_get_table_size (buf),
						config_get_table_int (buf));
      }
      if (mat->r.runlength_mode == 1) {
	/* XXX: check that all spacing tables are the same! */
	
      }

      snprintf (buf+j, BUF_SZ-j-1, "influence");
      if (config_exists (buf)) {
	mat->r.influence = config_get_table_int (buf);
	mat->r.inf_sz = config_get_table_size (buf);
	if ((mat->r.inf_sz % 3) != 0) {
	  fatal_error ("Influence table `%s' has to have 3n entries", buf);
	}
	mat->r.inf_sz /= 3;
      }
    }

    snprintf (buf+j, BUF_SZ-j-1, "lef_width");
    if (config_exists (buf) && config_get_int (buf) < mat->width->min()) {
      fatal_error ("%s: minimum lef_width has to be at least", buf);
    }
    if (config_exists (buf)) {
      mat->r.lef_width = config_get_int (buf);
    }
    else {
      mat->r.lef_width = mat->width->min();
    }

    snprintf (buf+j, BUF_SZ-j-1, "pitch");
    if (config_exists (buf) && config_get_int (buf) < 1) {
      fatal_error ("%s: minimum pitch has to be at least 1", buf);
    }
    if (config_exists (buf)) {
      mat->r.pitch = config_get_int (buf);
    }
    else {
      mat->r.pitch = mat->width->min() + mat->minSpacing();
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

    snprintf (buf+k, BUF_SZ-k-1, "antenna.ratio");
    if (config_exists (buf)) {
      mat->r.antenna_ratio = config_get_real (buf);
    }
    else {
      mat->r.antenna_ratio = 0;
    }
    snprintf (buf+k, BUF_SZ-k-1, "antenna.diffratio");
    if (config_exists (buf)) {
      mat->r.antenna_diff_ratio = config_get_real (buf);
    }
    else {
      mat->r.antenna_diff_ratio = 0;
    }
    
  }


  /* contacts */
  tech_strname (prefix, "vias.");
  k = strlen (buf);
  buf[BUF_SZ-1] = '\0';
  for (i=0; i < A_LEN (contacts); i++) {
    char *t;

    snprintf (buf+k, BUF_SZ-k-1, "%s_name", contacts[i]);
    t = config_get_string (buf);
    Contact *cmat = new Contact (t);
    cmat->lower = NULL;
    cmat->upper = NULL;

    snprintf (buf+k, BUF_SZ-k-1, "%s_dstyle", contacts[i]);
    if (config_exists (buf)) {
      t = config_get_string (buf);
      cmat->setDrawingStyle (t);
    }

    if (T->gdsH) {
      snprintf (buf+k, BUF_SZ-k-1, "%s_gds", contacts[i]);
      ADDGDS_TEMPL(cmat);
      snprintf (buf+k, BUF_SZ-k-1, "%s_gds_bloat", contacts[i]);
      ADDGDSBL_TEMPL(cmat);
    }

    snprintf (buf+k, BUF_SZ-k-1, "%s", contacts[i]);
    t = config_get_string (buf);

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
    int *wt = new int[1];
    wt[0] = config_get_int (buf);
    cmat->width = new RangeTable (1, wt);

    snprintf (buf+k, BUF_SZ-k-1, "%s.lef_width", t);
    if (config_exists (buf) && config_get_int (buf) < cmat->width->min()) {
      fatal_error ("%s: has to be at least the minimum width", buf);
    }
    if (config_exists (buf)) {
      cmat->lef_width = config_get_int (buf);
    }
    else {
      cmat->lef_width = cmat->width->min();
    }

    snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", t);
    if (config_get_int (buf) < 1) {
      fatal_error ("%s: has to be at least 1", buf);
    }
    wt = new int[1];
    wt[0] = config_get_int (buf);
    cmat->spacing_w = new RangeTable (1, wt);
    
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

    snprintf (buf+k, BUF_SZ-k-1, "%s.antenna.ratio", t);
    if (config_exists (buf)) {
      cmat->antenna_ratio = config_get_real (buf);
    }
    else {
      cmat->antenna_ratio = 0;
    }
    snprintf (buf+k, BUF_SZ-k-1, "%s.antenna.diffratio", t);
    if (config_exists (buf)) {
      cmat->antenna_diff_ratio = config_get_real (buf);
    }
    else {
      cmat->antenna_diff_ratio = 0;
    }

    snprintf (buf+k, BUF_SZ-k-1, "%s.generate.dx", t);
    if (config_exists (buf)) {
      cmat->spc_x = config_get_int (buf);
    }
    else {
      cmat->spc_x = -1;
    }
    snprintf (buf+k, BUF_SZ-k-1, "%s.generate.dy", t);
    if (config_exists (buf)) {
      cmat->spc_y = config_get_int (buf);
    }
    else {
      cmat->spc_y = -1;
    }
    if ((cmat->spc_x < 0 || cmat->spc_y < 0) &&
	(cmat->spc_x >= 0 || cmat->spc_y >= 0)) {
      fatal_error ("via.generate: must specify both dx and dy or neither");
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
  
  for (i=0; i < sz-1; i += 2) {
    Assert (i+1 < sz, "Hmm");
    if (idx <= table[i]) {
      return table[i+1];
    }
  }
  return table[sz-1];
}

int RangeTable::size()
{
  return (sz + 1)/2;
}

int RangeTable::range_threshold (int x)
{
  Assert (0 <= x && x < (sz+1)/2, "What?");
  return table[x*2];
}

/*--- derived rules ---*/
int DiffMat::viaSpaceEdge ()
{
  Assert (viaup, "Hmm");
  return via_edge + via_fet + viaup->minWidth();
}

int DiffMat::viaSpaceMid ()
{
  Assert (viaup, "Hmm");
  return 2*via_fet + viaup->minWidth();
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

int Technology::getMaxDiffSpacing ()
{
  int i, j;
  int spc = -1;
  for (i=0; i < num_devs; i++) {
    for (j=0; j < 2; j++) {
      if (spc < diff[j][0]->getOppDiffSpacing (i)) {
	spc = diff[j][0]->getOppDiffSpacing (i);
      }
      if (welldiff[j][i] && spc < welldiff[j][i]->getWdiffToDiffSpacing()) {
	spc = welldiff[j][i]->getWdiffToDiffSpacing();
      }
    }
  }
  return spc;
}

int Technology::getMaxWellDiffSpacing ()
{
  int i, j;
  int spc = -1;
  for (i=0; i < num_devs; i++) {
    for (j=0; j < 2; j++) {
      if (welldiff[j][0] && spc < welldiff[j][0]->getOppDiffSpacing (i)) {
	spc = welldiff[j][0]->getOppDiffSpacing (i);
      }
    }
  }
  return spc;
}

int Technology::getMaxSameDiffSpacing ()
{
  int i, j;
  int spc = -1;
  for (i=0; i < num_devs; i++) {
    for (j=0; j < 2; j++) {
      if (spc < diff[j][i]->getSpacing (i)) {
	spc = diff[j][i]->getSpacing (i);
      }
    }
  }
  return spc;
}

void Material::addGDS (char **layers, int sz)
{
  hash_bucket_t *b;
  Assert (Technology::T->gdsH, "What?");
  Assert (!gds, "What?");
  
  gds = list_new ();
  for (int i=0; i < sz; i++) {
    b = hash_lookup (Technology::T->gdsH, layers[i]);
    if (!b) {
      fatal_error ("Could not find GDS layer `%s'", layers[i]);
    }
    GDSLayer *gl = (GDSLayer *)b->v;
    list_append (gds, gl);
    gl->addMat (this);
  }
}

void Material::addGDSBloat (int *table, int sz)
{
  gds_bloat = table;
  if (!gds) {
    fatal_error ("Bloat should be added only after the layers are specified");
  }
  if (sz != list_length (gds)) {
    fatal_error ("Bloat table doesn't match gds table");
  }
}


void GDSLayer::addMat (Material *m)
{
  if (!mats) {
    mats = list_new ();
  }
  list_append (mats, m);
}

GDSLayer *Technology::GDSlookup (const char *s)
{
  hash_bucket_t *b;
  if (!gdsH) return NULL;
  b = hash_lookup (gdsH, s);
  if (!b) {
    return NULL;
  }
  return (GDSLayer *) b->v;
}

const  char *Material::viaUpName()
{
  Assert (viaup, "What?");
  return viaup->getName();
}
