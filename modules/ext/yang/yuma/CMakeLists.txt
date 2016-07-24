include(rift_externalproject)
include(rift_pkg_config)

set(yuma_src_dir
  ${RIFT_SUBMODULE_SOURCE_ROOT}/yuma/yuma123-2.2.5)
set(ncx_yang_dir ${yuma_src_dir}/netconf/modules/netconfcentral)

##
# These files are exported to the rest of the build
##
set(ncx_yang_files yuma-ncx.yang yangdump.yang yuma-app-common.yang yuma-types.yang)
rift_files_prepend_path(source_yang_files ${ncx_yang_dir} ${ncx_yang_files})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "DEBUG=1")
  set(PKG_CONFIG_FLAGS "CFLAGS=-g -O0" "CXXFLAGS=-g -O0")
endif()

#set(make_args ${PKG_DEBUG_FLAGS} LIB64=1 USE_WERROR=1 NOTOASTER=1)

rift_externalproject_add(
  yuma123
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/yuma123-2.2.5
  CONFIGURE_COMMAND ./configure
    --prefix=${CMAKE_INSTALL_PREFIX}/usr
    ${PKG_CONFIG_FLAGS}
  BUILD_COMMAND $(MAKE) ${make_args}
  INSTALL_COMMAND $(MAKE) ${make_args} install
  BCACHE_COMMAND
    $(MAKE) DESTDIR=${RIFT_SUBMODULE_INSTALL_PREFIX}/yuma123 ${make_args} install
)

externalproject_add_step(
  yuma123
  autoreconf
  COMMAND autoreconf -i -f
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)

##
# Install the yang source files
##
rift_install_yang_artifacts(
  COMPONENT baseyang-1.0
  YANG_FILES ${source_yang_files}
)

rift_make_pc_ext(ncx
  VERSION 2.2.5
  INCLUDEDIR include/yuma/ncx
  DESC Yuma123 libncx
  LIBS -lncx
  COMPONENT yuma123)
