#!/usr/bin/env python
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Paul Laidler
# Creation Date: 2016/01/04
#

import pytest
import rift.vcs.vcs
import time

import gi
gi.require_version('RwMcYang', '1.0')
from gi.repository import RwMcYang

@pytest.fixture(scope='module')
def rwnsr_proxy(launchpad_session):
    return launchpad_session.proxy(RwNsrYang)

def test_launchpad_longevity(launchpad_session, mgmt_session, mgmt_domain_name, rwnsr_proxy):
    time.sleep(60)

    rift.vcs.vcs.wait_until_system_started(mgmt_session)

    launchpad_state = mgmt_session.proxy(RwMcYang).get("/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name)
    assert launchpad_state == 'started'

    nsr_opdata = rwnsr_proxy.get('/ns-instance-opdata')
    for nsr in nsr_opdata.nsr:
        xpath = ("/ns-instance-opdata"
                 "/nsr[ns-instance-config-ref='%s']"
                 "/operational-status") % (nsr.ns_instance_config_ref)
        operational_status = rwnsr_proxy.get(xpath)
        assert operational_status == 'running'

