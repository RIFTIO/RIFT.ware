# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 11/25/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

find_upward = $(word 1, $(shell while [ `pwd` != / ] ; do find `pwd` -maxdepth 1 -name $1 ; cd .. ; done))

makefile.top := $(call find_upward, "Makefile.top")

-include $(makefile.top)
