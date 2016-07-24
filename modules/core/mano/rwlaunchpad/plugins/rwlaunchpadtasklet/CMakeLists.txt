# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 05/15/2015
# 

include(rift_plugin)

set(TASKLET_NAME rwlaunchpad)

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
    rift/tasklets/${TASKLET_NAME}/convert_pkg.py
    rift/tasklets/${TASKLET_NAME}/datacenters.py
    rift/tasklets/${TASKLET_NAME}/export.py
    rift/tasklets/${TASKLET_NAME}/extract.py
    rift/tasklets/${TASKLET_NAME}/image.py
    rift/tasklets/${TASKLET_NAME}/message.py
    rift/tasklets/${TASKLET_NAME}/onboard.py
    rift/tasklets/${TASKLET_NAME}/state.py
    rift/tasklets/${TASKLET_NAME}/tasklet.py
    rift/tasklets/${TASKLET_NAME}/tosca.py
    rift/tasklets/${TASKLET_NAME}/uploader.py
  COMPONENT ${PKG_LONG_NAME}
  PYTHON3_ONLY)

rift_python_install_tree(
  FILES
    rift/package/__init__.py
    rift/package/archive.py
    rift/package/charm.py
    rift/package/checksums.py
    rift/package/config.py
    rift/package/convert.py
    rift/package/icon.py
    rift/package/image.py
    rift/package/package.py
    rift/package/script.py
    rift/package/store.py
  COMPONENT ${PKG_LONG_NAME}
  PYTHON3_ONLY)

rift_add_subdirs(test)
