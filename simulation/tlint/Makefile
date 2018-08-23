#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BIN=tlint.$(EXT)
TARGETS=$(BIN)

OBJS=tlint.o

SRCS=$(OBJS:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BIN): $(LIB) $(OBJS) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS) ../prsim/$(EXT)/prs.o -o $(BIN) $(LIBACT)

-include Makefile.deps
