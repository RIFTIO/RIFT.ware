# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 11/25/2013
# 

find_upward = $(word 1, $(shell while [ `pwd` != / ] ; do find `pwd` -maxdepth 1 -name $1 ; cd .. ; done))

makefile.top := $(call find_upward, "Makefile.top")

-include $(makefile.top)
