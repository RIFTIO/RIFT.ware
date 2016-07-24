cmake_minimum_required(VERSION 2.8)

add_executable(dispatch_test dispatch_test.c)
target_link_libraries(dispatch_test dispatch)

add_dependencies(dispatch_test libdispatch)
