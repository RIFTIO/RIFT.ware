#!/usr/bin/env python3
import logging

import rift.auto.session
import rift.vcs.vcs
import gi
gi.require_version('RwlogMgmtYang', '1.0')

from gi.repository import (
        RwlogMgmtYang,
        )

logging.basicConfig(level=logging.DEBUG)
mgmt_session = rift.auto.session.NetconfSession(host="127.0.0.1")
mgmt_session.connect()
rift.vcs.vcs.wait_until_system_started(mgmt_session)
log_mgmt_proxy = mgmt_session.proxy(RwlogMgmtYang)

log_cfg_msg = RwlogMgmtYang.Logging.from_dict({
    "sink": [
        {
            "name": "logtest_mano",
            "filename": "logtest_mano",
            "filter": {
                "category": [{
                        "name": "rw-mano-log",
                        "severity": "debug",
                        "attribute": [{
                            "name": "category",
                            "value": "vnfm",
                            }]
                        }],
                }
        },
        {
            "name": "logtest_restconf",
            "filename": "logtest_restconf",
            "filter": {
                "category": [{
                        "name": "rw-restconf-log",
                        "severity": "debug"
                        }],
                }
        },
        {
            "name": "logtest_resmgr",
            "filename": "logtest_resmgr",
            "filter": {
                "category": [{
                        "name": "rw-resource-mgr-log",
                        "severity": "debug"
                        }],
                }
        },
        {
            "name": "logtest_cal",
            "filename": "logtest_cal",
            "filter": {
                "category": [{
                        "name": "rw-cal-log",
                        "severity": "debug"
                        }],
                }
        },
        {
            "name": "logtest_conman",
            "filename": "logtest_conman",
            "filter": {
                "category": [{
                        "name": "rw-conman-log",
                        "severity": "debug"
                        }],
                }
        },
        ]
    })

log_mgmt_proxy.merge_config("/rwlog-mgmt:logging", log_cfg_msg)
