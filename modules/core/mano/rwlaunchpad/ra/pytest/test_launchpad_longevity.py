#!/usr/bin/env python
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Paul Laidler
# Creation Date: 2016/01/04
#

import rift.vcs.vcs
import time
import gi
gi.require_version('RwMcYang', '1.0')
from gi.repository import RwMcYang

def test_launchpad_longevity(mgmt_session, mgmt_domain_name):
    time.sleep(60)
    rift.vcs.vcs.wait_until_system_started(mgmt_session)
    launchpad_state = mgmt_session.proxy(RwMcYang).get("/mgmt-domain/domain[name='%s']/launchpad/state" % mgmt_domain_name)
    assert launchpad_state == 'started'

