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

/* 
   Macro configuration file
      begin macros

      begin <expanded_name>
        string lef     "leffile"
	string spice   "spicefile"
	string verilog "verilogfile"
	int llx 
	int lly
	int urx 
	int ury
      end

      begin <unexpanded_name>.gen "string"   <-- generator
      
      end
*/

class ExternMacro {
 public:
  ExternMacro (Process *p);
  ~ExternMacro ();

  bool isValid() { return (_lef == nullptr ? false : true); }

  const char *getLEFFile() { return _lef; }

  void getBBox (long *bllx, long *blly,
		long *burx, long *bury) {
    *bllx = llx;
    *blly = lly;
    *burx = urx;
    *bury = ury;
  }

  const char *getSPICEFile() { return _spice; }

  const char *getVerilogFile() { return _verilog; }

  const char *getName() { return _name; }

 private:
  Process *_p;
  char *_name;			// name of macro
  const char *_lef;		// lef path
  const char *_spice;		// spice path
  const char *_verilog;		// verilog path
  
  long llx, lly, urx, ury;	// bounding box
};


#endif /* __ACT_EXTMACRO_H__ */
