
# STANDARD_RIFT_IO_COPYRIGHT

remove_definitions(-std=gnu99)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../)

rift_gtest(reaper_unittest
  TEST_SRCS test_reaper.cpp
  TEST_LIBS
    reaper
    reaper_client
    pthread
)


