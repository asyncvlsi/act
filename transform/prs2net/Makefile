#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BINARY=netgen.$(EXT)

TARGETS=$(BINARY)

OBJS=main.o 

SRCS=$(OBJS:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BINARY): $(LIB) $(OBJS) $(ACTPASSDEPEND)
	$(CXX) $(CFLAGS) $(OBJS) -o $(BINARY) $(LIBACTPASS)

-include Makefile.deps
