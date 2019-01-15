#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
BIN=tlint.$(EXT)
BIN2=tr2alint.$(EXT)
TARGETS=$(BIN) $(BIN2)

OBJS2=hs.o
OBJS1=tlint.o

weird: all install_conf

OBJS=$(OBJS1) $(OBJS2)

SRCS=$(OBJS:.o=.cc)

ZLIBS=-lz

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

CFLAGS+=-DHAVE_ZLIB

$(BIN): $(LIB) $(OBJS1) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS1) ../prsim/$(EXT)/prs.o -o $(BIN) $(LIBACT)

$(BIN2): $(LIB) $(OBJS2) $(ACTDEPEND)
	$(CXX) $(CFLAGS) $(OBJS2) -o $(BIN2) $(LIBACT) $(ZLIBS)

install_conf: lint.conf
	@$(VLSI_TOOLS_SRC)/scripts/install lint.conf $(INSTALLLIB)/act/lint.conf
	

-include Makefile.deps
