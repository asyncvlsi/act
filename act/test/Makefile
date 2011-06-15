#-------------------------------------------------------------------------
#
#  Copyright (c) 2011 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

TARGETS=act-test.$(EXT)

OBJS=test.o

SRCS=$(OBJS1:.o=.C)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(TARGETS): $(LIB) test.o $(ACTDEPEND)
	$(CXX) $(CFLAGS) test.o -o $(TARGETS) $(LIBACT)

-include Makefile.deps




