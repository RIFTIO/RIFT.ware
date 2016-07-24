# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Manish Patel
# Creation Date: 10/28/2015
# 

include(rift_plugin)

set(TASKLET_NAME rwconmantasklet)
set(CONMAN_INSTALL "etc/conman")

##
# Install translation script in demos
##
install(
  FILES
    rift/tasklets/${TASKLET_NAME}/xlate_cfg.py
    rift/tasklets/${TASKLET_NAME}/xlate_tags.yml
    rift/tasklets/${TASKLET_NAME}/juju_if.py
  DESTINATION ${CONMAN_INSTALL}
  COMPONENT ${PKG_LONG_NAME})


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
    rift/tasklets/${TASKLET_NAME}/rwconman_config.py
    rift/tasklets/${TASKLET_NAME}/rwconman_events.py
    rift/tasklets/${TASKLET_NAME}/jujuconf.py
    rift/tasklets/${TASKLET_NAME}/juju_if.py
    rift/tasklets/${TASKLET_NAME}/juju_intf.py
    rift/tasklets/${TASKLET_NAME}/RiftCA.py
    rift/tasklets/${TASKLET_NAME}/riftcm_config_plugin.py
    rift/tasklets/${TASKLET_NAME}/RiftCM_rpc.py
    rift/tasklets/${TASKLET_NAME}/rwconman_conagent.py
  COMPONENT ${PKG_LONG_NAME}
  PYTHON3_ONLY)
