# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 2015/09/10
# 

find_upward = $(word 1, $(shell while [ `pwd` != / ] ; do find `pwd` -maxdepth 1 -name $1 ; cd .. ; done))

makefile.top := $(call find_upward, "Makefile.top")

-include $(makefile.top)
