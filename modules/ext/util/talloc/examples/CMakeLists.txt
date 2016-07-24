add_executable(talloc_test talloc_test.c)
target_link_libraries(talloc_test talloc)
add_dependencies(talloc_test talloc-ext)

