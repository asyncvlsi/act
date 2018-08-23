#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
EXTRA=prs.c vpi.o

OBJS=vpi_prsim.o prs.o avl.o lex.o heap.o misc.o names.o

SRCS=$(OBJS:.o=.c)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

CFLAGS+=-I$(VCS_HOME)/include
DEPEND_FLAGS+=-I$(VCS_HOME)/include

CLEAN=avl.c avl.h heap.c heap.h lex.c lex.h lzw.c lzw.h misc.c misc.h names.c names.h prs.c prs.h

GENERATED=$(CLEAN)

vpi.o: $(OBJS)
	ld -x -r -o vpi.o $(OBJS)

prs.c:
	./mk_links

-include Makefile.deps
