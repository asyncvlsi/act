#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

TARGETS=aflat.$(EXT)

OBJS=main.o aflat.o

SRCS=$(OBJS1:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(TARGETS): $(LIB) $(OBJS) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGETS) $(LIBACT)

-include Makefile.deps




