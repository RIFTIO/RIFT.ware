include(rift_externalproject)

set(CFLAGS "-I${CMAKE_INSTALL_PREFIX}/usr/include/")
set(CXXFLAGS "-I${CMAKE_INSTALL_PREFIX}/usr/include/")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CFLAGS "${CFLAGS} -g -O0")
  set(CXXFLAGS "${CXXFLAGS} -g -O0")
endif()

rift_externalproject_add(
  gperftools
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gperftools-2.4
  CONFIGURE_COMMAND
      ./configure
        --prefix=${CMAKE_INSTALL_PREFIX}/usr
        --enable-libunwind
        CFLAGS=${CFLAGS}
        CXXFLAGS=${CXXFLAGS}
        LDFLAGS=-L${CMAKE_INSTALL_PREFIX}/usr/lib
)
