#-------------------------------------------------------------------------
#
#  Copyright (c) 2011-2019 Rajit Manohar
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
LIB=libactscm_$(EXT).a
LIB2=libactscmcli_$(EXT).a
TARGETLIBS=$(LIB) $(LIB2)

TARGETINCS=lisp.h lispCli.h

OBJSC=lispA-Z.o lispArith.o lispEval.o lispFrame.o \
	lispGC.o lispIO.o lispBuiltin.o lispMain.o lispParse.o \
	lispPrint.o lispString.o lispTrace.o lispPath.o

OBJS=$(OBJSC) lispCli.o

SRCS=$(OBJSC:.o=.c)

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std

install_conf: scm-install

$(LIB): $(OBJSC)
	ar ruv $(LIB) $(OBJSC)
	$(RANLIB) $(LIB)

$(LIB2): lispCli.o
	ar ruv $(LIB2) lispCli.o
	$(RANLIB) $(LIB2)

scm-install: default.scm
	@if [ ! -d $(INSTALLLIB)/scm ]; then mkdir $(INSTALLLIB)/scm; fi
	@$(VLSI_TOOLS_SRC)/scripts/install default.scm $(INSTALLLIB)/scm/default.scm

-include Makefile.deps

