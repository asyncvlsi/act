/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "misc.h"
#include "array.h"

#define BUF_SZ 10240
static char buf[BUF_SZ];

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

/*------------------------------------------------------------------------
 *
 * tech_init --
 *
 *   Read in the technology file and run some sanity checks
 *
 *------------------------------------------------------------------------
 */
void tech_init (const char *prefix, const char *s)
{
  int i, j, k, sz;
  int nmetals;
  const char *tables[] = { "diff.ptype", "diff.ntype", "diff.pfet", "diff.nfet",
			   "diff.pfet_well", "diff.nfet_well" };
  char **diff;

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
  tech_strname (prefix, "info.date");
  if (!config_exists (buf) || !config_get_string (buf)) {
    fatal_error ("Technology file: missing `%s'!", buf);
  }
  tech_strname (prefix, "general.scale");
  config_get_real (buf);
  tech_strname (prefix, "general.metals");
  nmetals = config_get_int (buf);
  if (nmetals < 1) {
    fatal_error ("Need at least one metal layer!");
  }

  tech_strname (prefix, "diff.types");
  sz = config_get_table_size (buf);
  if (sz < 1) {
    fatal_error ("No diffusion types?");
  }
  config_get_table_string (buf);
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
    for (j = 0; j < sz; j++) {
      if (i < 2) {
	/* ptype/ntype: diffusion */
	snprintf (buf+k, BUF_SZ-k-1, "%s.width", diff[j]);
	config_get_int (buf);
	snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]);
	if (config_get_table_size (buf) != sz) {
	  fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	}
	config_get_table_int (buf);
	snprintf (buf+k, BUF_SZ-k-1, "%s.oppspacing", diff[j]);
	if (config_get_table_size (buf) != sz) {
	  fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	}
	config_get_table_int (buf);
	snprintf (buf+k, BUF_SZ-k-1, "%s.polyspacing", diff[j]);
	config_get_int (buf);
	snprintf (buf+k, BUF_SZ-k-1, "%s.notchspacing", diff[j]);
	config_get_int (buf);
	snprintf (buf+k, BUF_SZ-k-1, "%s.overhang", diff[j]);
	verify_range_table (buf);
	snprintf (buf+k, BUF_SZ-k-1, "%s.via.edge", diff[j]);
	config_get_int (buf);
	snprintf (buf+k, BUF_SZ-k-1, "%s.via.fet", diff[j]);
	config_get_int (buf);

	/* contacts */
	A_NEW (contacts, char *);
	A_NEXT (contacts) = diff[j];
	A_INC (contacts);
      }
      else if (i < 4) {
	/* pfet/nfet: transistors */
	snprintf (buf+k, BUF_SZ-k-1, "%s.width", diff[j]);
	if (config_get_int (buf) < 1) {
	  fatal_error ("`%s': minimum width has to be at least 1", buf);
	}
	snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]);
	verify_range_table (buf);
      }
      else if (i < 6) {
	/* wells */
	if (strcmp (diff[j], "") != 0) {
	  /* there is a well */
	  snprintf (buf+k, BUF_SZ-k-1, "%s.width", diff[j]);
	  if (config_get_int (buf) < 1) {
	    fatal_error ("`%s': minimum width has to be at least 1", buf);
	  }
	  snprintf (buf+k, BUF_SZ-k-1, "%s.overhang", diff[j]);
	  if (config_get_int (buf) < 1) {
	    fatal_error ("`%s': minimum overhang has to be at least 1", buf);
	  }
	  snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", diff[j]);
	  if (config_get_table_size (buf) != sz) {
	    fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	  }
	  config_get_table_int (buf);
	  snprintf (buf+k, BUF_SZ-k-1, "%s.oppspacing", diff[j]);
	  if (config_get_table_size (buf) != sz) {
	    fatal_error ("Table `%s' has to be the same size as the # of types", buf);
	  }
	  config_get_table_int (buf);

	  A_NEW (contacts, char *);
	  A_NEXT (contacts) = diff[j];
	  A_INC (contacts);
	}
      }
    }
  }

  /* polysilicon */
  tech_strname (prefix, "materials.polysilicon.");
  k = strlen (buf);
  buf[BUF_SZ-1] = '\0';

  snprintf (buf+k, BUF_SZ-k-1, "width");
  if (config_get_int (buf) < 1) {
    fatal_error ("%s: minimum width has to be at least 1", buf);
  }
  snprintf (buf+k, BUF_SZ-k-1, "spacing");
  verify_range_table (buf);
  snprintf (buf+k, BUF_SZ-k-1, "pitch");
  if (config_exists (buf) && config_get_int (buf) < 1) {
    fatal_error ("%s: minimum pitch has to be at least 1", buf);
  }
  snprintf (buf+k, BUF_SZ-k-1, "direction");
  if (config_exists (buf)) {
    i = config_get_int (buf);
    if (i < 0 || i > 2) {
      fatal_error ("%s: direction is either 0, 1, or 2", buf);
    }
  }
  snprintf (buf+k, BUF_SZ-k-1, "minarea");
  if (config_exists (buf) && config_get_int (buf) < 0) {
    fatal_error ("%s: has to be non-negative", buf);
  }
  snprintf (buf+k, BUF_SZ-k-1, "minturn");
  if (config_exists (buf) && config_get_int (buf) < 0) {
    fatal_error ("%s: has to be non-negative", buf);
  }
  snprintf (buf+k, BUF_SZ-k-1, "endofline");
  if (config_exists (buf) && config_get_int (buf) < 0) {
    fatal_error ("%s: has to be non-negative", buf);
  }
  snprintf (buf+k, BUF_SZ-k-1, "overhang");
  verify_range_table (buf);
  snprintf (buf+k, BUF_SZ-k-1, "notch_overhang");
  verify_range_table (buf);
  snprintf (buf+k, BUF_SZ-k-1, "via.nspacing");
  if (config_get_table_size (buf) != sz) {
    fatal_error ("%s: table size must match number of types", sz);
  }
  snprintf (buf+k, BUF_SZ-k-1, "via.pspacing");
  if (config_get_table_size (buf) != sz) {
    fatal_error ("%s: table size must match number of types", sz);
  }
  A_NEW (contacts, char *);
  A_NEXT (contacts) = "polysilicon";
  A_INC (contacts);


  /* metals */

  tech_strname (prefix, "materials.metal.");
  k = strlen (buf);
  buf[BUF_SZ-1] = '\0';
  
  for (i=1; i <= nmetals; i++) {
    char *t;

    snprintf (buf+k, BUF_SZ-k-1, "m%d", i);
    t = config_get_string (buf);

    if (i != nmetals) {
      A_NEW (contacts, char *);
      A_NEXT (contacts) = Strdup (buf+k);
      A_INC (contacts);
    }

    /* now look for materials.metal.t */
    snprintf (buf+k, BUF_SZ-k-1, "%s.", t);
    j = strlen (buf);
    
    snprintf (buf+j, BUF_SZ-j-1, "width");
    verify_range_table (buf);
    snprintf (buf+j, BUF_SZ-j-1, "spacing");
    verify_range_table (buf);
    snprintf (buf+j, BUF_SZ-j-1, "pitch");
    if (config_exists (buf) && config_get_int (buf) < 1) {
      fatal_error ("%s: minimum pitch has to be at least 1", buf);
    }
    snprintf (buf+j, BUF_SZ-j-1, "direction");
    if (config_exists (buf)) {
      int tmp;
      tmp = config_get_int (buf);
      if (tmp < 0 || tmp > 2) {
	fatal_error ("%s: direction is either 0, 1, or 2", buf);
      }
    }
    snprintf (buf+j, BUF_SZ-j-1, "minarea");
    if (config_exists (buf) && config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }
    snprintf (buf+j, BUF_SZ-j-1, "minturn");
    if (config_exists (buf) && config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }
    snprintf (buf+j, BUF_SZ-j-1, "endofline");
    if (config_exists (buf) && config_get_int (buf) < 0) {
      fatal_error ("%s: has to be non-negative", buf);
    }

  }

  /* contacts */
  tech_strname (prefix, "contacts.");
  k = strlen (buf);
  buf[BUF_SZ-1] = '\0';
  for (i=0; i < A_LEN (contacts); i++) {
    int found_contact;
    snprintf (buf+k, BUF_SZ-k-1, "%s.width", contacts[i]);
    if (config_get_int (buf) < 1) {
      fatal_error ("%s: has to be at least 1", buf);
    }
    snprintf (buf+k, BUF_SZ-k-1, "%s.spacing", contacts[i]);
    if (config_get_int (buf) < 1) {
      fatal_error ("%s: has to be at least 1", buf);
    }
    found_contact = 0;
    snprintf (buf+k, BUF_SZ-k-1, "%s.sym.surround", contacts[i]);
    if (config_exists (buf)) {
      if (config_get_int (buf) < 0) {
	fatal_error ("%s: has to be non-negative", buf);
      }
      found_contact++;
    }
    snprintf (buf+k, BUF_SZ-k-1, "%s.asym.surround", contacts[i]);
    if (config_exists (buf)) {
      if (config_get_int (buf) < 0) {
	fatal_error ("%s: has to be non-negative", buf);
      }
      snprintf (buf+k, BUF_SZ-k-1, "%s.asym.opp", contacts[i]);
      if (config_exists (buf)) {
	if (config_get_int (buf) < 0) {
	  fatal_error ("%s: has to be non-negative", buf);
	}
	found_contact++;
      }
    }
    if (!found_contact) {
      fatal_error ("Missing contact for material `%s'", contacts[i]);
    }
  }
  A_FREE (contacts);
}
