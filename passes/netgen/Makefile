#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

weird: install_inc all

EXTRA=netgen.o 
TARGETINCS=netlist.h
TARGETINCSUBDIR=act/passes

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

netgen.o: netlist.o emit.o
	ld -r -o netgen.o netlist.o emit.o

-include Makefile.deps
