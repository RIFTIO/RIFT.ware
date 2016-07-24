# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Kiran Kashalkar
# Creation Date: 08/18/2015
# 

##
# DEPENDENCY ALERT
# The submodule dependencies must be specified in the
# .gitmodules.dep file at the top level (supermodule) directory
# If this submodule depends other submodules remember to update
# the .gitmodules.dep
##

cmake_minimum_required(VERSION 2.8)

##
# Submodule specific includes will go here,
# These are specified here, since these variables are accessed
# from multiple sub directories. If the variable is subdirectory
# specific it must be declared in the subdirectory.
##


##
# Include the subdirs
##
set(
  subdirs
    about
    composer
    debug
#    goodbyworld
#    helloworld
    launchpad
  )
rift_add_subdirs(
  SUBDIR_LIST
    ${subdirs}
  )
