# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Paul Laidler
# Creation Date: 09/16/2015
# 

cmake_minimum_required(VERSION 2.8)

install(
  PROGRAMS
    mission_control_systest
    mission_control_delete_systest
    mission_control_negative_systest
    mission_control_negative_cloud_account_systest
    mission_control_negative_mgmt_domain_systest
    mission_control_negative_vmpool_systest
    mission_control_reload_systest
  DESTINATION usr/rift/systemtest/mission_control
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    pytest/conftest.py
    pytest/test_mission_control.py
    pytest/test_mission_control_delete.py
    pytest/test_mission_control_negative.py
    pytest/test_mission_control_negative_cloud_account.py
    pytest/test_mission_control_negative_mgmt_domain.py
    pytest/test_mission_control_negative_vmpool.py
  DESTINATION usr/rift/systemtest/pytest/mission_control
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    racfg/mission_control_systest_cloudsim.racfg
    racfg/mission_control_systest_openstack.racfg
    racfg/mission_control_delete_systest_cloudsim.racfg
    racfg/mission_control_reload_systest_openstack.racfg
    racfg/mission_control_reload_systest_openstack_xml.racfg
    racfg/mission_control_systest_cloudsim_negative.racfg
    racfg/mission_control_systest_cloudsim_negative_xml.racfg
    racfg/mission_control_systest_openstack_negative.racfg
    racfg/multi_tenant_systest_openstack.racfg
  DESTINATION
    usr/rift/systemtest/mission_control
    COMPONENT ${PKG_LONG_NAME})

