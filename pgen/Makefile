#-------------------------------------------------------------------------
#
#  Copyright (c) 2011 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

EXE=pgen.$(EXT)

TARGETS=$(EXE)

SRCS=pgen.c genwalk.c print.c
OBJS=$(SRCS:.c=.o)

TARGETLIBS=expr.c expr.h

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(EXE): $(OBJS) $(LIBDEPEND)
	$(CC) $(OBJS) -o $(EXE) $(LIBCOMMON)

-include Makefile.deps
