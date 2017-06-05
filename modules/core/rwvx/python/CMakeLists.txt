
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_python)

rift_python_install_tree(FILES
    rift/vx/__init__.py
    rift/vx/component.py
    COMPONENT ${INSTALL_COMPONENT})

rift_py3test(utest_python_component
  TEST_ARGS utest_component.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
  DEPENDS rw_base_yang
  )
