#-------------------------------------------------------------------------
#
#  Copyright (c) 2011-2018 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
#
# Make everything, in the right order
# 
SUBDIRS=common pgen act passes transform simulation layout

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std
