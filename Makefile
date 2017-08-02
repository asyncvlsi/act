#-------------------------------------------------------------------------
#
#  Copyright (c) 2011 Rajit Manohar
#  All Rights Reserved
#
#-------------------------------------------------------------------------
#
# Make everything, in the right order
# 
SUBDIRS=common pgen xact layout sim analysis synthesis

include $(VLSI_TOOLS_SRC)/scripts/Makefile.std
