/*************************************************************************
 *
 *  Copyright (c) 2019 Rajit Manohar
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
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <map>
#include <common/config.h>
#include <ctype.h>

#include <act/act.h>
#include <act/tech.h>
#include <common/pp.h>

#define pp_nl pp_forced (pp, 0)
#define pp_nltab pp_forced (pp, 3)
#define pp_TAB pp_nltab; pp_setb (pp)
#define pp_UNTAB pp_endb (pp); pp_nl
#define pp_SPACE pp_nl; pp_nl

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif
 
static void emit_header (pp_t *pp)
{
  pp_printf (pp, "tech"); pp_TAB;
  pp_printf (pp, "format 33"); pp_nl;
  for (int i=0; Technology::T->name[i]; i++) {
    if (Technology::T->name[i] == ' ' || Technology::T->name[i] == '\t') break;
    pp_printf (pp, "%c", Technology::T->name[i]);
  }
  pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;

  pp_printf (pp, "version"); pp_TAB;
  pp_printf (pp, "version 1"); pp_nl;
  pp_printf (pp, "description \"%s\"", Technology::T->date); pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;
}

static void emit_planes (pp_t *pp)
{
  pp_printf (pp, "#--- Planes ---"); pp_nl;
  pp_printf (pp, "planes"); pp_TAB;
  pp_printf (pp, "well,w"); pp_nl;
  pp_printf (pp, "select,s"); pp_nl;
  pp_printf (pp, "active,a"); pp_nl;
  for (int i=0; i < Technology::T->nmetals; i++) {
    pp_printf (pp, "metal%d,m%d", i+1, i+1); pp_nl;
    if (i != Technology::T->nmetals-1) {
      pp_printf (pp, "via%d,%s", i+1,
		 Technology::T->metal[i]->viaUpName()); pp_nl;
    }
  }
  pp_printf (pp, "comment");
  pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;
}

static void emit_tiletypes (pp_t *pp)
{
  int empty = 1;
  
  pp_printf (pp, "#--- Tile types ---"); pp_nl;
  pp_printf (pp, "types"); pp_TAB;
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      if (Technology::T->well[j][i]) {
	pp_printf (pp, "well   %s", Technology::T->well[j][i]->getName());
	pp_nl;
	empty = 0;
      }
    }
    for (int j=0; j < 2; j++) {
      if (Technology::T->welldiff[j][i]) {
	pp_printf (pp, "active   %s", Technology::T->welldiff[j][i]->getName());
	pp_nl;
	empty = 0;
      }
    }
    for (int j=0; j < 2; j++) {
      if (Technology::T->sel[j][i]) {
	pp_printf (pp, "select   %s", Technology::T->sel[j][i]->getName());
	pp_nl;
	empty = 0;
      }
    }
  }
  if (!empty) {
    pp_nl;
  }
  empty = 1;
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      if (Technology::T->diff[j][i]) {
	pp_printf (pp, "active   %s", Technology::T->diff[j][i]->getName());
	pp_nl;
	pp_printf (pp, "active   %s", Technology::T->diff[j][i]->viaUpName());
	pp_nl;
	empty = 0;
      }
      if (Technology::T->welldiff[j][i]) {
	pp_printf (pp, "active   %s", Technology::T->welldiff[j][i]->viaUpName());
	pp_nl;
	empty = 0;
      }
      if (Technology::T->fet[j][i]) {
	pp_printf (pp, "active   %s", Technology::T->fet[j][i]->getName());
	pp_nl;
	empty = 0;
      }
    }
  }
  pp_printf (pp, "active   %s", Technology::T->poly->getName()); pp_nl;
  pp_printf (pp, "active   %s", Technology::T->poly->viaUpName()); pp_nl;
  pp_nl;

  for (int i=0; i < Technology::T->nmetals; i++) {
    const char *tmp;
    pp_printf (pp, "metal%d m%d", i+1, i+1);
    tmp = Technology::T->metal[i]->getLEFName();
    if (tmp) {
      char tbuf[20];
      snprintf (tbuf, 20, "metal%d", i+1);
      if (strcmp (tmp, tbuf) != 0) {
	snprintf (tbuf, 20, "m%d", i+1);
	if (strcmp (tmp, tbuf) != 0) {
	  pp_printf (pp, ",%s", tmp);
	}
      }
    }
    pp_nl;
    pp_printf (pp, "metal%d m%dpin", i+1, i+1); pp_nl;
    if (i != Technology::T->nmetals-1) {
      pp_printf (pp, "metal%d m%dc,%s", i+1, i+2,
		 Technology::T->metal[i]->viaUpName()); pp_nl;
    }
  }
  pp_printf (pp, "comment comment"); 
  pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;
}


static void emit_contacts (pp_t *pp)
{
  pp_printf (pp, "#--- contacts ---"); pp_nl;
  pp_printf (pp, "contact"); pp_TAB;

  pp_printf (pp, "%s %s m1", Technology::T->poly->viaUpName(),
	     Technology::T->poly->getName());
  pp_nl;
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      if (Technology::T->diff[j][i]) {
	pp_printf (pp, "%s %s m1", Technology::T->diff[j][i]->viaUpName(),
		   Technology::T->diff[j][i]->getName());
	pp_nl;
      }
#if 0      
      if (Technology::T->well[j][i]) {
	pp_printf (pp, "%s %s m1", Technology::T->well[j][i]->viaUpName(),
		   Technology::T->well[j][i]->getName());
	pp_nl;
      }
#endif      
      if (Technology::T->welldiff[j][i]) {
	pp_printf (pp, "%s %s m1", Technology::T->welldiff[j][i]->viaUpName(),
		   Technology::T->welldiff[j][i]->getName());
	pp_nl;
      }
    }
  }

  for (int i=1; i < Technology::T->nmetals; i++) {
    pp_printf (pp, "m%dc m%d m%d", i+1, i, i+1);
    pp_nl;
  }
  pp_printf (pp, "stackable"); pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;
}

#define PTYPE 1
#define NTYPE 0

static void emit_styles (pp_t *pp)
{
  pp_printf (pp, "styles"); pp_TAB;
  pp_printf (pp, "styletype mos"); pp_TAB;

  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      char c = (j == PTYPE) ? 'p' : 'n';
      char d = (j == PTYPE) ? 'n' : 'p';
      if (Technology::T->well[j][i]) {
	pp_printf (pp, "%s %cwell", Technology::T->well[j][i]->getName(), d);
	pp_nl;
      }
      if (Technology::T->diff[j][i]) {
	pp_printf (pp, "%s %cdiffusion",
		   Technology::T->diff[j][i]->getName(), c); pp_nl;
	pp_printf (pp, "%s %cdiffusion metal1 contact_X'es",
		   Technology::T->diff[j][i]->viaUpName(), c); pp_nl;
      }
      if (Technology::T->fet[j][i]) {
	pp_printf (pp, "%s %ctransistor %ctransistor_stripes",
		   Technology::T->fet[j][i]->getName(), c, c); pp_nl;
      }
      if (Technology::T->welldiff[j][i]) {
	pp_printf (pp, "%s %cdiff_in_%cwell", Technology::T->welldiff[j][i]->getName(), d, d);
	pp_nl;
	pp_printf (pp, "%s %cdiff_in_%cwell metal1_alt contact_X'es",
		   Technology::T->welldiff[j][i]->viaUpName(), d, d);
	pp_nl;
      }
      if (Technology::T->sel[j][i]) {
	/* something here */
      }
    }
  }
  pp_printf (pp, "%s polysilicon", Technology::T->poly->getName()); pp_nl;
  pp_printf (pp, "%s poly_contact contact_X'es",
	     Technology::T->poly->viaUpName()); pp_nl;


  for (int i=0; i < Technology::T->nmetals; i++) {
    pp_printf (pp, "m%d metal%d", i+1, i+1); pp_nl;
    if (i > 0) {
      char tbuf[100];
      if (Technology::T->metal[i-1]->getUpC()->getDrawingStyle()) {
	snprintf (tbuf, 100, "%s", Technology::T->metal[i-1]->getUpC()->getDrawingStyle());
      }
      else {
	snprintf (tbuf, 100, "via%d", i);
      }
      pp_printf (pp, "m%dc metal%d metal%d %s", i+1, i, i+1, tbuf);
      pp_nl;
    }
  }

  //pp_printf (pp, "boundary subcircuit"); pp_nl;
  pp_printf (pp, "error_p error_waffle"); pp_nl;
  pp_printf (pp, "error_s error_waffle"); pp_nl;
  pp_printf (pp, "error_ps error_waffle"); pp_nl;
  //pp_printf (pp, "pad overglass metal%d", Technology::T->nmetals+1); pp_nl;

  pp_endb (pp); pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;
}


static void emit_compose (pp_t *pp)
{
  pp_printf (pp, "compose"); pp_TAB;

  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      pp_printf (pp, "compose %s %s %s",
		 Technology::T->fet[j][i]->getName(),
		 Technology::T->poly->getName(),
		 Technology::T->diff[j][i]->getName());
      pp_nl;

      if (Technology::T->well[j][i]) {
	pp_printf (pp, "paint %s %s %s",
		   Technology::T->diff[1-j][i]->getName(),
		   Technology::T->well[j][i]->getName(),
		   Technology::T->diff[j][i]->getName()); pp_nl;
	pp_printf (pp, "paint %s %s %s",
		   Technology::T->fet[1-j][i]->getName(),
		   Technology::T->well[j][i]->getName(),
		   Technology::T->fet[j][i]->getName()); pp_nl;

#if 0
	/* add this later */
	if (Technology::T->welldiff[1-j][i] &&
	    Technology::T->welldiff[j][i]) {
	  pp_printf (pp, "paint %sc %s %sc",
		     Technology::T->welldiff[1-j][i]->getName(),
		     Technology::T->well[j][i]->getName(),
		     Technology::T->welldiff[j][i]->getName()); pp_nl;
	
	  pp_printf (pp, "paint %sc %s %sc",
		     Technology::T->welldiff[1-j][i]->getName(),
		     Technology::T->well[j][i]->getName(),
		     Technology::T->well[j][i]->getName()); pp_nl;
	}
#endif
      }
    }
  }
  
  pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;
}


static void emit_connect (pp_t *pp)
{
  Material *m1, *m2;
  pp_printf (pp, "connect"); pp_TAB;
  pp_printf (pp, "%s", Technology::T->poly->getName());
  pp_printf (pp, ",%s/a", Technology::T->poly->viaUpName());
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      pp_printf (pp, ",%s", Technology::T->fet[j][i]->getName());
    }
  }
  pp_printf (pp, " ");
  pp_printf (pp, "%s", Technology::T->poly->getName());
  pp_printf (pp, ",%s/a", Technology::T->poly->viaUpName());
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      pp_printf (pp, ",%s", Technology::T->fet[j][i]->getName());
    }
  }
  pp_nl;
  
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      m1 = Technology::T->welldiff[j][i];
      m2 = Technology::T->well[j][i];
      if (m1 && m2) {
	pp_printf (pp, "%s,%s,%s/a ", m1->getName(), m2->getName(),
		   m1->viaUpName());
	pp_printf (pp, "%s,%s,%s/a", m1->getName(), m2->getName(),
		   m1->viaUpName());
	pp_nl;
      }

      m1 = Technology::T->diff[j][i];
      if (m1) {
	pp_printf (pp, "%s,%s/a %s,%s/a", m1->getName(),
		   m1->viaUpName(), m1->getName(), m1->viaUpName());
	pp_nl;
      }
    }
  }

  pp_printf (pp, "m1,m2c/m1,%s/m1", Technology::T->poly->viaUpName());
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      m1 = Technology::T->welldiff[j][i];
      if (m1) {
	pp_printf (pp, ",%s/a", m1->viaUpName());
      }
      m1 = Technology::T->diff[j][i];
      if (m1) {
	pp_printf (pp, ",%s/a", m1->viaUpName());
      }
    }
  }
  pp_printf (pp, " ");
  pp_printf (pp, "m1,m2c/m1,%s/m1", Technology::T->poly->viaUpName());
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      m1 = Technology::T->welldiff[j][i];
      if (m1) {
	pp_printf (pp, ",%s/a", m1->viaUpName());
      }
      m1 = Technology::T->diff[j][i];
      if (m1) {
	pp_printf (pp, ",%s/a", m1->viaUpName());
      }
    }
  }
  pp_nl;
  
  for (int i=2; i < Technology::T->nmetals+1; i++) {
    if (i != Technology::T->nmetals) {
      pp_printf (pp, "m%d,m%dc/m%d,m%dc/m%d m%d,m%dc/m%d,m%dc/m%d",
		 i, i, i, (i+1), i, 
		 i, i, i, (i+1), i);
      pp_nl;
    }
    else {
      pp_printf (pp, "m%d,m%dc/m%d m%d,m%dc/m%d",
		 i, i, i, i, i, i);
    }
  }
  

  pp_UNTAB;
  pp_printf (pp, "end"); pp_SPACE;
}


void emit_mzrouter (pp_t *pp)
{
  pp_printf (pp, "mzrouter"); pp_TAB;
  pp_printf (pp, "# this is a dummy section"); pp_nl;
  pp_printf (pp, "style irouter"); pp_nl;
  pp_printf (pp, "layer m1  2 1 1 1"); pp_nl;
  pp_printf (pp, "layer %s 2 2 1 1", Technology::T->poly->getName()); pp_nl;
  pp_printf (pp, "contact %s m1 %s 10", Technology::T->poly->viaUpName(),
	     Technology::T->poly->getName());
  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}

void emit_scalefactor (pp_t *pp)
{
  if (Technology::T->scale != (int)Technology::T->scale) {
    double x = Technology::T->scale*10.0;
    if (x != (int)x) {
      warning ("Technology scale factor is not an integer number of angstroms");
    }
    pp_printf (pp, "scalefactor %d angstroms", (int)x);
  }
  else {
    pp_printf (pp, "scalefactor %d nanometers", (int)Technology::T->scale);
  }
  pp_nl;
}

int scalefactor (void)
{
  if (Technology::T->scale != (int)Technology::T->scale) {
    double x = Technology::T->scale*10.0;
    return (int)x;
  }
  else {
    return (int)Technology::T->scale;
  }
}


void emit_cifinput (pp_t *pp, Material *m)
{
  int i;
  if (!m) return;

  if (!m->getGDSlist()) return;
  
  pp_printf (pp, "layer %s", m->getName());
  pp_TAB;
  list_t *l = m->getGDSlist ();
  int *bloat = m->getGDSBloat ();
  int min_bloat;
  if (bloat) {
    min_bloat = bloat[0];
  }
  else {
    min_bloat = 0;
  }
  i = 0;
  for (listitem_t *li = list_first (l); li; li = list_next (li)) {
    GDSLayer *g = (GDSLayer *) list_value (li);
    if (i == 0) {
      pp_printf (pp, "or %s", g->getName());
    }
    else {
      pp_printf (pp, "and %s", g->getName());
    }
    pp_nl;
    if (bloat) {
      if (min_bloat > bloat[i]) {
	min_bloat = bloat[i];
      }
    }
    i++;
  }
  if (min_bloat != 0) {
    pp_printf (pp, "shrink %d", min_bloat);
    pp_nl;
  }
  char buf[1024];
  snprintf (buf, 1024, "layout.materials.%s.gds_mask", m->getName());
  if (config_exists (buf)) {
    char **table = config_get_table_string (buf);
    for (int i=0; i < config_get_table_size (buf); i++) {
      pp_printf (pp, "and-not %s", table[i]);
      pp_nl;
    }
  }
  pp_printf (pp, "labels %s",
	     ((GDSLayer *) list_value (list_first (l)))->getName());
  pp_nl;
  
  pp_UNTAB;
}

void emit_cifinputc (pp_t *pp, Contact *c)
{
  int i;
  if (!c) return;
  if (!c->getGDSlist()) return;
  pp_printf (pp, "layer %s", c->getName());
  pp_TAB;
  list_t *l = c->getGDSlist ();
  int *bloat = c->getGDSBloat ();
  int min_bloat;
  if (bloat) {
    min_bloat = bloat[0];
  }
  else {
    min_bloat = 0;
  }
  i = 0;
  for (listitem_t *li = list_first (l); li; li = list_next (li)) {
    GDSLayer *g = (GDSLayer *) list_value (li);
    if (i == 0) {
      pp_printf (pp, "or %s", g->getName());
    }
    else {
      pp_printf (pp, "and %s", g->getName());
    }
    pp_nl;
    if (bloat) {
      if (min_bloat > bloat[i]) {
	min_bloat = bloat[i];
      }
    }
    i++;
  }
  if (min_bloat != 0) {
    pp_printf (pp, "shrink %d", min_bloat);
    pp_nl;
  }
  char buf[1024];
  snprintf (buf, 1024, "layout.vias.%s_gds_mask", c->getName());
  if (config_exists (buf)) {
    char **table = config_get_table_string (buf);
    for (int i=0; i < config_get_table_size (buf); i++) {
      pp_printf (pp, "and-not %s", table[i]);
      pp_nl;
    }
  }
  pp_printf (pp, "labels %s",
	     ((GDSLayer *) list_value (list_first (l)))->getName());
  pp_nl;

  pp_UNTAB;
}

void emit_cif (pp_t *pp)
{
  const char *gdsl = "layout.gds.layers";
  int s = scalefactor();
  pp_printf (pp, "cifoutput"); pp_TAB;
  pp_printf (pp, "style generic"); pp_nl;
  emit_scalefactor (pp);
  pp_printf (pp, "options calma-permissive-labels"); pp_nl;
  pp_nl;

  if (!config_exists (gdsl)) {
    warning ("Empty cifinput/cifoutput section; missing GDS layers");
  }
  else {
    char **gds_all = config_get_table_string (gdsl);
    for (int i=0; i < config_get_table_size (gdsl); i++) {
      GDSLayer *g = Technology::T->GDSlookup (gds_all[i]);
      if (!g || !g->matList()) {
	warning ("GDS layer `%s' specified but unused?", gds_all[i]);
	continue;
      }
      pp_printf (pp, "layer %s", gds_all[i]);
      pp_nl;
      pp_puts (pp, "   ");
      pp_setb (pp);
      listitem_t *li;
      bool found_first = false;
      for (li = g->matList(); li; li = list_next (li)) {
	struct GDSLayer::mat_info *mx = (GDSLayer::mat_info *) list_value (li);
	pp_printf (pp, "bloat-or %s * %d", mx->m->getName(),
		   mx->bloat*s);
	pp_nl;
	if (mx->is_first) {
	  found_first = true;
	}
      }
      if (found_first) {
	for (li = g->matList(); li; li = list_next (li)) {
	  struct GDSLayer::mat_info *mx = (GDSLayer::mat_info*) list_value (li);
	  if (mx->is_first) {
	    if (found_first) {
	      pp_printf (pp, "labels %s", mx->m->getName());
	      found_first = false;
	    }
	    else {
	      pp_printf (pp, ",%s", mx->m->getName());
	    }
	  }
	}
	pp_nl;
      }
      pp_printf (pp, "calma %d %d", g->getMajor(), g->getMinor());
      pp_endb (pp);
      pp_nl;

      pp_nl;
    }
  }
  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;

  /*
   * For cifinput, we need to do a bit of pre-processing.
   *
   * A abstract geometry layer gets turned into a number of GDS
   * layers, each optionally bloated. Let m = the min bloat
   * (right now assumed to be non-negative). 
   *
   * The AND of all the GDS layers, shrunk by the min bloat should
   * roughly correspond to the geometry.
   *
   * However, this is not always accurate because the set of GDS
   * layers for one abstract layer may be a superset of those of
   * another type. In this case, we need to mask the layers with the
   * difference in layers (just need one).
   *
   */

  pp_printf (pp, "cifinput"); pp_TAB;
  pp_printf (pp, "style generic"); pp_nl;
  emit_scalefactor (pp);
  pp_nl;

  /*
   * Base layers
   *  diff, diffc, fet
   *
   *  selects, wells
   *  poly
   */
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      Material *diff = Technology::T->diff[j][i];
      Contact *diffc = Technology::T->diff[j][i]->getUpC();
      Material *fet = Technology::T->fet[j][i];

      emit_cifinput (pp, diff);
      emit_cifinputc (pp, diffc);

      /* well diff: nn/pp diff */
      diff = Technology::T->welldiff[j][i];
      if (diff) {
	diffc = Technology::T->welldiff[j][i]->getUpC();
      }
      else {
	diffc = NULL;
      }
      if (diff) {
	emit_cifinput (pp, diff);
      }
      if (diffc) {
	emit_cifinputc (pp, diffc);
      }

      WellMat *well = Technology::T->well[j][i];
      if (well) {
	// stuff here
	emit_cifinput (pp, well);
      }

      // n/p select
      Material *sel = Technology::T->sel[j][i];
      if (sel) {
	emit_cifinput (pp, sel);
      }
    }
  }

  // poly
  PolyMat *poly = Technology::T->poly;
  emit_cifinput (pp, poly);
  emit_cifinputc (pp, poly->getUpC());

  // metal
  for (int i=0; i < Technology::T->nmetals-1; i++) {
    RoutingMat *metal = Technology::T->metal[i];
    Contact *metalc = metal->getUpC ();
    emit_cifinput (pp, metal);
    emit_cifinputc (pp, metalc);
  }  

  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}

void emit_spacing (pp_t *pp, const char *nm1, const char *nm2, int amt, int touching_ok = 0)
{
  pp_printf (pp, "spacing %s %s %d %s\\", nm1, nm2, amt,
	     touching_ok == 1 ? "touching_ok" : "touching_illegal");
  pp_nl;
  pp_printf (pp, "   \"%s to %s spacing < %d \"", nm1, nm2, amt);
  pp_nl;
}

void emit_spacing_corner (pp_t *pp, const char *nm1, const char *nm2, int amt,
			  const char *corner)
{
  pp_printf (pp, "spacing %s %s %d corner_ok %s\\", nm1, nm2, amt, corner);
  pp_nl;
  pp_printf (pp, "   \"%s to %s spacing < %d \"", nm1, nm2, amt);
  pp_nl;
}

void emit_width_spacing (pp_t *pp, Material *mat, char *nm = NULL)
{
  const char *name;
  if (!mat) return;
  if (!nm) {
    name = mat->getName();
  }
  else {
    name = nm;
  }

  /* a material can be also its upcontact or downcontact */
  

  pp_printf (pp, "# rules for %s", mat->getName());
  pp_nl;
  
  pp_printf (pp, "width %s %d \\", name, mat->minWidth());
  pp_nl;
  pp_printf (pp, "   \"%s width < %d\"", mat->getName(), mat->minWidth());
  pp_nl;

  emit_spacing (pp, name, name, mat->minSpacing (), 1);

  if (mat->minArea() != 0) {
    pp_printf (pp, "area %s %d %d \\", name, mat->minArea(),
	       mat->minArea()/mat->minWidth());
    pp_nl;
    pp_printf (pp, "  \"%s minimum area < %d\"", mat->getName(),
	       mat->minArea());
    pp_nl;
  }
}

void emit_surround (pp_t *pp, const char *mat, const char *surround, int amt, int absence_ok = 0)
{
  char buf[100];

  if (surround[0] == 'm' && isdigit (surround[1]) &&
      (!surround[2] ||isdigit (surround[2]) &&
       (!surround[3] || isdigit(surround[3])))) {
    /* metal surround */
    snprintf (buf, 100, "(all%s)/%s", surround, surround);
  }
  else {
    snprintf (buf, 100, "%s", surround);
  }

  pp_printf (pp, "surround %s %s %d absence_%s \\",
	     mat, buf, amt, absence_ok ? "ok" : "illegal");
  pp_nl;
  pp_printf (pp, "   \"%s surround of %s < %d\"",
	     surround, mat, amt);
  pp_nl;
}

void emit_surround (pp_t *pp, Material *mat, const char *surround, int amt)
{
  emit_surround (pp, mat->getName(), surround, amt);
}

void emit_surround_metal (pp_t *pp, Material *mat, const char *surround, int amt)
{
  pp_printf (pp, "surround %s *%s %d absence_illegal \\",
	     mat->getName(), surround, amt);
  pp_nl;
  pp_printf (pp, "   \"%s surround of %s < %d\"",
	     surround, mat->getName(), amt);
  pp_nl;
}


void emit_width_spacing_c (pp_t *pp, Contact *mat, char *nm = NULL)
{
  const char *name;
  if (!mat) return;
  if (!nm) {
    name = mat->getName();
  }
  else {
    name = nm;
  }

  /* a material can be also its upcontact or downcontact */
  

  pp_printf (pp, "# rules for %s", mat->getName());
  pp_nl;
  
  pp_printf (pp, "width %s %d \\", name, mat->minWidth());
  pp_nl;
  pp_printf (pp, "   \"%s width < %d\"", mat->getName(), mat->minWidth());
  pp_nl;

  emit_spacing (pp, name, name, mat->minSpacing (), 1);

  if (mat->minArea() != 0) {
    pp_printf (pp, "area %s %d %d \\", name, mat->minArea(),
	       mat->minArea()/mat->minWidth());
    pp_nl;
    pp_printf (pp, "  \"%s minimum area < %d\"", mat->getName(),
	       mat->minArea());
    pp_nl;
  }

  if (mat->getSym() > 0) {
    emit_surround (pp, mat, mat->getLowerName(), mat->getSym());
  }
  if (mat->getSymUp() > 0) {
    emit_surround (pp, mat, mat->getUpperName(), mat->getSymUp());
  }
  if (mat->isAsym()) {
    if (mat->getAsym() > 0 && (mat->getAsym() != mat->getSym())) {
      pp_printf (pp, "surround %s %s %d directional \\",
		 mat->getName(), mat->getLowerName(), mat->getAsym());
      pp_nl;
      pp_printf (pp, "   \"%s surround of via %s < %d in one direction\"",
		 mat->getLowerName(), mat->getName(), mat->getAsym());
      pp_nl;
    }
    if (mat->getAsymUp() > 0 && (mat->getAsymUp() != mat->getSymUp())) {
      pp_printf (pp, "surround %s %s %d directional \\",
		 mat->getName(), mat->getUpperName(), mat->getAsymUp());
      pp_nl;
      pp_printf (pp, "   \"%s surround of via %s < %d in one direction\"",
		 mat->getUpperName(), mat->getName(), mat->getAsymUp());
      pp_nl;
    }
  }
}

void emit_width_spacing_metalc (pp_t *pp, Contact *mat, char *nm = NULL)
{
  const char *name;
  if (!mat) return;
  if (!nm) {
    name = mat->getName();
  }
  else {
    name = nm;
  }

  /* a material can be also its upcontact or downcontact */
  

  pp_printf (pp, "# rules for %s", mat->getName());
  pp_nl;
  
  pp_printf (pp, "width %s %d \\", name, mat->minWidth());
  pp_nl;
  pp_printf (pp, "   \"%s width < %d\"", mat->getName(), mat->minWidth());
  pp_nl;

  emit_spacing (pp, name, name, mat->minSpacing (), 1);

  if (mat->minArea() != 0) {
    pp_printf (pp, "area %s %d %d \\", name, mat->minArea(),
	       mat->minArea()/mat->minWidth());
    pp_nl;
    pp_printf (pp, "  \"%s minimum area < %d\"", mat->getName(),
	       mat->minArea());
    pp_nl;
  }

  if (mat->getSym() > 0) {
    emit_surround_metal (pp, mat, mat->getLowerName(), mat->getSym());
  }
  if (mat->getSymUp() > 0) {
    emit_surround_metal (pp, mat, mat->getUpperName(), mat->getSymUp());
  }
  if (mat->isAsym()) {
    if (mat->getAsym() > 0 && (mat->getAsym() != mat->getSym())) {
      pp_printf (pp, "surround %s *%s %d directional \\",
		 mat->getName(), mat->getLowerName(), mat->getAsym());
      pp_nl;
      pp_printf (pp, "   \"%s surround of via %s < %d in one direction\"",
		 mat->getLowerName(), mat->getName(), mat->getAsym());
      pp_nl;
    }
    if (mat->getAsymUp() > 0 && (mat->getAsymUp() != mat->getSymUp())) {
      pp_printf (pp, "surround %s *%s %d directional \\",
		 mat->getName(), mat->getUpperName(), mat->getAsymUp());
      pp_nl;
      pp_printf (pp, "   \"%s surround of via %s < %d in one direction\"",
		 mat->getUpperName(), mat->getName(), mat->getAsymUp());
      pp_nl;
    }
  }
}



/*
  Emit overhang of mat1 over nm by amt 
*/
void emit_overhang (pp_t *pp, Material *mat1,
		    const char *mat1s, const char *nm, int amt)
{
  if (!mat1) return;
  
  pp_printf (pp, "# more rules for %s", mat1->getName());
  pp_nl;
  
  pp_printf (pp, "overhang %s %s %d \\", mat1s, nm, amt);
  pp_nl;
  pp_printf (pp, "   \"%s overhang of %s < %d\"", mat1->getName(),  nm, amt);
  pp_nl;

}

void emit_overhang (pp_t *pp, Material *mat1, const char *nm, int amt)
{
  if (!mat1) return;

  emit_overhang (pp, mat1, mat1->getName(), nm, amt);
}


void emit_drc (pp_t *pp)
{
  char buf[1024];
  char buf2[1024];
  
  pp_printf (pp, "drc"); pp_TAB;

  /* base layers */
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      Material *diff = Technology::T->diff[j][i];
      Contact *diffc = Technology::T->diff[j][i]->getUpC();
      Material *fet = Technology::T->fet[j][i];

      if (diff && diffc && fet) {
	snprintf (buf, 1024, "%s,%s,%s", diff->getName(),
		  diffc->getName(), fet->getName());
      }
      else if (diff) {
	snprintf (buf, 1024, "%s", diff->getName());
      }
      emit_width_spacing (pp, Technology::T->diff[j][i], buf);

      /* emit poly contact to diff spacing */
      emit_spacing (pp, Technology::T->poly->getUpC()->getName(),
		    buf,
		    (j == 0 ? 
		     Technology::T->poly->getViaNSpacing (i) :
		     Technology::T->poly->getViaPSpacing (i)));
      
      

      if (diff && diffc) {
	snprintf (buf2, 1024, "%s,%s", diff->getName(),
		  diffc->getName());
      }
      else if (diff) {
	snprintf (buf2, 1024, "%s", diff->getName());
      }

      
      emit_overhang (pp, Technology::T->diff[j][i], buf2,
		     Technology::T->fet[j][i]->getName(),
		     Technology::T->diff[j][i]->effOverhang (0));
      
      if (diff) {
	emit_width_spacing_c (pp, Technology::T->diff[j][i]->getUpC());
      }

      if (Technology::T->well[j][i]) {
	emit_surround (pp, buf, Technology::T->well[j][i]->getName(),
		       Technology::T->well[j][i]->getOverhang(), 1);
      }

      emit_width_spacing (pp, Technology::T->fet[j][i]);

      diff = Technology::T->welldiff[j][i];
      if (Technology::T->welldiff[j][i]) {
	diffc = Technology::T->welldiff[j][i]->getUpC();
      }
      else {
	diffc = NULL;
      }
      if (diff && diffc) {
	snprintf (buf, 1024, "%s,%s", diff->getName(), diffc->getName());
      }
      else if (diff) {
	snprintf (buf, 1024, "%s", diff->getName());
      }
      emit_width_spacing (pp, Technology::T->welldiff[j][i], buf);
      emit_width_spacing (pp, Technology::T->well[j][i]);
      if (Technology::T->welldiff[j][i]) {
	emit_width_spacing_c (pp, Technology::T->welldiff[j][i]->getUpC());
      }

      if (Technology::T->fet[j][i] && Technology::T->diff[j][i]) {
	int spc;
	pp_printf (pp, "# diff to contact spacing"); pp_nl;
	spc = Technology::T->diff[j][i]->getViaFet() -
	  (Technology::T->diff[j][i]->getUpC()->minWidth()-
	   Technology::T->diff[j][i]->getUpC()->getSym()+1)/2;
	if (spc <= 0) {
	  spc = 1;
	}
	
	emit_spacing (pp, Technology::T->fet[j][i]->getName(),
		      Technology::T->diff[j][i]->getUpC()->getName(), spc);
      }

      if (Technology::T->well[j][i]) {
	WellMat *well = Technology::T->well[j][i];
#if 0	
	if (well->getUpC()) {
	  snprintf (buf2, 1024, "%s,%s", well->getName(),
		    well->getUpC()->getName());
	}
	else {
	  snprintf (buf2, 1024, "%s", well->getName());
	}
	/* XX FIXME */
#endif

	emit_spacing (pp, well->getName(),
		      Technology::T->diff[1-j][i]->getName(),
		      well->getOverhang() + well->oppSpacing (i));
      }
    }
  }

  emit_spacing (pp, "allndiff", "allpdiff", 
		Technology::T->getMaxDiffSpacing ());
  emit_spacing (pp, "allnndiff", "allppdiff", 
		Technology::T->getMaxWellDiffSpacing ());

  emit_spacing (pp, "allndiff", "allppdiff", 
		Technology::T->getMaxDiffSpacing (),
		1);
  emit_spacing (pp, "allpdiff", "allnndiff", 
		Technology::T->getMaxDiffSpacing (),
		1);
  
  /*-- poly rules --*/
  
  snprintf (buf, 1024, "allpolynonfet,allfet");
  emit_width_spacing (pp, Technology::T->poly, buf);
  emit_width_spacing_c (pp, Technology::T->poly->getUpC());

  emit_overhang (pp, Technology::T->poly, "allpolynonfet", "allfet",
		 Technology::T->poly->getOverhang (0));


  

  int pspacing = 0;
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      if (Technology::T->diff[j][i]) {
	pspacing = MAX (pspacing, Technology::T->diff[j][i]->getPolySpacing());
      }
    }
  }

  /* poly spacing to active */
  emit_spacing (pp, "allpolynonfet", "allfet", pspacing, 1);
  emit_spacing_corner (pp, "allpolynonfet", "allactivenonfet", pspacing,
		       "allfet");

  /*-- other poly rules --*/


  /* metal */
  for (int i=0; i < Technology::T->nmetals; i++) {
    snprintf (buf, 1024, "(allm%d)/m%d", i+1, i+1);
    emit_width_spacing (pp, Technology::T->metal[i], buf);
    emit_width_spacing_metalc (pp, Technology::T->metal[i]->getUpC());
  }
  
  
  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}

void emit_lef (pp_t *pp)
{
  int i, j;
  pp_printf (pp, "lef"); pp_TAB;

  for (i=0; i < Technology::T->num_devs; i++) {
    for (j=0; j < 2; j++) {
      if (Technology::T->well[j][i]) {
	pp_printf (pp, "masterslice %s %s", Technology::T->well[j][i]->getName(),
		   Technology::T->well[j][i]->getName());
	pp_nl;
      }
    }
  }

  for (int i=0; i < Technology::T->nmetals; i++) {
    pp_printf (pp, "routing m%d m%d metal%d", i+1, i+1, i+1);
    pp_nl;
  }

  for (int i=0; i < Technology::T->nmetals-1; i++) {
    pp_printf (pp, "cut m%dc via%d %s",
	       i+2, i+1, Technology::T->metal[i]->viaUpName());
    pp_nl;
  }

  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}


void emit_extract (pp_t *pp)
{
  pp_printf (pp, "extract"); pp_TAB;

  pp_printf (pp, "style generic"); pp_nl;
  pp_printf (pp, "cscale 1"); pp_nl;
  pp_printf (pp, "lambda %f", (float)Technology::T->scale/10.0); pp_nl;
  pp_printf (pp, "step 100"); pp_nl;
  pp_printf (pp, "sidehalo 8"); pp_nl;
  pp_printf (pp, "rscale 1"); pp_nl;
  int order = 0;
  pp_printf (pp, "planeorder well %d", order++); pp_nl;
  pp_printf (pp, "planeorder select %d", order++); pp_nl;
  pp_printf (pp, "planeorder active %d", order++); pp_nl;
  for (int i=0; i < Technology::T->nmetals; i++) {
    pp_printf (pp, "planeorder metal%d %d", i+1, order++); pp_nl;
  }

  for (int i=0; i < Technology::T->nmetals-1; i++) {
    pp_printf (pp, "planeorder via%d %d", i+1, order++); pp_nl;
  }
  
  pp_printf (pp, "planeorder comment %d", order++);
  pp_nl;

  char **act_flav = config_get_table_string ("act.dev_flavors");
  char **dev_names = config_get_table_string ("net.ext_devs");
  char **map_name = config_get_table_string ("net.ext_map");

  /* emit resistance classes */
  for (int i=0; i < Technology::T->num_devs; i++) {
    pp_printf (pp, "resist (");
    pp_printf (pp, "%s,%s/a",
	       Technology::T->diff[0][i]->getName(),
	       Technology::T->diff[0][i]->getUpC()->getName());
    if (Technology::T->welldiff[0][i]) {
      pp_printf (pp, ",%s,%s/a",
		 Technology::T->welldiff[0][i]->getName(),
		 Technology::T->welldiff[0][i]->getUpC()->getName());
    }
    pp_printf (pp, ") 1000"); /* XXX need number here! */
    pp_nl;
    pp_printf (pp, "resist (");
    pp_printf (pp, "%s,%s/a",
	       Technology::T->diff[1][i]->getName(),
	       Technology::T->diff[1][i]->getUpC()->getName());
    if (Technology::T->welldiff[1][i]) {
      pp_printf (pp, ",%s,%s/a",
		 Technology::T->welldiff[1][i]->getName(),
		 Technology::T->welldiff[1][i]->getUpC()->getName());
    }
    pp_printf (pp, ") 1000"); /* XXX need number here! */
    pp_nl;
  }
  /* poly, m1 m2, etc. */
  
  for (int i=0; i < Technology::T->num_devs; i++) {
    char tmp[128];
    char *devnm;
    snprintf (tmp, 128, "nfet_%s", act_flav[i]);
    devnm = NULL;
    for (int k=0; k < config_get_table_size ("net.ext_map"); k++) {
       if (strcmp (map_name[k], tmp) == 0) {
          devnm = dev_names[k];
          break;
       }
    }
    if (!devnm) {
	fatal_error ("Device name map inconsistency!"); 
    }
    /* j = 0 : nfet */
    pp_printf (pp, "device mosfet %s %s allndiff %s,space Gnd!",
	       devnm,
	       Technology::T->fet[0][i]->getName(),
	       Technology::T->well[0][i] ?
	       Technology::T->well[0][i]->getName() : "space/w");
    pp_nl;
	       
    snprintf (tmp, 128, "pfet_%s", act_flav[i]);
    devnm = NULL;
    for (int k=0; k < config_get_table_size ("net.ext_map"); k++) {
       if (strcmp (map_name[k], tmp) == 0) {
          devnm = dev_names[k];
          break;
       }
    }
    if (!devnm) {
	fatal_error ("Device name map inconsistency!"); 
    }
    pp_printf (pp, "device mosfet %s %s allpdiff %s,space Vdd!",
	       devnm,
	       Technology::T->fet[1][i]->getName(),
	       Technology::T->well[1][i] ?
	       Technology::T->well[1][i]->getName() : "space/w");
    pp_nl;
  }
  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}

void emit_wiring (pp_t *pp)
{
  pp_printf (pp, "wiring"); pp_TAB;

  pp_printf (pp, "# dummy"); pp_nl;
  for (int i=0; i < Technology::T->nmetals-1; i++) {
    pp_printf (pp, "contact m%dc 28 m%d 0 m%d 0", i+2,
	       i+1, i+2); pp_nl;
  }
  pp_printf (pp, "contact %s 28 m1 0 %s 0",
	     Technology::T->poly->viaUpName(),
	     Technology::T->poly->getName());
  
  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}

void emit_router (pp_t *pp)
{
  pp_printf (pp, "router"); pp_TAB;

  pp_printf (pp, "# dummy"); pp_nl;
  pp_printf (pp, "layer1  m1 18 m1,m2c/m1 18"); pp_nl;
  pp_printf (pp, "layer2  m2 20 m2c/m2,m3c,m2 20"); pp_nl;
  pp_printf (pp, "contacts m2c 28"); pp_nl;
  pp_printf (pp, "gridspacing 8");

  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}

void emit_plowing (pp_t *pp)
{
  pp_printf (pp, "plowing"); pp_TAB;

  pp_printf (pp, "# dummy"); pp_nl;
  pp_printf (pp, "fixed %s,%s",
	     Technology::T->fet[0][0]->getName(),
	     Technology::T->fet[1][0]->getName());
  pp_nl;
  pp_printf (pp, "covered %s,%s",
	     Technology::T->fet[0][0]->getName(),
	     Technology::T->fet[1][0]->getName());
  pp_nl;
  pp_printf (pp, "drag %s,%s",
	     Technology::T->fet[0][0]->getName(),
	     Technology::T->fet[1][0]->getName());

  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}

void emit_plot_map (pp_t *pp, Contact *c)
{
  if (!c) return;

  pp_printf (pp, "map %s %s %s", c->getName(),
	     c->getLowerName(), c->getUpperName());
  pp_nl;
}

void emit_plot (pp_t *pp)
{
  pp_printf (pp, "plot"); pp_TAB;

  pp_printf (pp, "style pnm"); pp_TAB;

  for (int i=0; i < Technology::T->nmetals; i++) {
    pp_printf (pp, "draw m%d", i+1); pp_nl;
  }
  pp_printf (pp, "draw polysilicon"); pp_nl;

  for (int i=0; i < Technology::T->num_devs; i++)
    for (int j=0; j < 2; j++) {
      pp_printf (pp, "draw %s", Technology::T->fet[j][i]->getName());
      pp_nl;

      pp_printf (pp, "draw %s", Technology::T->diff[j][i]->getName());
      pp_nl;
    }
  pp_nl;

  emit_plot_map (pp, Technology::T->poly->getUpC());

  for (int i=0; i < Technology::T->nmetals-1; i++) {
    emit_plot_map (pp, Technology::T->metal[i]->getUpC());
  }

  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      emit_plot_map (pp, Technology::T->diff[j][i]->getUpC());
    }
  }

  pp_UNTAB;
  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}


void emit_aliases (pp_t *pp)
{
  int first;
  pp_printf (pp, "aliases"); pp_TAB;

  for (int i=0; i < Technology::T->nmetals; i++) {
    pp_printf (pp, "allm%d *m%d", i+1, i+1); pp_nl;
  }

  pp_printf (pp, "allpolynonfet %s,%s", Technology::T->poly->getName(),
	     Technology::T->poly->getUpC()->getName());
  pp_nl;

  pp_printf (pp, "allfet ");
  first = 1;
  for (int i=0; i < Technology::T->num_devs; i++) {
    for (int j=0; j < 2; j++) {
      if (Technology::T->fet[j][i]) {
	if (!first) {
	  pp_printf (pp, ",");
	}
	first = 0;
	pp_printf (pp, "%s", Technology::T->fet[j][i]->getName());
      }
    }
  }
  pp_nl;

  pp_printf (pp, "allndiff ");
  first = 1;
  for (int i=0; i < Technology::T->num_devs; i++) {
    int j = 0; /* n */
    if (Technology::T->diff[j][i]) {
      if (!first) { pp_printf (pp, ","); } first = 0;
      pp_printf (pp, "%s", Technology::T->diff[j][i]->getName());
      if (Technology::T->diff[j][i]->getUpC()) {
	pp_printf (pp, ",%s", Technology::T->diff[j][i]->getUpC()->getName());
      }
    }
  }
  pp_nl;

  pp_printf (pp, "allppdiff ");
  first = 1;
  for (int i=0; i < Technology::T->num_devs; i++) {
    int j = 0; /* p */
    if (Technology::T->welldiff[j][i]) {
      if (!first) { pp_printf (pp, ","); } first = 0;
      pp_printf (pp, "%s", Technology::T->welldiff[j][i]->getName());
      if (Technology::T->welldiff[j][i]->getUpC()) {
	pp_printf (pp, ",%s", Technology::T->welldiff[j][i]->getUpC()->getName());
      }
    }
  }
  pp_nl;

  pp_printf (pp, "allpdiff ");
  first = 1;
  for (int i=0; i < Technology::T->num_devs; i++) {
    int j = 1; /* p */
    if (Technology::T->diff[j][i]) {
      if (!first) { pp_printf (pp, ","); } first = 0;
      pp_printf (pp, "%s", Technology::T->diff[j][i]->getName());
      if (Technology::T->diff[j][i]->getUpC()) {
	pp_printf (pp, ",%s", Technology::T->diff[j][i]->getUpC()->getName());
      }
    }
  }
  pp_nl;

  pp_printf (pp, "allnndiff ");
  first = 1;
  for (int i=0; i < Technology::T->num_devs; i++) {
    int j = 1; /* n */
    if (Technology::T->welldiff[j][i]) {
      if (!first) { pp_printf (pp, ","); } first = 0;
      pp_printf (pp, "%s", Technology::T->welldiff[j][i]->getName());
      if (Technology::T->welldiff[j][i]->getUpC()) {
	pp_printf (pp, ",%s", Technology::T->welldiff[j][i]->getUpC()->getName());
      }
    }
  }
  pp_nl;


  pp_printf (pp, "allactivenonfet allndiff,allpdiff,allnndiff,allppdiff");
  
  /*
    allnactivenonfet *ndiff,*nsd

    allpactivenonfet *pdiff,*psd
    allpactive	   allpactivenonfet,allpfets

    allactivenonfet  allnactivenonfet,allpactivenonfet
    allactive	   allactivenonfet,allfets

    allpolynonfet    *poly
    allpoly	   allpolynonfet,allfets
  */
  
  pp_UNTAB;
  pp_printf (pp, "end");
  pp_SPACE;
}


int main (int argc, char **argv)
{
  pp_t *pp;
  
  Act::Init (&argc, &argv, "layout:layout.conf");
  Technology::Init();
  
  if (Technology::T->nmetals < 2) {
    fatal_error ("Can't handle a process with fewer than two metal layers!");
  }

  pp = pp_init (stdout, 1000);

  emit_header (pp);
  emit_planes (pp);
  emit_tiletypes (pp);
  emit_contacts (pp);
  emit_aliases (pp);
  emit_styles (pp);
  emit_compose (pp);
  emit_connect (pp);
  emit_cif (pp);
  emit_mzrouter (pp);
  emit_drc (pp);
  emit_lef (pp);
  emit_extract (pp);
  emit_wiring (pp);
  emit_router (pp);
  emit_plowing (pp);
  emit_plot (pp);
  
  
  pp_close (pp);
  
  return 0;
}
