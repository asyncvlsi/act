#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BINARY=netgen.$(EXT)

weird: all install_conf

TARGETS=$(BINARY)

OBJS=main.o 

SRCS=$(OBJS:.o=.cc)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(BINARY): $(LIB) $(OBJS) $(ACTPASSDEPEND)
	$(CXX) $(CFLAGS) $(OBJS) -o $(BINARY) $(LIBACTPASS)

install_conf: prs2net.conf
	@$(VLSI_TOOLS_SRC)/scripts/install prs2net.conf $(INSTALLLIB)/act/prs2net.conf

-include Makefile.deps
