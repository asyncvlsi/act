#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

TARGETLIBS=libactpass_$(EXT).a
TARGETINCS=passes.h
TARGETINCSUBDIR=act

USEOBJS=aflat.o netgen.o flatten.o

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(TARGETLIBS): $(USEOBJS)
	ar ruv $(TARGETLIBS) $(USEOBJS)
	$(RANLIB) $(TARGETLIBS)

-include Makefile.deps
