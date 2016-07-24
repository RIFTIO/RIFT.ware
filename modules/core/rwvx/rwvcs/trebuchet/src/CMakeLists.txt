
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

add_definitions(-std=gnu99)

rift_pkg_check_modules(CURL REQUIRED libcurl>=7.35.0)
rift_pkg_check_modules(JSON REQUIRED json-c>=0.11)

include_directories(BEFORE
  ${CURL_INCLUDE_DIRS}
  ${JSON_INCLUDE_DIRS}
)

add_library(trebuchet
  trebuchet.c)
target_link_libraries(trebuchet
  ${CURL_LIBRARIES}
  ${JSON_LIBRARIES}
)

_install(TARGETS trebuchet
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT rwvcs-1.0)

