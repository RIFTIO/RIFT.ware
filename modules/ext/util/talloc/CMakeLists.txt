include(rift_externalproject)

rift_externalproject_add(
  talloc-ext
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/talloc-2.1.0
  CONFIGURE_COMMAND
    ./configure --disable-python --enable-debug  --prefix=${CMAKE_INSTALL_PREFIX}/usr
)

rift_add_subdirs(SUBDIR_LIST examples)
