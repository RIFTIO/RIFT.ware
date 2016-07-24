# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Paul Laidler
# Creation Date: 09/16/2015
# 

cmake_minimum_required(VERSION 2.8)

install(
  PROGRAMS
    pingpong_longevity_systest
    pingpong_vnf_systest
    pingpong_records_systest
    pingpong_vnf_reload_systest
    pingpong_lp_standalone_systest
    pingpong_recovery_systest
    scaling_systest
  DESTINATION usr/rift/systemtest/pingpong_vnf
  COMPONENT ${PKG_LONG_NAME})

install(
  PROGRAMS
    multi_vm_vnf_slb_systest.sh
    multi_vm_vnf_trafgen_systest.sh
  DESTINATION usr/rift/systemtest/multi_vm_vnf
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    pytest/multivm_vnf/conftest.py
    pytest/multivm_vnf/test_multi_vm_vnf_slb.py
    pytest/multivm_vnf/test_multi_vm_vnf_trafgen.py
    pytest/multivm_vnf/test_trafgen_data.py
  DESTINATION usr/rift/systemtest/pytest/multi_vm_vnf
  COMPONENT ${PKG_LONG_NAME})

install(
  PROGRAMS
    launchpad_longevity_systest
  DESTINATION usr/rift/systemtest/launchpad
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    pytest/test_launchpad_longevity.py
    pytest/test_startstop.py
  DESTINATION usr/rift/systemtest/pytest/mission_control/launchpad
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    pytest/conftest.py
    pytest/test_pingpong_longevity.py
    pytest/test_onboard_vnf.py
    pytest/test_records.py
    pytest/test_scaling.py
  DESTINATION usr/rift/systemtest/pytest/mission_control/pingpong_vnf
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    racfg/pingpong_vnf_systest_cloudsim.racfg
    racfg/pingpong_vnf_systest_openstack.racfg
    racfg/pingpong_records_systest_openstack.racfg
    racfg/pingpong_records_systest_openstack_xml.racfg
    racfg/pingpong_vnf_reload_systest_openstack.racfg
    racfg/pingpong_vnf_reload_systest_openstack_xml.racfg
    racfg/pingpong_lp_standalone_systest_openstack.racfg
    racfg/pingpong_lp_standalone_systest_openstack_xml.racfg
    racfg/scaling_systest.racfg
    racfg/recovery_systest.racfg
  DESTINATION usr/rift/systemtest/pingpong_vnf
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    racfg/multivm_vnf_slb_systest.racfg
    racfg/multivm_vnf_trafgen_systest.racfg
  DESTINATION usr/rift/systemtest/multi_vm_vnf
  COMPONENT ${PKG_LONG_NAME})
