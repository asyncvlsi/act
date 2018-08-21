#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

TARGETLIBS=libactpass_$(EXT).a
TARGETINCS=passes.h
TARGETINCSUBDIR=act

USEOBJS=aflat.o netgen.o

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(TARGETS): $(USEOBJS)
	ar ruv $(TARGETS) $(USEOBJS)
	$(RANLIB) $(TARGETS)

-include Makefile.deps
