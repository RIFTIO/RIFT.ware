# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 05/15/2015
# 

include(rift_plugin)

set(TASKLET_NAME rwvnfmtasklet)

##
# This function creates an install target for the plugin artifacts
##
rift_install_python_plugin(${TASKLET_NAME} ${TASKLET_NAME}.py)

# Workaround RIFT-6485 - rpmbuild defaults to python2 for
# anything not in a site-packages directory so we have to
# install the plugin implementation in site-packages and then
# import it from the actual plugin.
rift_python_install_tree(
  FILES
    rift/tasklets/${TASKLET_NAME}/__init__.py
    rift/tasklets/${TASKLET_NAME}/${TASKLET_NAME}.py
    rift/tasklets/${TASKLET_NAME}/mon_params.py
  COMPONENT ${PKG_LONG_NAME}
  PYTHON3_ONLY)

rift_add_subdirs(test)
