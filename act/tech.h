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

/**
 * @file tech.h
 *
 * @brief This contains the data structures used to hold simplified
 * technology information in terms of the technology design
 * rules. We only use a simplified version of the design rules that
 * are conservative but sufficient to support layout generation.
 */


/**
 * @class RangeTable
 *
 * @brief A range table is a table of values that are indexed by
 * ranges. It can be viewed as a map from contiguous ranges [a,b] to
 * v.
 * 
 * More information about the format of a range table can be
 * found at:
 *    https://avlsi.csl.yale.edu/act/doku.php?id=config:layout&s[]=range&s[]=table#range_tables
 * This is used to hold design rules where the rule is indexed by the
 * value of another parameter (e.g. minimum width of a metal depends
 * on the length of the metal wire)
 */
class RangeTable {
 public:
  /**
   * @param _s is the size of the range table
   * @param _tab is the range table, in the standard range table
   * format
   */
  RangeTable (int _s, int *_tab) {
    sz = _s;
    table = _tab;
    minval = -1;
  }

  /**
   * @return the minimum value in the range table
   */
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

  /**
   * An array operator overload that is used so that the range table
   * can be accessed like a simple array.
   *
   * @param idx is the index to look up in the range table
   * @return the value stored in the range table for the given index
   */
  int operator[](int idx);

  /**
   * @return the number of entries in the range table
   */
  int size();

  /**
   * Return the minimum value for the i-th range in the range tablel
   * @param idx is the range number
   * @return the min value of the idx-th range.
   */
  int range_threshold (int idx);
  
 protected:
  int sz;			///< # of table entries
  int *table;			///< range table contents
  int minval;			///< minimum value of range
};

class Contact;
class Material;

/**
 * @class GDSLayer
 *
 * @brief Holds information about a GDS layer. Any Material will have
 * a footprint on a single or multiple GDS layers. These layers are
 * specified in the layout configuration file (layout.conf). More
 * details on this are available at
 * https://avlsi.csl.yale.edu/act/doku.php?id=config:layout
 */
class GDSLayer {
private:
  const char *name;		///< the name of the GDS layer used in
				///the layout configuration file
  int major, minor;		///< the GDS major and minor number
				///for this layer
  
  list_t *mats;			///< link back to materials that use
				///this GDS layer
  
public:

  /**
   * Create a GDS layer
   * @param nm is the name of the layer
   * @param maj is the major number for the GDS layer
   * @param min is the minor number for the GDS layer
   */
  GDSLayer (char *nm, int maj, int min) {
    name = string_cache (nm);
    major = maj;
    minor = min;
    mats = NULL;
  }

  /**
   * Add a material to the list of materials that use this GDS layer
   * @param m is the Material
   */
  void addMat (Material *m);

  /**
   * Return a listitem_t used to iterate over the materials that use
   * this GDS layer
   * @return NULL if there are no materials, or the first item in the
   * list
   */
  listitem_t *matList () { if (mats) { return list_first (mats); } else { return NULL; } }

  /**
   * @return the major number of this GDS layer
   */
  int getMajor() { return major; }

  /**
   * @return the minor number of this GDS layer
   */
  int getMinor() { return minor; }
};


/**
 * @class Material
 * 
 * @brief Used to hold information about a material. Materials
 * correspond to abstract geometry, and get mapped to one or more GDS
 * layers. More details are avaiable at:
 * https://avlsi.csl.yale.edu/act/doku.php?id=config:layout
 */
class Material {
 public:
  /** 
   * Create new material
   * @param nm is the name of the material. This is typically the
   * magic layer name for drawing
   */
  Material(const char *nm = NULL) {
    if (nm) {
      name = string_cache (nm);
    }
    else {
      name = NULL;
    }
    width = NULL;
    spacing_w = NULL;
    minarea = 0;
    maxarea = 0;
    viaup = NULL;
    viadn = NULL;
    gds = NULL;
    gds_bloat = NULL;
  }

  /**
   * @return the name of the material
   */
  virtual const char *getName() { return name; }

  /**
   * Given a list of GDS names, add those to the GDS layers that must
   * be generated for this material.
   * @param table is an array of GDS names
   * @param sz is the size of the table
   */
  void addGDS (char **table, int sz);

  /**
   * Store the GDS bloat table associated with the GDS table that was
   * added for this material
   * @param table is the list of bloat values
   * @param sz is the size of the table
   */
  void addGDSBloat (int *table, int sz);

  /**
   * This returns the name of the via to connect up from the material,
   * if any
   * @return the name of the via
   */
  const char *viaUpName();

  /**
   * @return the minimum area for this material (0 = none)
   */
  int minArea() { return minarea; }

  /**
   * @return the minimum width for this material
   */
  int minWidth () { return width->min(); }

  /**
   * @return the minimum spacing for this material
   */
  int minSpacing() { return spacing_w->min(); }

protected:
  const char *name;		///< drawing name in magic

  RangeTable *width;		///< min width range table (indexed by length)
  RangeTable *spacing_w;	///< min spacing range table (indexed by width)

  int minarea;			///< minimum area for material; 0
				///means no constraint
  
  int maxarea;			///< maximum area for material; 0
				///means no constraint

  Contact *viaup;		///< contact/via to material that is above
  Contact *viadn;		///< contact/via to material that is below

  list_t *gds;			///< GDS layer list
  int *gds_bloat;		///< GDS bloat table

  friend class Technology;
};


/**
 * @class RoutingRules
 *
 * @brief Holds design rules for all routing materials (metals, poly)
 */
struct RoutingRules {
  int endofline;		///< end of line extension rule
  int endofline_width;		///< width for end of line extension
  int minjog;			///< minumum turn distance on a jog

  double antenna_ratio;		///< antenna ratios for this layer
  double antenna_diff_ratio;	///< antenna diffusion ratio
  
  int pitch;			///< routing material pitch (can be
				///used to be larger than the minwidth
				///plus min spacing
  
  int lef_width;		///< width of the layer in the
				///LEF. Sometimes it is better to have
				///the LEF width to be larger than the
				///min width for the material so that
				///the routing grid is easier to work
				///with for the detailed router

  const char *lef_name;		///< lef/def name for this material

  unsigned int routex:1;	///< can be used for x routing
  unsigned int routey:1;	///< can be used for y routing

  int runlength_mode;		///< 0 = parallelrunlength, 1 = twowidths

  int runlength;		///< negative if it doesn't exist;
				/// otherwise # of parallel run
				/// lengths
  int *parallelrunlength;	///< parallel run length options
  
  RangeTable **spacing_aux;	///< extra range tables for parallel run
				/// lengths

  int *influence;		///< spacing influence table
  int inf_sz;			///< # of lines in the influence table
};


/**
 * @class RoutingMat
 *
 * @brief Used to hold routing materials with extra routing design
 * rules.
 */
class RoutingMat : public Material {
public:
  /**
   * Create a new routing material
   * @param s is the name of the routing material
   */
  RoutingMat (char *s) : Material(s) {
    r.influence = NULL;
    r.inf_sz = 0;
    r.pitch = 0;
    r.lef_width = 0;
    r.lef_name = NULL;
    r.runlength = -1;
    r.runlength_mode = -1;
    r.parallelrunlength = NULL;
    r.spacing_aux = NULL;
  }

  /**
   * Set the LEF name for this layer
   * @param s is the LEF name
   */
  void setLEFName (char *s) { r.lef_name = Strdup (s); }

  /**
   * @return the pitch for this routing layer
   */
  int getPitch() { return r.pitch; }

  /**
   * @return width to be used for the LEF file
   */
  int getLEFWidth() { return r.lef_width; }

  /**
   * @return the LEF name; if not specified, then use the normal
   * material name
   */
  const char *getLEFName() {
    if (r.lef_name) { return r.lef_name; }
    return getName();
  }

  /**
   * @return the name of the contact to connect up from this layer to
   * the next routing layer
   */
  Contact *getUpC() { return viaup; }

  /**
   * @param w is the width of the material
   * @return the minimum spacing given the width 
   */
  int getSpacing(int w) { return (*spacing_w)[w]; }

  /**
   * @return 1 if this has non-trivial spacing rules that have to be
   * recoded in the LEF
   */
  int isComplexSpacing() {
    if (r.runlength != -1) return 1;
    if (spacing_w->size() > 1) return 1;
    return 0;
  }

  /**
   * Complex spacing can be of type 0 (simple parallel run length
   * rule), or of type 1 (a twowidth rule)
   * @return the type of complex spacing rule 
   */
  int complexSpacingMode() { return r.runlength_mode; }

  /**
   * @return the size of the parallel run length table
   */
  int numRunLength()  { return r.runlength; }

  /**
   * @return the run length for a given width
   */
  int getRunLength(int w) { return r.parallelrunlength[w]; }

  /**
   * @return the RangeTable for parallel run lengths
   */
  RangeTable *getRunTable (int w) {
    if (w == 0) { return spacing_w; }
    else { return r.spacing_aux[w-1]; }
  }

  /**
   * @return end-of-line extension 
   */
  int getEol() { return r.endofline; }

  /**
   * @return end-of-line width rule
   */
  int getEolWithin() { return r.endofline_width; }

  /**
   * @return antenna ratio
   */
  double getAntenna() { return r.antenna_ratio; }

  /**
   * @return antenna diffusion ratio
   */
  double getAntennaDiff() { return r.antenna_diff_ratio; }

  /**
   * @return size of the spacing influence table
   */
  int numInfluence() { return r.inf_sz; }

  /**
   * @return the influence table
   */
  int *getInfluence() { return r.influence; }
  
 protected:
  RoutingRules r;

  friend class Technology;
};


/**
 * @class PolyMat
 *
 * @brief This is for polysilicon, which is a special routing material
 * with more issues.
 */
class PolyMat : public RoutingMat {
 public:
  PolyMat (char *s) : RoutingMat(s) { }

  /**
   * @return the poly overhang over diffusion given the width of the
   * poly
   */
  int getOverhang (int w) { return (*overhang)[w]; }

  /**
   * @return the poly overhang for a notch given the width of the poly
   */
  int getNotchOverhang (int w) { return (*notch_overhang)[w]; }

  /**
   * @return the contact from poly up to the first metal routing layer
   */
  Contact *getUpC() { return viaup; }

  /**
   * @return spacing of poly via to n-type diffusion (type is the
   * diffusion flavor)
   */
  int getViaNSpacing (int type) {
    if (!via_n) {
      return 1;
    }
    return via_n[type];
  }
  
  /**
   * @return spacing of poly via to p-type diffusion (type is the
   * diffusion flavor)
   */
  int getViaPSpacing (int type) {
    if (!via_p) {
      return 1;
    }
    return via_p[type];
  }

  
 protected:
  RangeTable *overhang;		///< poly overhang beyond diffusion
  RangeTable *notch_overhang;	///< overhang for a notch
  int *via_n;		      ///< spacing of poly via to n-type diff
  int *via_p;		      ///< spacing of poly via to p-type diff

  friend class Technology;
};

/**
 * @class FetMat
 *
 * @brief Used for transistors
 */
class FetMat : public Material {
 public:
  /**
   * @param s is the name of the transistor
   */
  FetMat (char *s) : Material(s) { num_dummy = 0; }

  /**
   * @return the spacing given the length of the transitor ("width" of
   * drawn material)
   */
  int getSpacing (int w) {
    return (*spacing_w)[w];
  }

  /**
   * @return the number of dummy polys needed
   */
  int numDummyPoly() { return num_dummy; }
  
protected:
  int num_dummy;		///< # of dummy poly needed

  friend class Technology;
};


/**
 * @class WellMat
 *
 * @brief Used for wells
 */
class WellMat : public Material {
 public:
  /**
   * @param s is the name of the well
   */
  WellMat (char *s) : Material (s) { }

  /**
   * @return well overhang over diffusion
   */
  int getOverhang () { return overhang; }

  /**
   * @return well overhang over well diffusion (pplus/nplus)
   */
  int getOverhangWelldiff () { return overhang_welldiff; }

  /**
   * @return minimum spacing to well for device flavor "dev", for same
   * type of well (i.e. n-to-n or p-to-p)
   */
  int minSpacing(int dev) { return spacing[dev]; }
  
  /**
   * @return minimum spacing to well for device flavor "dev", for
   * opposite type of well (i.e. n-to-p or p-to-n)
   */
  int oppSpacing(int dev) { return oppspacing[dev]; }

  /**
   * @return the maximum distance from a well to a well plug
   */
  int maxPlugDist() { return plug_dist; }
  
protected:
  int *spacing;	      ///< to other wells of the same type
  int *oppspacing;    ///< to other wells of a different type
  int overhang;	      ///< overhang from diffusion 
  int overhang_welldiff; ///< overhang from well diffusion 
  int plug_dist;      ///< max distance to plug

  friend class Technology;
};


/**
 * @class DiffMat
 *
 * @brief Used to hold rules for diffusion
 */
class DiffMat : public Material {
 public:
  /**
   * @param s is the name of the diffusion layer
   */
  DiffMat (char *s) : Material (s) { }

  /**
   * Used to calculate the amount of diffusion overhang given the
   * width of the last fet in a transistor stack (a.k.a. channel
   * connected region). The hasvia flag is used to indicate whether
   * this diffusion needs to overhang enough to accommodate a
   * via. This uses viaSpaceEdge() as well to make sure there is
   * enough space.
   *
   * @param w is the width of the last fet
   * @param hasvia is 1 if there is a via here, 0 otherwise
   * @return the amount of diffusion overhang
   */
  int effOverhang(int w, int hasvia = 0);

  /**
   * @return the amount of space needed to accomodate a via where one
   * side has a fet and the other side is the edge.
   */
  int viaSpaceEdge ();

  /**
   * @return the amount of space needed to accomodaet a via between
   * two fets (i.e. in the middle of a transistor stack)
   */
  int viaSpaceMid ();

  /**
   * @return spacing from poly to diffusion
   */
  int getPolySpacing () { return polyspacing; }

  /**
   * @return the amount of space for the notch in the diffusion. This
   * is the space from the notch edge to poly.
   */
  int getNotchSpacing () { return notchspacing; }

  /**
   * @param flavor is the transistor flavor
   * @return the spacing to the opposite type (i.e. n-to-p or p-to-n)
   * of diffusion
   */
  int getOppDiffSpacing (int flavor) { return oppspacing[flavor]; }

  /**
   * @param flavor is the transistor flavor
   * @return the spacing from this diffusion to the diffusion of the
   * same type (n/p) of the specified flavor
   */
  int getSpacing (int flavor) { return spacing[flavor]; }

  /**
   * @return diffusion to well-diffusion spacing (should just be the
   * diffusion spacing)
   */
  int getWdiffToDiffSpacing() { return diffspacing; }

  /**
   * @return the spacing from the edge of a via to the transistor
   */
  int getViaFet() { return via_fet; }

  /**
   * @return the contact from diffusion up to the first metal routing
   * layer 
   */
  Contact *getUpC() { return viaup; }
  
protected:
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



/**
 * @class Contact
 *
 * @brief Used to represent a contact between two routing layers
 * (adjacent) or between a material and the first routing layer for
 * it.
 *
 * A contact can be symmetric or asymmetric. A symmetric contact has
 * equal amounts of overhang around the via. An asymmetric contact has
 * asymmetric overhang.
 */
class Contact : public Material {
 public:
  /**
   * @param s is the name of the contact
   */
  Contact (char *s) : Material (s) {
    asym_surround_up = 0;
    asym_surround_dn = 0;
    style = NULL;
    lef_width = 0;
  }

  /**
   * @return 1 if this is a symmetric contact, 0 otherwise
   */
  int isSym() { return (asym_surround_up == 0) && (asym_surround_dn == 0); }

  /**
   * @return 1 if this is not a symmetric contact, 0 otherwise
   */
  int isAsym() { return !isSym(); }

  /**
   * @return the symmetric surround on the layer below this via
   */
  int getSym() { return sym_surround_dn; }

  /**
   * @return the symmetric surround on the layer above this via
   */
  int getSymUp() { return sym_surround_up; }

  /**
   * @return the asymmetric surround on the layer below this via
   */
  int getAsym() { return asym_surround_dn; }
  
  /**
   * @return the asymmetric surround on the layer above this via
   */
  int getAsymUp() { return asym_surround_up; }

  /**
   * @return the antenna ratio for this layer
   */
  double getAntenna() { return antenna_ratio; }

  /**
   * @return the antenna diff ratio
   */
  double getAntennaDiff() { return antenna_diff_ratio; }

  /**
   * @return 1 if this has a viaGenerate rule
   */
  int viaGenerate() { return spc_x > 0; }

  /**
   * @return the x-spacing for a via generate rule
   */
  int viaGenX() { return spc_x; }

  /**
   * @return the y-spacing for a via generate rule
   */
  int viaGenY() { return spc_y; }

  /**
   * @return the LEF width to be used
   */
  int getLEFWidth() { return lef_width; }

  /**
   * @return the magic drawing style string
   */
  const char *getDrawingStyle () { return style; }

  /**
   * Set the magic drawing style string
   */
  void setDrawingStyle (const char *s) { style = s; }

  /**
   * @return the name of the layer below
   */
  const char *getLowerName() { return lower->getName(); }

  /**
   * @return the name of the layer above
   */
  const char *getUpperName() { return upper->getName(); }
  
protected:
  Material *lower, *upper;

  int lef_width;

  int sym_surround_dn;
  int sym_surround_up;
  
  int asym_surround_dn;
  int asym_surround_up;

  double antenna_ratio;
  double antenna_diff_ratio;

  const char *style;

  // generate
  int spc_x, spc_y;
  
  friend class Technology;
};


/**
 * @class Technology
 *
 * @brief This holds all the technology design rules (or at least the
 * approximate ones) used by the ACT library and tools. This is
 * sufficient to generate the technology LEF used for routing. There
 * can only be one Technology allocated. This information is loaded
 * from the layout.conf ACT configuration file.
 */
class Technology {
 public:
  static Technology *T;
  static void Init ();		///< make sure that layout.conf has been loaded!
  
  const char *name;		/* name and date */
  const char *date;		/* string */

  double scale;			/* scale factor to get nanometers from
				   the integer units */

  int nmetals;			/* # of metal layers */

  int dummy_poly;		/* # of dummy poly */

  int welltap_adjust;		/* adjustment for welltap y-center */

  GDSLayer *GDSlookup (const char *name); // return gds layer

  /* indexed by EDGE_PFET, EDGE_NFET */
  int num_devs;			/* # of device types */
  DiffMat **diff[2];		/* diffusion for p/n devices */
  WellMat **well[2];		/* device wells */
  DiffMat **welldiff[2];	/* substrate diffusion */
  FetMat **fet[2];		/* transistors for each type */
  Material **sel[2];		/* selects for each diffusion */

  PolyMat *poly;

  RoutingMat **metal;

  struct Hashtable *gdsH;	/* gds layers */

  int getMaxDiffSpacing ();	/* this space  guarantees correct
				   spacing between any two types of
				   diffusion (not including welldiff) */

  int getMaxWellDiffSpacing ();	/* this space  guarantees correct
				   spacing between any two types of
				   well diffusion */
  
  int getMaxSameDiffSpacing (); /* this space guarantees correct
				   spacing between two diffusions of
				   the same type, across all diffusion
				   types */

};
  
  


#endif /* __TECHFILE_H__ */
