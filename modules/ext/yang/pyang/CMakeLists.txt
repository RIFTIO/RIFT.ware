include(rift_externalproject)

set(pyang_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/pyang)
set(pyang_yang_dir ${pyang_src_dir}/modules)

##
# These files are exported to the rest of the build
##

rift_externalproject_add(
  pyang
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pyang
  CONFIGURE_COMMAND echo
  BUILD_COMMAND
    python ./setup.py build
  INSTALL_COMMAND
    python ./setup.py install --prefix=${CMAKE_INSTALL_PREFIX}/usr
  BCACHE_COMMAND
  export PYTHONPATH=$ENV{PYTHONPATH}:${RIFT_SUBMODULE_INSTALL_PREFIX}/pyang/${CMAKE_INSTALL_PREFIX}/usr/lib/python2.7/site-packages
  && mkdir -p ${RIFT_SUBMODULE_INSTALL_PREFIX}/pyang/${CMAKE_INSTALL_PREFIX}/usr/lib/python2.7/site-packages
  && python ./setup.py install --prefix=${RIFT_SUBMODULE_INSTALL_PREFIX}/pyang/${CMAKE_INSTALL_PREFIX}/usr
)
