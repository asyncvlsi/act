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
#ifndef __TECHFILE_H__
#define __TECHFILE_H__

#include <stdio.h>

class RangeTable {
 public:
  RangeTable (int _s, int *_tab) {
    sz = _s;
    table = _tab;
    minval = -1;
  }
  int min() {

    if (minval >= 0) return minval;
    if (sz == 1) {
      minval = table[0];
    }
    for (int i=1; i < sz; i += 2) {
      if (minval == -1) {
	minval = table[i];
      }
      else if (table[i] < minval) {
	minval = table[i];
      }
    }
    return minval;
  }
  
  int operator[](int idx);
  int size();
  int range_threshold (int idx);
 protected:
  int sz;
  int *table;
  int minval;
};

class Contact;

class Material {
 public:
  Material() {
    name = NULL;
    gds[0] = -1;
    gds[1] = -1;
    width = NULL;
    spacing = NULL;
    minarea = 0;
    maxarea = 0;
    xgrid = 0;
    ygrid = 0;
    viaup = NULL;
    viadn = NULL;
    pitch = 0;
    runlength = -1;
    runlength_mode = -1;
    parallelrunlength = NULL;
    spacing_aux = NULL;
  }

  const char *getName() { return name; }
  
protected:
  const char *name;		/* drawing name in magic */
  int gds[2];			/* gds ids */

  RangeTable *width;		/* min width (indexed by length) */
  RangeTable *spacing;		/* min spacing (indexed by width) */

  int runlength_mode;		// 0 = parallelrunlength, 1 = twowidths
  int runlength;		// negative if it doesn't exist;
				// otherwise # of parallel run lengths
  int *parallelrunlength;	// parallel run length options
  RangeTable **spacing_aux;	// extra range tables for parallel run
				// lengths

  int minarea;			/* 0 means no constraint */
  int maxarea;			/* 0 means no constraint */
  int xgrid, ygrid;		/* 0,0 if not on a grid */

  int pitch;

  Contact *viaup, *viadn;

  friend class Technology;
};

struct RoutingRules {
  int endofline;		/* end of line extension */
  int endofline_width;		/* width for extension */
  int minjog;

  double antenna_ratio;		// antenna ratios for this layer
  double antenna_diff_ratio;
  
  unsigned int routex:1;	/* can be used for x routing */
  unsigned int routey:1;	/* can be used for y routing */

  int *influence;		// spacing influence table
  int inf_sz;			// # of lines in the table
};

class RoutingMat : public Material {
public:
  RoutingMat (char *s) { name = s; r.influence = NULL; r.inf_sz = 0; }
  int minWidth () { return width->min(); }
  int minArea() { return minarea; }
  int minSpacing() { return spacing->min(); }
  int getPitch() { return pitch; }
  Contact *getUpC() { return viaup; }
  int getSpacing(int w) { return (*spacing)[w]; }
  int isComplexSpacing() {
    if (runlength != -1) return 1;
    if (spacing->size() > 1) return 1;
    return 0;
  }
  int complexSpacingMode() { return runlength_mode; }
  int numRunLength()  { return runlength; }
  int getRunLength(int w) { return parallelrunlength[w]; }
  RangeTable *getRunTable (int w) {
    if (w == 0) { return spacing; }
    else { return spacing_aux[w-1]; }
  }
  int getEol() { return r.endofline; }
  int getEolWithin() { return r.endofline_width; }
  
  double getAntenna() { return r.antenna_ratio; }
  double getAntennaDiff() { return r.antenna_diff_ratio; }

  int numInfluence() { return r.inf_sz; }
  int *getInfluence() { return r.influence; }
  
 protected:
  RoutingRules r;

  friend class Technology;
};


class PolyMat : public RoutingMat {
 public:
  PolyMat (char *s) : RoutingMat(s) { }
  int getOverhang (int w) { return (*overhang)[w]; }
  int getNotchOverhang (int w) { return (*notch_overhang)[w]; }
  Contact *getUpC() { return viaup; }

  
 protected:
  int width;

  RangeTable *overhang;		/* poly overhang beyond diffusion */
  RangeTable *notch_overhang;	/* overhang for a notch */
  int *via_n;		      /* spacing of poly via to n-type diff */
  int *via_p;		      /* spacing of poly via to p-type diff */

  friend class Technology;
};

class FetMat : public Material {
 public:
  FetMat (char *s) { name = s; }
  int getSpacing (int w) {
    return (*spacing)[w];
  }
protected:
  int width;
  int num_dummy;		/* # of dummy poly needed */

  friend class Technology;
};


class WellMat : public Material {
 public:
  WellMat (char *s) { name = s; }
  int getOverhang () { return overhang; }
  int getOverhangWelldiff () { return overhang_welldiff; }
  int minArea () { return minarea; }
  int minSpacing(int dev) { return spacing[dev]; }
  int oppSpacing(int dev) { return oppspacing[dev]; }
  int minWidth() { return width; }
  int maxPlugDist() { return plug_dist; }
  
protected:
  int width;
  int *spacing;	      /* to other wells of the same type */
  int *oppspacing;    /* to other wells of a different type */
  int overhang;	      /* overhang from diffusion */
  int overhang_welldiff; /* overhang from well diffusion */
  int plug_dist;      /* max distance to plug */

  friend class Technology;
};


class DiffMat : public Material {
 public:
  DiffMat (char *s) { name = s; }

  /* return diffusion overhang, given width of fet and whether or not
     the edge of the diffusion has a contact or not */
  int effOverhang(int w, int hasvia = 0);
  int viaSpaceEdge ();
  int viaSpaceMid ();
  int minArea () { return minarea; }
  int getPolySpacing () { return polyspacing; }
  int getNotchSpacing () { return notchspacing; }
  int getOppDiffSpacing (int flavor) { return oppspacing[flavor]; }
  int getSpacing (int flavor) { return spacing[flavor]; }
  int getWidth () { return width; }
  int getWdiffToDiffSpacing() { return diffspacing; }
  Contact *getUpC() { return viaup; }
  
protected:
  int width;
  int diffspacing;
  int *spacing;
  int *oppspacing;
  int polyspacing;
  int notchspacing;
  RangeTable *overhang;
  int via_edge;
  int via_fet;

  friend class Technology;
};

class Contact : public Material {
 public:
  Contact (char *s) {
    asym_surround_up = 0;
    asym_surround_dn = 0;
    name = s;
  }
  int getWidth() { return width_int; }
  int getSpacing() { return spacing; } 
  int isSym() { return (asym_surround_up == 0) && (asym_surround_dn == 0); }
  int isAsym() { return !isSym(); }
  int getSym() { return sym_surround_dn; }
  int getSymUp() { return sym_surround_up; }
  int getAsym() { return asym_surround_dn; }
  int getAsymUp() { return asym_surround_up; }
  double getAntenna() { return antenna_ratio; }
  double getAntennaDiff() { return antenna_diff_ratio; }

  int viaGenerate() { return spc_x > 0; }
  int viaGenX() { return spc_x; }
  int viaGenY() { return spc_y; }
  
protected:
  int width_int, spacing;
  Material *lower, *upper;

  int sym_surround_dn;
  int sym_surround_up;
  
  int asym_surround_dn;
  int asym_surround_up;

  double antenna_ratio;
  double antenna_diff_ratio;

  // generate
  int spc_x, spc_y;
  
  friend class Technology;
};


class Technology {
 public:
  static Technology *T;
  static void Init (const char *techfilename);
  
  const char *name;		/* name and date */
  const char *date;		/* string */

  double scale;			/* scale factor to get nanometers from
				   the integer units */

  int nmetals;			/* # of metal layers */

  int dummy_poly;		/* # of dummy poly */

  int welltap_adjust;		/* adjustment for welltap y-center */

  /* indexed by EDGE_PFET, EDGE_NFET */
  int num_devs;			/* # of device types */
  DiffMat **diff[2];		/* diffusion for p/n devices */
  WellMat **well[2];		/* device wells */
  DiffMat **welldiff[2];	/* substrate diffusion */
  FetMat **fet[2];		/* transistors for each type */

  PolyMat *poly;

  RoutingMat **metal;

  int getMaxDiffSpacing ();	/* this space  guarantees correct
				   spacing between any two types of
				   diffusion (not including welldiff) */
  
  int getMaxSameDiffSpacing (); /* this space guarantees correct
				   spacing between two diffusions of
				   the same type, across all diffusion
				   types */

};
  
  


#endif /* __TECHFILE_H__ */
