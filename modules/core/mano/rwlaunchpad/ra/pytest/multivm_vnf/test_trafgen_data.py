#!/usr/bin/env python
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_trafgen_data.py
@author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
@date 03/16/2016
@brief Scriptable load-balancer test with multi-vm VNFs
"""

import ipaddress
import pytest
import re
import subprocess
import time

import rift.auto.session

from gi.repository import (
    RwTrafgenYang,
    RwTrafgenDataYang,
    RwVnfBaseOpdataYang,
    RwVnfBaseConfigYang,
    RwTrafgenYang
)


@pytest.fixture(scope='session')
def trafgen_vnfr(request, rwvnfr_proxy, session_type):
    vnfr = "/vnfr-catalog/vnfr"
    vnfrs = rwvnfr_proxy.get(vnfr, list_obj=True)
    for vnfr in vnfrs.vnfr:
        if 'trafgen' in vnfr.short_name:
            return vnfr
    assert False, "Not found the VNFR with name 'trafgen'"

@pytest.fixture(scope='session')
def trafgen_session(request, trafgen_vnfr, session_type):
    trafgen_host = trafgen_vnfr.vnf_configuration.config_access.mgmt_ip_address
    if session_type == 'netconf':
        tg_session = rift.auto.session.NetconfSession(host=trafgen_host)
    elif session_type == 'restconf':
        tg_session = rift.auto.session.RestconfSession(host=trafgen_host)

    tg_session.connect()
    rift.vcs.vcs.wait_until_system_started(tg_session, 900)
    return tg_session

@pytest.fixture(scope='session')
def trafgen_ports(request, trafgen_vnfr, session_type):
    return [cp.name for cp in trafgen_vnfr.connection_point]

@pytest.fixture(scope='module')
def tgdata_proxy(trafgen_session):
    '''fixture that returns a proxy to RwTrafgenDataYang'''
    return trafgen_session.proxy(RwTrafgenDataYang)


@pytest.fixture(scope='module')
def tgcfg_proxy(trafgen_session):
    '''fixture that returns a proxy to RwTrafgenYang'''
    return trafgen_session.proxy(RwTrafgenYang)


@pytest.fixture(scope='module')
def vnfdata_proxy(trafgen_session):
    '''fixture that returns a proxy to RwVnfBaseOpdataYang'''
    return trafgen_session.proxy(RwVnfBaseOpdataYang)


@pytest.fixture(scope='module')
def vnfcfg_proxy(trafgen_session):
    '''fixture that returns a proxy to RwVnfBaseConfigYang'''
    return trafgen_session.proxy(RwVnfBaseConfigYang)


def confirm_config(tgcfg_proxy, vnf_name):
    '''To ensure the configuration is present for the given VNF

    Arguments:
        vnf_name - vnf name of configuration
    '''
    xpath = "/vnf-config/vnf[name='%s'][instance='0']" % vnf_name
    for _ in range(24):
        tg_config = tgcfg_proxy.get_config(xpath)
        if tg_config is not None:
            break
        time.sleep(10)
    else:
        assert False, "Configuration check timeout"


def start_traffic(tgdata_proxy, tgcfg_proxy, port_name):
    '''Start traffic on the port with the specified name.

    Arguments:
        port_name - name of port on which to start traffic
    '''
    confirm_config(tgcfg_proxy, 'trafgen')
    rpc_input = RwTrafgenDataYang.RwStartTrafgenTraffic.from_dict({
        'vnf_name':'trafgen',
        'vnf_instance':0,
        'port_name':port_name
    })
    rpc_output = RwVnfBaseOpdataYang.YangOutput_RwVnfBaseOpdata_Start_VnfOutput()
    tgdata_proxy.rpc(rpc_input, rpc_name='start', output_obj=rpc_output)


def stop_traffic(tgdata_proxy, port_name):
    '''Stop traffic on the port with the specified name.

    Arguments:
        port_name - name of port on which to stop traffic
    '''
    rpc_input = RwTrafgenDataYang.RwStopTrafgenTraffic.from_dict({
        'vnf_name':'trafgen',
        'vnf_instance':0,
        'port_name':port_name
    })
    rpc_output = RwVnfBaseOpdataYang.YangOutput_RwVnfBaseOpdata_Stop_VnfOutput()
    tgdata_proxy.rpc(rpc_input, rpc_name='stop', output_obj=rpc_output)


def wait_for_traffic_started(vnfdata_proxy, vnf_name, port_name, timeout=120, interval=2, threshold=60):
    '''Wait for traffic to be started on the specified port

    Traffic is determined to be started if the input/output packets on the port
    increment during the specified interval

    Arguments:
        port_name - name of the port being monitored
        timeout - time allowed for traffic to start
        interval - interval at which the counters should be checked
        threhsold - values under the threshold treated as 0
    '''
    def value_incremented(previous_sample, current_sample):
        '''Comparison that returns True if the the sampled counter increased
        beyond the specified threshold during the sampling interval
        otherwise returns false
        '''
        return (int(current_sample) - int(previous_sample)) > threshold

    xpath = "/vnf-opdata/vnf[name='{}'][instance='0']/port-state[portname='{}']/counters/{}"
    vnfdata_proxy.wait_for_interval(xpath.format(vnf_name, port_name, 'input-packets'),
                                    value_incremented, timeout=timeout, interval=interval)


def wait_for_traffic_stopped(vnfdata_proxy, vnf_name, port_name, timeout=60, interval=2, threshold=60):
    '''Wait for traffic to be stopped on the specified port

    Traffic is determined to be stopped if the input/output packets on the port
    remain unchanged during the specified interval

    Arguments:
        port_name - name of the port being monitored
        timeout - time allowed for traffic to start
        interval - interval at which the counters should be checked
        threshold - values under the threshold treated as 0
    '''
    def value_unchanged(previous_sample, current_sample):
        '''Comparison that returns True if the the sampled counter increased
        less than the specified threshold during the sampling interval
        otherwise returns False
        '''
        return (int(current_sample) - int(previous_sample)) < threshold

    xpath = "/vnf-opdata/vnf[name='{}'][instance='0']/port-state[portname='{}']/counters/{}"
    vnfdata_proxy.wait_for_interval(xpath.format(vnf_name, port_name, 'input-packets'), value_unchanged, timeout=timeout, interval=interval)

@pytest.mark.depends('multivmvnf')
@pytest.mark.incremental
class TestMVVSlbDataFlow:

    def test_start_stop_traffic(self, vnfdata_proxy, tgdata_proxy, tgcfg_proxy, trafgen_ports):
        ''' This test verfies that traffic can be stopped and started on
        all trafgen ports.

        Arguments:
            vnfdata_proxy - proxy to retrieve vnf operational data
            tgdata_proxy - proxy to retrieve trafgen operational data
            tgcfg_proxy - proxy to retrieve trafgen configuration
            trafgen_ports - list of port names on which traffic can be started
        '''
        time.sleep(300)
        for port in trafgen_ports:
            start_traffic(tgdata_proxy, tgcfg_proxy, port)
            wait_for_traffic_started(vnfdata_proxy, 'trafgen', port)
            stop_traffic(tgdata_proxy, port)
            wait_for_traffic_stopped(vnfdata_proxy, 'trafgen',  port)


    def test_start_traffic(self, vnfdata_proxy, tgdata_proxy, tgcfg_proxy, trafgen_ports):
        ''' This test starts traffic on all trafgen ports in preperation for
        subsequent tests

        Arguments:
            vnfdata_proxy - proxy to retrieve vnf operational data
            tgdata_proxy - proxy to retrieve trafgen operational data
            tgcfg_proxy - proxy to retrieve trafgen configuration
            trafgen_ports - list of port names on which traffic can be started
        '''
        for port in trafgen_ports:
            start_traffic(tgdata_proxy, tgcfg_proxy, port)
            wait_for_traffic_started(vnfdata_proxy, 'trafgen', port)
