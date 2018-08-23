#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BIN1=prsim.$(EXT)
BIN2=prspack.$(EXT)
TARGETS=$(BIN1) $(BIN2)

OBJS1=prsim.o channel.o prs.o
OBJS2=prspack.o

OBJS=$(OBJS1) $(OBJS2)

SRCS=$(OBJS:.o=.c)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BIN1): $(LIB) $(OBJS1) $(LIBDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) -o $(BIN1) $(LIBCOMMON) -ledit

$(BIN2): $(LIB) $(OBJS2) $(LIBDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BIN2) $(LIBCOMMON)


-include Makefile.deps
