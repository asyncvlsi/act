#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA  02110-1301, USA.
#
#-------------------------------------------------------------------------
TARGETLIBS=libactpass_$(EXT).a
TARGETINCS=passes.h
TARGETINCSUBDIR=act
SUBDIRS=flatten netgen cells booleanize

USEOBJS=flatten/$(EXT)/aflat.o \
	booleanize/$(EXT)/booleanize.o \
	netgen/$(EXT)/netlist.o \
	netgen/$(EXT)/emit.o \
 	flatten/$(EXT)/flatten.o \
	cells/$(EXT)/cells.o

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(TARGETLIBS): $(USEOBJS)
	ar ruv $(TARGETLIBS) $(USEOBJS)
	$(RANLIB) $(TARGETLIBS)

-include Makefile.deps
