# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 08/28/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Define a Makefile function: find_upwards(filename)
#
# Searches for a file of the given name in the directory ., .., ../.., ../../.., etc.,
# until the file is found or the root directory is reached
##
find_upward = $(word 1, $(shell while [ `pwd` != / ] ; do find `pwd` -maxdepth 1 -name $1 ; cd .. ; done))

##
# Call find_upward("Makefile.top") to find the nearest upwards adjacent Makefile.top
##
makefile.top := $(call find_upward, "Makefile.top")

##
# If Makefile.top was found, then include it
##
include $(makefile.top)
