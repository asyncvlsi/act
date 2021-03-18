#-------------------------------------------------------------------------
#
#  Copyright (c) 2018-2020 Rajit Manohar
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
TARGETINCS=passes.h
TARGETINCSUBDIR=act
SUBDIRS=flatten netgen cells booleanize state sizing

LIB1=libactpass_$(EXT).a
LIB2=libactpass_sh_$(EXT).so

TARGETLIBS=$(LIB1) $(LIB2)

USEOBJS=booleanize/$(EXT)/booleanize.o \
	netgen/$(EXT)/netlist.o \
	netgen/$(EXT)/emit.o \
	flatten/$(EXT)/flatten.o \
	cells/$(EXT)/cells.o \
	state/$(EXT)/statepass.o \
	sizing/$(EXT)/sizing.o

SHOBJS=$(USEOBJS:.o=.os)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(LIB1): $(USEOBJS)
	ar ruv $(LIB1) $(USEOBJS)
	$(RANLIB) $(LIB1)

$(LIB2): $(SHOBJS)
	$(VLSI_TOOLS_SRC)/scripts/linkso $(LIB2) $(SHOBJS) $(SHLIBACT)

-include Makefile.deps
