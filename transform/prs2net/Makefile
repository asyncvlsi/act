#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BINARY=netgen.$(EXT)

TARGETS=$(BINARY)
TARGETINCS=netlist.h
TARGETINCSUBDIR=act

EXTRA=../../passes/$(EXT)/netgen.o

OBJS=main.o netlist.o emit.o

SRCS=$(OBJS:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BINARY): $(LIB) $(OBJS) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS) -o $(BINARY) $(LIBACT)

$(EXTRA): netlist.o emit.o
	-if [ ! -d ../../passes/$(EXT) ]; \
         then \
           mkdir ../../passes/$(EXT); \
        fi
	ld -r -o $(EXTRA) netlist.o emit.o

-include Makefile.deps
