/*************************************************************************
 *
 *  Copyright (c) 2022 Rajit Manohar
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
#ifndef __ACT_EXTMACRO_H__
#define __ACT_EXTMACRO_H__

#include <act/act.h>

/**
 * @class ExternMacro
 *
 * @brief This is used to record information about a macro from an ACT
 * configuration file.
 *
 * A macro is specified in a configuration file in the macro section
 * as follows
 *
 * ```
 * begin macros
 *
 *   begin <expanded_name>
 *      string lef     "leffile"
 *      string spice   "spicefile"
 *      string verilog "verilogfile"
 *      int llx <val>
 *	int lly <val>
 *	int urx <val>
 *	int ury <val>
 *   end
 *
 * end
 * ```
 *
 * A macro generator can be specified using
 *
 * ```
 * begin macros
 *
 *    string <unexpanded_name>.gen "string" 
 *
 * end
 * ```
*/
class ExternMacro {
 public:
  /**
   * Search for an external macro definition for process p
   *
   * @param p is the process to look for
   */
  ExternMacro (Process *p);
  ~ExternMacro ();

  /**
   * @return true if a valid macro for the process was found, false
   * otherwise 
   */
  bool isValid() { return (_lef == nullptr ? false : true); }

  /**
   * @return the LEF file name associated with the macro
   */
  const char *getLEFFile() { return _lef; }

  /**
   * @param bllx the lower left x coordinate of the macro
   * @param blly the lower left y coordinate of the macro
   * @param burx the upper right x coordinate of the macro
   * @param bury the upper right y coordinate of the macro
   */
  void getBBox (long *bllx, long *blly,
		long *burx, long *bury) {
    *bllx = llx;
    *blly = lly;
    *burx = urx;
    *bury = ury;
  }

  /**
   * @return the SPICE file name for the macro
   */
  const char *getSPICEFile() { return _spice; }

  /**
   * @return the Verilog file name for the macro
   */
  const char *getVerilogFile() { return _verilog; }

  /**
   * @return the name of the macro
   */
  const char *getName() { return _name; }

 private:
  Process *_p;			///< the process
  char *_name;			///< name of macro
  const char *_lef;		///< lef path
  const char *_spice;		///< spice path
  const char *_verilog;		///< verilog path
  
  long llx, lly, urx, ury;	///< bounding box
};


#endif /* __ACT_EXTMACRO_H__ */
