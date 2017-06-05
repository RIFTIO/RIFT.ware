
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_python)

rift_python_install_tree(FILES
    rift/vcs/__init__.py
    rift/vcs/api.py
    rift/vcs/core.py
    rift/vcs/demo.py
    rift/vcs/ext.py
    rift/vcs/fastpath.py
    rift/vcs/manifest.py
    rift/vcs/mano.py
    rift/vcs/mgmt.py
    rift/vcs/logger.py
    rift/vcs/pool.py
    rift/vcs/port.py
    rift/vcs/procs.py
    rift/vcs/tasklets.py
    rift/vcs/vcs.py
    rift/vcs/vms.py
    rift/vcs/mano_vms.py
    rift/vcs/yang_model.py
    rift/vcs/component.py
    COMPONENT ${INSTALL_COMPONENT})

rift_python_install_tree(FILES
    rift/vcs/compiler/__init__.py
    rift/vcs/compiler/compiler.py
    rift/vcs/compiler/constraints.py
    rift/vcs/compiler/constructors.py
    rift/vcs/compiler/exc.py
    rift/vcs/compiler/transforms.py
    COMPONENT ${INSTALL_COMPONENT})

rift_py3test(utest_python_vcs
  TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/setup.py test
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS log_test_yang
  )

rift_add_subdirs(SUBDIR_LIST plugins)

cmake_minimum_required(VERSION 2.8)
install(FILES etc/syslog_template.conf DESTINATION etc COMPONENT ${INSTALL_COMPONENT})

install(
  PROGRAMS
    basic_systest
  DESTINATION usr/rift/systemtest/vcs
  COMPONENT ${INSTALL_COMPONENT})

install(
  PROGRAMS
    pytest/basic/basic.py
  DESTINATION usr/rift/systemtest/pytest/rift/basic
  COMPONENT ${INSTALL_COMPONENT})

install(
  FILES
    racfg/basic.racfg
    racfg/basic_xml.racfg
  DESTINATION usr/rift/systemtest/vcs
  COMPONENT ${INSTALL_COMPONENT})
