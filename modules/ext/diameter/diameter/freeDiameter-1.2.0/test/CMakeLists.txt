# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 8/5/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME diameter_gtest)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

rift_allow_compiler_warning(literal-suffix)
rift_allow_compiler_warning(write-strings)


#############################
# Some parameters for the tests

ADD_DEFINITIONS(-DTEST_DEBUG)
ADD_DEFINITIONS(-DBUILD_DIR="${CMAKE_BINARY_DIR}")

INCLUDE_DIRECTORIES( "../libfdproto" )
INCLUDE_DIRECTORIES( "../libfdcore" )
INCLUDE_DIRECTORIES(${LFDCORE_INCLUDES})


##
# Converted unit test target
##
rift_gtest(diameter_gtest
  TEST_SRCS testdict.cpp testcnx.cpp testmesg.cpp testostr.cpp testpeers.cpp testsctp.cpp testsess.cpp testdisp.cpp testfifo.cpp
  TEST_LIBS libfdcore libfdproto ${GNUTLS_LIBRARIES} ${GCRYPT_LIBRARY}
  DEPENDS ${diameter_deps}
)

