#-------------------------------------------------------------------------
#
#  Copyright (c) 2011 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------

EXE=tfile.$(EXT)

TARGETS=$(EXE)

SRCS=tech.c test.c
OBJS=$(SRCS:.c=.o)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

$(EXE): $(OBJS) $(LIBDEPEND)
	$(CC) $(OBJS) -o $(EXE) $(LIBCOMMON)

-include Makefile.deps
