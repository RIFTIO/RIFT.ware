#!/usr/bin/env python3
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file test_startstop.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 12/17/2015
@brief System test of launchpad start and stop functionality
"""

import pytest

import gi
gi.require_version('RwMcYang', '1.0')
from gi.repository import RwMcYang

@pytest.fixture(scope='module')
def proxy(request, mgmt_session):
    '''fixture which returns a proxy to RwMcYang

    Arguments:
        request       - pytest fixture request
        mgmt_session  - mgmt_session fixture - instance of a rift.auto.session class
    '''
    return mgmt_session.proxy(RwMcYang)

@pytest.mark.depends('launchpad')
@pytest.mark.incremental
class TestLaunchpadStartStop:

    @pytest.mark.feature('mission-control')
    def test_stop_launchpad(self, proxy, mgmt_domain_name):
        '''Invoke stop launchpad RPC

        Asserts:
            Launchpad begins test in state 'started'
            Launchpad finishes test in state 'stopped'
        '''

        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=10,
                fail_on=['crashed'])
        stop_launchpad_input = RwMcYang.StopLaunchpadInput(mgmt_domain=mgmt_domain_name)
        stop_launchpad_output = proxy.rpc(stop_launchpad_input)
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'stopped',
                timeout=120,
                fail_on=['crashed'])

    @pytest.mark.feature('mission-control')
    def test_start_launchpad(self, proxy, mgmt_domain_name, launchpad_scraper):
        '''Invoke start launchpad RPC

        Asserts:
            Launchpad begins test in state 'stopped'
            Launchpad finishes test in state 'started'
        '''
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'stopped',
                timeout=10,
                fail_on=['crashed'])
        start_launchpad_input = RwMcYang.StartLaunchpadInput(mgmt_domain=mgmt_domain_name)
        start_launchpad_output = proxy.rpc(start_launchpad_input)
        proxy.wait_for(
                "/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name,
                'started',
                timeout=400,
                fail_on=['crashed'])
        launchpad_scraper.reset()

