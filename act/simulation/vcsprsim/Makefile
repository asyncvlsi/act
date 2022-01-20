#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA  02110-1301, USA.
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
