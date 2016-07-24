
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_python)

rift_python_install_tree(FILES
    rift/vx/__init__.py
    rift/vx/component.py
    COMPONENT rwvx-1.0)

rift_pytest(utest_python_component
  TEST_ARGS utest_component.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
  DEPENDS rw_base_yang
  )
