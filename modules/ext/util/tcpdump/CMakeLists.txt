include(rift_externalproject)

rift_externalproject_add(
  tcpdump
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tcpdump
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr
)


# This is an external project, don't symlink stuff for
# consistency.
_install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tcpdump/tcpdump-build/libtcpdump.a
  DESTINATION usr/lib/ COMPONENT tcpdump)

_install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpdump/interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpdump/netdissect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpdump/tcpdump_export.h
  DESTINATION usr/include/tcpdump COMPONENT tcpdump)
