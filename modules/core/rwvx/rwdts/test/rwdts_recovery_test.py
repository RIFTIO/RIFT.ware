#!/usr/bin/env python3

# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

import argparse
import asyncio
import logging
import os
import sys
import unittest
import re
import psutil
import types

import xmlrunner

import gi
gi.require_version('RwDtsToyTaskletYang', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwVcsYang', '1.0')

import gi.repository.RwManifestYang as rwmanifest
import gi.repository.RwVcsYang as rwvcs
import gi.repository.RwDtsToyTaskletYang as toyyang
import gi.repository.RwYang as RwYang
import rift.auto.session
import rift.vcs.vcs

import rift.tasklets
import rift.test.dts

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async

class DtsPerf(rift.test.dts.AbstractDTSTest):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """
    @classmethod
    def configure_schema(cls):
        return toyyang.get_schema()

    def setUp(self):
        """
        1. Creates an asyncio loop
        2. Triggers the hook configure_test
        """
        def scheduler_tick(self, *args):
            self.call_soon(self.stop)
            self.run_forever()

        # Init params: loop & timers
        self.loop = asyncio.new_event_loop()

        self.loop.scheduler_tick = types.MethodType(scheduler_tick, self.loop)

        self.asyncio_timer = None
        self.stop_timer = None
        self.__class__.id_cnt += 1
        self.configure_test(self.loop, self.__class__.id_cnt)

    @classmethod
    def configure_manifest(cls):
        manifest = rwmanifest.Manifest()
        manifest.bootstrap_phase = rwmanifest.BootstrapPhase.from_dict({
            'rwtrace' : {
                'enable' : True,
                'level' : 2
            },
            'log' : {
                 'enable' : True,
                 'severity' : 2,
                 'console_severity' : 2
            },
            'rwmgmt' : {
                'northbound_listing' : [ 'cli_rwfpath_schema_listing.txt' ]
            },
            'rwtasklet' : {
                'plugin_name' : 'rwinit-c',
            },
            'zookeeper' : {
                'master_ip' : '127.0.0.1' ,
                'unique_ports' : False,
                'zake' : True
            },
            'rwvm' : {
                'instances' : [{
                    'component_name' : 'msgbroker',
                    'config_ready' : True
                },
                {
                    'component_name' : 'dtsrouter',
                    'config_ready' : True
                }]
            }
        })
        manifest.init_phase = rwmanifest.InitPhase.from_dict({
            'environment' : {
                'python_variable' : [
                    "rw_component_name = 'rwdtsperf-c-vm'",
                    "component_type = 'rwvm'",
                    "instance_id = 729"
                ],
                'component_name' : '$python(rw_component_name)',
                'component_type' : '$python(component_type)',
                'instance_id' : '$python(instance_id)'
            },
            'settings' : {
                'rwmsg' : {
                    'multi_broker' : {
                        'enable' : True
                    }
                },
                'rwdtsrouter' : {
                    'multi_dtsrouter' : {
                        'enable' : True
                    }
                },
                'rwvcs' : {
                    'collapse_each_rwvm' : True,
                    'collapse_each_rwprocess' : True
                }
            }
        })
        manifest.inventory = rwmanifest.Inventory.from_dict({
           'component' : [{
              'component_name': 'rwdtsperf-c-collection',
              'component_type': 'RWCOLLECTION',
              'rwcollection': {
                  'collection_type' : 'rwcolony',
                  'event_list' : {
                      'event' : [{
                          'name' : 'onentry',
                          'action' : [{
                              'name' : 'Start rwdtsperf-c-vm',
                              'start' : {
                                  'python_variable' : [ "vm_ip_address = '127.0.0.1'" ],
                                  'component_name' : 'rwdtsperf-c-vm',
                                  'instance_id' : '729',
                                  'config_ready' : True
                              }
                          }]
                      }]
                  }
              }
           },
           {
              'component_name': 'rwdtsperf-c-vm',
              'component_type': 'RWVM',
              'rwvm': {
                  'leader' : True,
                  'event_list' : {
                      'event' : [{
                          'name' : 'onentry',
                          'action' : [{
                              'name' : 'Start rwdtsperf-c-collection',
                              'start' : {
                                  'component_name' : 'rwdtsperf-c-collection',
                                  'config_ready' : True
                              }
                          }]
                      }]
                  }
              }
           },
           {
              'component_name': 'rwdtsperf-c-proc',
              'component_type': 'RWPROC',
              'rwproc': {
                  'tasklet': [{
                      'name': 'Start rwdtsperf-c',
                      'component_name': 'rwdtsperf-c',
                      'config_ready' : True
                  }]
              }
           },
           {
              'component_name': 'rwdtsperf-c-proc-restart',
              'component_type': 'RWPROC',
              'rwproc': {
                  'tasklet': [{
                      'name': 'Start rwdtsperf-c',
                      'component_name': 'rwdtsperf-c',
                      'recovery_action' : 'RESTART'
                  }]
              }
           },
           {
              'component_name': 'RW.Proc_1.uAgent',
              'component_type': 'RWPROC',
              'rwproc': {
                  'tasklet': [{
                      'python_variable' : [ "cmdargs_str = '--confd-proto AF_INET --confd-ip 127.0.0.1'" ],
                      'name': 'Start RW.uAgent for RW.Proc_1.uAgent',
                      'component_name': 'RW.uAgent',
                      'config_ready' : True
                  }]
              }
           },
           {
              'component_name': 'rwdtsperf-c',
              'component_type': 'RWTASKLET',
              'rwtasklet': {
                  'plugin_directory': 'rwdtsperf-c',
                  'plugin_name': 'rwdtsperf-c'
              }
           },
           {
              'component_name': 'RW.uAgent',
              'component_type': 'RWTASKLET',
              'rwtasklet': {
                  'plugin_directory': 'rwuagent-c',
                  'plugin_name': 'rwuagent-c'
              }
           },
           {
              'component_name': 'msgbroker',
              'component_type': 'RWTASKLET',
              'rwtasklet': {
                  'plugin_directory': 'rwmsgbroker-c',
                  'plugin_name': 'rwmsgbroker-c'
              }
           },
           {
              'component_name': 'logd',
              'component_type': 'RWTASKLET',
              'rwtasklet': {
                  'plugin_directory': 'rwlogd-c',
                  'plugin_name': 'rwlogd-c'
              }
           },
           {
              'component_name': 'dtsrouter',
              'component_type': 'RWTASKLET',
              'rwtasklet': {
                  'plugin_directory': 'rwdtsrouter-c',
                  'plugin_name': 'rwdtsrouter-c'
              }
           }]
        })
        return manifest

    def tearDown(self):
        self.loop.stop()
        self.loop.close()


class Trafgen(rift.test.dts.AbstractDTSTest):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """
    @classmethod
    def configure_schema(cls):
        schema = RwYang.Model.load_and_merge_schema(
                     rwvcs.get_schema(),
                     'librwcal_yang_gen.so', 
                     'Rwcal')
        cls.model = RwYang.Model.create_libncx()
        cls.model.load_schema_ypbc(schema)
        xml = cls.manifest.to_xml_v2(cls.model, 1)
        xml = re.sub('rw-manifest:', '', xml)
        xml = re.sub('<manifest xmlns:rw-manifest="http://riftio.com/ns/riftware-1.0/rw-manifest">', '<?xml version="1.0" ?>\n<manifest xmlns="http://riftio.com/ns/riftware-1.0/rw-manifest" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://riftio.com/ns/riftware-1.0/rw-manifest ./rw-manifest.xsd">', xml)
        xml = '\n'.join(xml.split('\n')[1:])
        with open('lptestmanifest.xml', 'w') as f:
           f.write(str(xml))
        f.close()
        return schema

    def setUp(self):
        """
        1. Creates an asyncio loop
        2. Triggers the hook configure_test
        """
        def scheduler_tick(self, *args):
            self.call_soon(self.stop)
            self.run_forever()

        # Init params: loop & timers
        self.loop = asyncio.new_event_loop()

        self.loop.scheduler_tick = types.MethodType(scheduler_tick, self.loop)

        self.asyncio_timer = None
        self.stop_timer = None
        self.__class__.id_cnt += 1
        self.configure_test(self.loop, self.__class__.id_cnt)


    @classmethod
    def configure_manifest(cls):
        manifest = rwmanifest.Manifest()
        manifest.bootstrap_phase = rwmanifest.BootstrapPhase.from_dict({
            'rwtrace' : {
                'enable' : True,
                'level' : 5
            },
            'log' : {
                 'enable' : True,
                 'severity' : 5,
                 'console_severity' : 5
            },
            'rwmgmt' : {
                'northbound_listing' : [ 'cli_rwfpath_schema_listing.txt' ]
            },
            'rwtasklet' : {
                'plugin_name' : 'rwinit-c',
            },
            'zookeeper' : {
                'master_ip' : '127.0.0.1' ,
                'unique_ports' : False,
                'zake' : True
            },
            'rwvm' : {
                'instances' : [{
                    'component_name' : 'msgbroker',
                    'config_ready' : True
                },
                {
                    'component_name' : 'dtsrouter',
                    'config_ready' : True
                }]
            },
            'serf': {
                'start': True
            },
#           'rwsecurity': {
#               'use_ssl': True,
#               'cert': '/net/mahi/localdisk/kelayath/ws/rift/etc/ssl/current.cert',
#               'key': '/net/mahi/localdisk/kelayath/ws/rift/etc/ssl/current.key'
#           }
        })
        manifest.init_phase = rwmanifest.InitPhase.from_dict({
            'environment' : {
                'python_variable' : [
                    "rw_component_name = 'RW_VM_MASTER'",
                    "component_type = 'rwvm'",
                    "instance_id = 9"
                ],
                'component_name' : '$python(rw_component_name)',
                'component_type' : '$python(component_type)',
                'instance_id' : '$python(instance_id)'
            },
            'settings' : {
                'rwmsg' : {
                    'multi_broker' : {
                        'enable' : True
                    }
                },
                'rwdtsrouter' : {
                    'multi_dtsrouter' : {
                        'enable' : True
                    }
                },
                'rwvcs' : {
                    'collapse_each_rwvm' : True,
                    'collapse_each_rwprocess' : True
                }
            }
        })
        manifest.inventory = rwmanifest.Inventory.from_dict({
           'component' : [{
               'component_name': 'rw.colony',
               'component_type': 'RWCOLLECTION',
               'rwcollection': {
                   'collection_type': 'rwcolony',
                   'event_list': {
                       "event": [{
                           'name': 'onentry',
                           'action': [
                               {
                                   'name': 'Start trafgen for rw.colony',
                                   'start': {
                                       'component_name': 'trafgen',
                                       'instance_id': '7',
                                       'config_ready': True
                                   }
                               },
                               {
                                   "name": "Start trafsink for rw.colony",
                                   "start": {
                                       "component_name": "trafsink",
                                       "instance_id": "8",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start RW_VM_MASTER for rw.colony",
                                   "start": {
                                       "python_variable": [ "vm_ip_address = '127.0.0.1'" ],
                                       "component_name": "RW_VM_MASTER",
                                       "instance_id": "9",
                                       "config_ready": True
                                   }
                               }
                           ]
                       }]
                   }
               }
           },


           {
               "component_name": "trafgen",
               "component_type": "RWCOLLECTION",
               "rwcollection": {
                   "collection_type": "rwcluster",
                   "event_list": {
                       "event": [{
                           "name": "onentry",
                           "action": [
                               {
                                   "name": "Start RW.VM.TG.LEAD for trafgen",
                                   "start": {
                                       "python_variable": [ "vm_ip_address = '127.0.0.2'" ],
                                       "component_name": "RW.VM.TG.LEAD",
                                       "instance_id": "10",
                                       "config_ready": True
                                   }
                               } 
                              ,{
                                   "name": "Start RW.VM.trafgen for trafgen",
                                   "start": {
                                       "python_variable": [ "vm_ip_address = '127.0.0.3'" ],
                                       "component_name": "RW.VM.trafgen",
                                       "instance_id": "11",
                                       "config_ready": True
                                   }
                               }
                           ]
                       }]
                   }
               }
           },

           {
               "component_name": "RW.VM.TG.LEAD",
               "component_type": "RWVM",
               "rwvm": {
                   "leader": True,
                   "event_list": {
                       "event": [{
                           "name": "onentry",
                           "action": [
                               {
                                   "name": "Start the RW.Proc_4.Fpath~58003",
                                   "start": {
                                       "component_name": "RW.Proc_4.Fpath~58003",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start the RW.Proc_5.NcMgr",
                                   "start": {
                                       "component_name": "RW.Proc_5.NcMgr",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start the RW.Proc_6.FpCtrl",
                                   "start": {
                                       "component_name": "RW.Proc_6.FpCtrl",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start the RW.Proc_7.IfMgr",
                                   "start": {
                                       "component_name": "RW.Proc_7.IfMgr",
                                       "config_ready": True
                                   }
                               }
                           ]
                       }]
                   }
               }
           },


           {
               "component_name": "RW.Proc_4.Fpath~58003",
               "component_type": "RWPROC",
               "rwproc": {
                   "run_as": "root",
                   "tasklet": [{
                       "name": "Start RW.Fpath for RW.Proc_4.Fpath~58003",
                       "component_name": "RW.Fpath",
                       "instance_id": 1,
                       "config_ready": True,
                       "python_variable": [
                           "colony_name = 'trafgen'",
                           "colony_id = 7",
                           "port_map = 'vfap/1/1|eth_sim:name=fabric1|vfap'",
                           "cmdargs_str = ''"
                       ]
                   }]
               }
           },

           {
               "component_name": "RW.Fpath",
               "component_type": "RWTASKLET",
               "rwtasklet": {
                   "plugin_directory": "./usr/lib/rift/plugins/rwfpath-c",
                   "plugin_name": "rwfpath-c"
               }
           },


           {
               "component_name": "RW.Proc_5.NcMgr",
               "component_type": "RWPROC",
               "rwproc": {
                   "run_as": "root",
                   "tasklet": [{
                       "name": "Start RW.NcMgr for RW.Proc_5.NcMgr",
                       "component_name": "RW.NcMgr",
                       "config_ready": True,
                       "python_variable": [
                           "cmdargs_str = ''",
                           "colony_name = 'trafgen'",
                           "colony_id = 7"
                       ]
                   }]
               }
           },
           {
               "component_name": "RW.NcMgr",
               "component_type": "RWTASKLET",
               "rwtasklet": {
                   "plugin_directory": "./usr/lib/rift/plugins/rwncmgr-c",
                   "plugin_name": "rwncmgr-c"
               }
           },


           {
               "component_name": "RW.Proc_6.FpCtrl",
               "component_type": "RWPROC",
               "rwproc": {
                   "tasklet": [
                       {
                           "name": "Start RW.FpCtrl for RW.Proc_6.FpCtrl",
                           "component_name": "RW.FpCtrl",
                           "config_ready": True,
                           "python_variable": [
                               "cmdargs_str = ''",
                               "colony_name = 'trafgen'",
                               "colony_id = 7"
                           ]
                       },
                       {
                           "name": "Start RW.NNLatencyTasklet for RW.Proc_6.FpCtrl",
                           "component_name": "RW.NNLatencyTasklet",
                           "config_ready": True
                       },
                       {
                           "name": "Start RW.SfMgr for RW.Proc_6.FpCtrl",
                           "component_name": "RW.SfMgr",
                           "config_ready": True
                       },
                       {
                           "name": "Start RW.SffMgr for RW.Proc_6.FpCtrl",
                           "component_name": "RW.SffMgr",
                           "config_ready": True
                       }
                   ]
               }
           },

           {
               "component_name": "RW.FpCtrl",
               "component_type": "RWTASKLET",
               "rwtasklet": {
                   "plugin_directory": "./usr/lib/rift/plugins/rwfpctrl-c",
                   "plugin_name": "rwfpctrl-c"
               }
           },
           {
               "component_name": "RW.NNLatencyTasklet",
               "component_type": "RWTASKLET",
               "rwtasklet": {
                   "plugin_directory": "./usr/lib/rift/plugins/rwnnlatencytasklet",
                   "plugin_name": "rwnnlatencytasklet"
               }
           },
           {
               "component_name": "RW.SfMgr",
               "component_type": "RWTASKLET",
               "rwtasklet": {
                   "plugin_directory": "./usr/lib/rift/plugins/sfmgr",
                   "plugin_name": "rwsfmgr"
               }
           },
           {
               "component_name": "RW.SffMgr",
               "component_type": "RWTASKLET",
               "rwtasklet": {
                   "plugin_directory": "./usr/lib/rift/plugins/sffmgr",
                   "plugin_name": "rwsffmgr"
               }
           },
           {
               "component_name": "RW.Proc_7.IfMgr",
               "component_type": "RWPROC",
               "rwproc": {
                   "tasklet": [{
                       "name": "Start RW.IfMgr for RW.Proc_7.IfMgr",
                       "component_name": "RW.IfMgr",
                       "config_ready": True,
                       "python_variable": [
                           "cmdargs_str = ''",
                           "colony_name = 'trafgen'",
                           "colony_id = 7"
                       ]
                   }]
               }
           },
           {
               "component_name": "RW.IfMgr",
               "component_type": "RWTASKLET",
               "rwtasklet": {
                   "plugin_directory": "./usr/lib/rift/plugins/rwifmgr-c",
                   "plugin_name": "rwifmgr-c"
               }
           },
           {
               "component_name": "RW.VM.trafgen",
               "component_type": "RWVM",
               "rwvm": {
                   "event_list": {
                       "event": [{
                           "name": "onentry",
                           "action": [{
                               "name": "Start the RW.Proc_8.Fpath~94220",
                               "start": {
                                   "component_name": "RW.Proc_8.Fpath~94220",
                                   "config_ready": True
                               }
                           }]
                       }]
                   }
               }
           },

           {
               "component_name": "RW.Proc_8.Fpath~94220",
               "component_type": "RWPROC",
               "rwproc": {
                   "run_as": "root",
                   "tasklet": [{
                       "name": "Start RW.Fpath for RW.Proc_8.Fpath~94220",
                       "component_name": "RW.Fpath",
                       "instance_id": 2,
                       "config_ready": True,
                       "python_variable": [
                           "colony_name = 'trafgen'",
                           "colony_id = 7",
                           "port_map = 'vfap/2/1|eth_sim:name=fabric1|vfap,trafgen/2/1|eth_sim:name=trafgenport|external'",
                           "cmdargs_str = ''"
                       ]
                   }]
               }
           },

           {
               "component_name": "trafsink",
               "component_type": "RWCOLLECTION",
               "rwcollection": {
                   "collection_type": "rwcluster",
                   "event_list": {
                       "event": [{
                           "name": "onentry",
                           "action": [
                               {
                                   "name": "Start RW.VM.APP.LEAD for trafsink",
                                   "start": {
                                       "python_variable": [ "vm_ip_address = '127.0.0.4'" ],
                                       "component_name": "RW.VM.APP.LEAD",
                                       "instance_id": "12",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start RW.VM.loadbal for trafsink",
                                   "start": {
                                       "python_variable": [ "vm_ip_address = '127.0.0.5'" ],
                                       "component_name": "RW.VM.loadbal",
                                       "instance_id": "13",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start RW.VM.trafsink for trafsink",
                                   "start": {
                                       "python_variable": [ "vm_ip_address = '127.0.0.6'" ],
                                       "component_name": "RW.VM.trafsink",
                                       "instance_id": "14",
                                       "config_ready": True
                                   }
                               }
                           ]
                       }]
                   }
               }
           },
           {
               "component_name": "RW.VM.APP.LEAD",
               "component_type": "RWVM",
               "rwvm": {
                   "leader": True,
                   "event_list": {
                       "event": [{
                           "name": "onentry",
                           "action": [
                               {
                                   "name": "Start the RW.Proc_9.Fpath~15567",
                                   "start": {
                                       "component_name": "RW.Proc_9.Fpath~15567",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start the RW.Proc_10.NcMgr",
                                   "start": {
                                       "component_name": "RW.Proc_10.NcMgr",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start the RW.Proc_11.FpCtrl",
                                   "start": {
                                       "component_name": "RW.Proc_11.FpCtrl",
                                       "config_ready": True
                                   }
                               },
                               {
                                   "name": "Start the RW.Proc_12.IfMgr",
                                   "start": {
                                       "component_name": "RW.Proc_12.IfMgr",
                                       "config_ready": True
                                   }
                               }
                           ]
                       }]
                   }
               }
           },

           {
               "component_name": "RW.Proc_9.Fpath~15567",
               "component_type": "RWPROC",
               "rwproc": {
                   "run_as": "root",
                   "tasklet": [{
                       "name": "Start RW.Fpath for RW.Proc_9.Fpath~15567",
                       "component_name": "RW.Fpath",
                       "instance_id": 3,
                       "config_ready": True,
                       "python_variable": [
                           "colony_name = 'trafsink'",
                           "colony_id = 8",
                           "port_map = 'vfap/3/1|eth_sim:name=fabric1|vfap'",
                           "cmdargs_str = ''"
                       ]
                   }]
               }
           },
           {
               "component_name": "RW.Proc_10.NcMgr",
               "component_type": "RWPROC",
               "rwproc": {
                   "run_as": "root",
                   "tasklet": [{
                       "name": "Start RW.NcMgr for RW.Proc_10.NcMgr",
                       "component_name": "RW.NcMgr",
                       "config_ready": True,
                       "python_variable": [
                           "cmdargs_str = ''",
                           "colony_name = 'trafsink'",
                           "colony_id = 8"
                       ]
                   }]
               }
           },


                {
                    "component_name": "RW.Proc_11.FpCtrl",
                    "component_type": "RWPROC",
                    "rwproc": {
                        "tasklet": [
                            {
                                "name": "Start RW.FpCtrl for RW.Proc_11.FpCtrl",
                                "component_name": "RW.FpCtrl",
                                "config_ready": True,
                                "python_variable": [
                                    "cmdargs_str = ''",
                                    "colony_name = 'trafsink'",
                                    "colony_id = 8"
                                ]
                            },
                            {
                                "name": "Start RW.NNLatencyTasklet for RW.Proc_11.FpCtrl",
                                "component_name": "RW.NNLatencyTasklet",
                                "config_ready": True
                            },
                            {
                                "name": "Start RW.SfMgr for RW.Proc_11.FpCtrl",
                                "component_name": "RW.SfMgr",
                                "config_ready": True
                            },
                            {
                                "name": "Start RW.SffMgr for RW.Proc_11.FpCtrl",
                                "component_name": "RW.SffMgr",
                                "config_ready": True
                            }
                        ]
                    }
                },

                {
                    "component_name": "RW.Proc_12.IfMgr",
                    "component_type": "RWPROC",
                    "rwproc": {
                        "tasklet": [{
                            "name": "Start RW.IfMgr for RW.Proc_12.IfMgr",
                            "component_name": "RW.IfMgr",
                            "config_ready": True,
                            "python_variable": [
                                "cmdargs_str = ''",
                                "colony_name = 'trafsink'",
                                "colony_id = 8"
                            ]
                        }]
                    }
                },
                {
                    "component_name": "RW.VM.loadbal",
                    "component_type": "RWVM",
                    "rwvm": {
                        "event_list": {
                            "event": [{
                                "name": "onentry",
                                "action": [{
                                    "name": "Start the RW.Proc_13.Fpath~30102",
                                    "start": {
                                        "component_name": "RW.Proc_13.Fpath~30102",
                                        "config_ready": True
                                    }
                                }]
                            }]
                        }
                    }
                },

                {
                    "component_name": "RW.Proc_13.Fpath~30102",
                    "component_type": "RWPROC",
                    "rwproc": {
                        "run_as": "root",
                        "tasklet": [{
                            "name": "Start RW.Fpath for RW.Proc_13.Fpath~30102",
                            "component_name": "RW.Fpath",
                            "instance_id": 4,
                            "config_ready": True,
                            "python_variable": [
                                "colony_name = 'trafsink'",
                                "colony_id = 8",
                                "port_map = 'vfap/4/1|eth_sim:name=fabric1|vfap,trafsink/4/1|eth_sim:name=lbport1|external,trafsink/4/2|eth_sim:name=lbport2|external'",
                                "cmdargs_str = ''"
                            ]
                        }]
                    }
                },
                {
                    "component_name": "RW.VM.trafsink",
                    "component_type": "RWVM",
                    "rwvm": {
                        "event_list": {
                            "event": [{
                                "name": "onentry",
                                "action": [{
                                    "name": "Start the RW.Proc_14.Fpath~27537",
                                    "start": {
                                        "component_name": "RW.Proc_14.Fpath~27537",
                                        "config_ready": True
                                    }
                                }]
                            }]
                        }
                    }
                },

                {
                    "component_name": "RW.Proc_14.Fpath~27537",
                    "component_type": "RWPROC",
                    "rwproc": {
                        "run_as": "root",
                        "tasklet": [{
                            "name": "Start RW.Fpath for RW.Proc_14.Fpath~27537",
                            "component_name": "RW.Fpath",
                            "instance_id": 5,
                            "config_ready": True,
                            "python_variable": [
                                "colony_name = 'trafsink'",
                                "colony_id = 8",
                                "port_map = 'vfap/5/1|eth_sim:name=fabric1|vfap,trafsink/5/1|eth_sim:name=trafsinkport|external'",
                                "cmdargs_str = ''"
                            ]
                        }]
                    }
                },

                {
                    "component_name": "RW_VM_MASTER",
                    "component_type": "RWVM",
                    "rwvm": {
                        "leader": True,
                        "event_list": {
                            "event": [{
                                "name": "onentry",
                                "action": [
                                    {
                                        "name": "Start the rw.colony",
                                        "start": {
                                            "component_name": "rw.colony",
                                            "config_ready": True
                                        }
                                    },
                                    {
                                        "name": "Start the RW.Proc_1.uAgent",
                                        "start": {
                                            "component_name": "RW.Proc_1.uAgent",
                                            "config_ready": True
                                        }
                                    },
#                                   {
#                                       "name": "Start the RW.CLI",
#                                       "start": {
#                                           "component_name": "RW.CLI",
#                                           "config_ready": True
#                                       }
#                                   },
                                    {
                                        "name": "Start the RW.Proc_2.Restconf",
                                        "start": {
                                            "component_name": "RW.Proc_2.Restconf",
                                            "config_ready": True
                                        }
                                    },
                                    {
                                        "name": "Start the RW.Proc_3.RestPortForward",
                                        "start": {
                                            "component_name": "RW.Proc_3.RestPortForward",
                                            "config_ready": True
                                        }
                                    },
                                    {
                                        "name": "Start the RW.MC.UI",
                                        "start": {
                                            "component_name": "RW.MC.UI",
                                            "config_ready": True
                                        }
                                    },
                                    {
                                        "name": "Start the logd",
                                        "start": {
                                            "component_name": "logd",
                                            "recovery_action": "RESTART",
                                            "config_ready": True
                                        }
                                    }
                                ]
                            }]
                        }
                    }
                },


                {
                    "component_name": "RW.Proc_1.uAgent",
                    "component_type": "RWPROC",
                    "rwproc": {
                        "tasklet": [{
                            "name": "Start RW.uAgent for RW.Proc_1.uAgent",
                            "component_name": "RW.uAgent",
                            "config_ready": True,
                            "python_variable": [ "cmdargs_str = '--confd-proto AF_INET --confd-ip 127.0.0.1'" ]
                        }]
                    }
                },
                {
                    "component_name": "RW.uAgent",
                    "component_type": "RWTASKLET",
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwuagent-c",
                        "plugin_name": "rwuagent-c"
                    }
                },
#               {
#                   "component_name": "RW.CLI",
#                   "component_type": "PROC",
#                   "native_proc": {
#                       "exe_path": "./usr/bin/rwcli",
#                       "args": "--netconf_host 127.0.0.1 --netconf_port 2022 --schema_listing cli_rwfpath_schema_listing.txt",
#                   }
#               },

                {
                    "component_name": "RW.Proc_2.Restconf",
                    "component_type": "RWPROC",
                    "rwproc": {
                        "tasklet": [{
                            "name": "Start RW.Restconf for RW.Proc_2.Restconf",
                            "component_name": "RW.Restconf",
                            "config_ready": True
                        }]
                    }
                },
                {
                    "component_name": "RW.Restconf",
                    "component_type": "RWTASKLET",
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/restconf",
                        "plugin_name": "restconf"
                    }
                },
#               {
#                   "component_name": "RW.Proc_3.RestPortForward",
#                   "component_type": "RWPROC",
#                   "rwproc": {
#                       "tasklet": [{
#                           "name": "Start RW.RestPortForward for RW.Proc_3.RestPortForward",
#                           "component_name": "RW.RestPortForward",
#                           "config_ready": True
#                       }]
#                   }
#               },


#               {
#                   "component_name": "RW.RestPortForward",
#                   "component_type": "RWTASKLET",
#                   "rwtasklet": {
#                       "plugin_directory": "./usr/lib/rift/plugins/restportforward",
#                       "plugin_name": "restportforward"
#                   }
#               },
                {
                    "component_name": "RW.MC.UI",
                    "component_type": "PROC",
                    "native_proc": {
                        "exe_path": "./usr/share/rw.ui/skyquake/scripts/launch_ui.sh",
                    }
                },
                {
                    "component_name": "logd",
                    "component_type": "RWTASKLET",
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwlogd-c",
                        "plugin_name": "rwlogd-c"
                    }
                },
                {
                    "component_name": "msgbroker",
                    "component_type": "RWTASKLET",
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwmsgbroker-c",
                        "plugin_name": "rwmsgbroker-c"
                    }
                },


                {
                    "component_name": "dtsrouter",
                    "component_type": "RWTASKLET",
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwdtsrouter-c",
                        "plugin_name": "rwdtsrouter-c"
                    }
                }
            ]
      })
        return manifest


    def tearDown(self):
        self.loop.stop()
        self.loop.close()

class MissionControl(rift.test.dts.AbstractDTSTest):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """
    @classmethod
    def configure_schema(cls):
        schema = RwYang.Model.load_and_merge_schema(
                     rwvcs.get_schema(),
                     'librwcal_yang_gen.so', 
                     'Rwcal')
        cls.model = RwYang.Model.create_libncx()
        cls.model.load_schema_ypbc(schema)
        xml = cls.manifest.to_xml_v2(cls.model, 1)
        xml = re.sub('rw-manifest:', '', xml)
        xml = re.sub('<manifest xmlns:rw-manifest="http://riftio.com/ns/riftware-1.0/rw-manifest">', '<?xml version="1.0" ?>\n<manifest xmlns="http://riftio.com/ns/riftware-1.0/rw-manifest" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://riftio.com/ns/riftware-1.0/rw-manifest ./rw-manifest.xsd">', xml)
        xml = '\n'.join(xml.split('\n')[1:])
        with open('lptestmanifest.xml', 'w') as f:
           f.write(str(xml))
        f.close()
        return schema

    def setUp(self):
        """
        1. Creates an asyncio loop
        2. Triggers the hook configure_test
        """
        def scheduler_tick(self, *args):
            self.call_soon(self.stop)
            self.run_forever()

        # Init params: loop & timers
        self.loop = asyncio.new_event_loop()

        self.loop.scheduler_tick = types.MethodType(scheduler_tick, self.loop)

        self.asyncio_timer = None
        self.stop_timer = None
        self.__class__.id_cnt += 1
        self.configure_test(self.loop, self.__class__.id_cnt)


    @classmethod
    def configure_manifest(cls):
        manifest = rwmanifest.Manifest()
        manifest.bootstrap_phase = rwmanifest.BootstrapPhase.from_dict({
            "rwmgmt": {
                "northbound_listing": [ "cli_rwmc_schema_listing.txt" ]
            }, 
            "rwtasklet": {
                "plugin_name": "rwinit-c"
            }, 
            "rwtrace": {
                "enable": True, 
                "level": 5, 
            }, 
            "log": {
                "enable": True, 
                "severity": 6, 
                "bootstrap_time": 30, 
                "console_severity": 5
            }, 
            "zookeeper": {
                "master_ip": "127.0.0.1", 
                "unique_ports": False, 
                "zake": True
            }, 
            "serf": {
                "start": True
            }, 
            "rwvm": {
                "instances": [
                    {
                        "component_name": "msgbroker", 
                        "config_ready": True
                    }, 
                    {
                        "component_name": "dtsrouter", 
                        "config_ready": True
                    }
                ]
            }, 
            "rwsecurity": {
                "use_ssl": True, 
                "cert": "/net/mahi/localdisk/kelayath/ws/coreha/etc/ssl/current.cert", 
                "key": "/net/mahi/localdisk/kelayath/ws/coreha/etc/ssl/current.key"
            }
        }) 
        manifest.init_phase = rwmanifest.InitPhase.from_dict({
            "environment": {
                "python_variable": [
                    "vm_ip_address = '127.0.0.1'",
                    "rw_component_name = 'vm-mission-control'",
                    "instance_id = 2",
                    "component_type = 'rwvm'",
                ], 
                "component_name": "$python(rw_component_name)", 
                "instance_id": "$python(instance_id)", 
                "component_type": "$python(rw_component_type)"
            }, 
            "settings": {
                "rwmsg": {
                    "multi_broker": {
                        "enable": True
                    }
                }, 
                "rwdtsrouter": {
                    "multi_dtsrouter": {
                        "enable": True
                    }
                }, 
                "rwvcs": {
                    "collapse_each_rwvm": True, 
                    "collapse_each_rwprocess": True
                }
            }
        }) 
        manifest.inventory = rwmanifest.Inventory.from_dict({
            "component": [
                {
                    "component_name": "rw.colony", 
                    "component_type": "RWCOLLECTION", 
                    "rwcollection": {
                        "collection_type": "rwcolony", 
                        "event_list": {
                            "event": [{
                                "name": "onentry", 
                                "action": [{
                                    "name": "Start vm-mission-control for rw.colony", 
                                    "start": {
                                        "python_variable": ["vm_ip_address = '127.0.0.1'"], 
                                        "component_name": "vm-mission-control", 
                                        "instance_id": "2", 
                                        "config_ready": True
                                    }
                                }]
                            }]
                        }
                    }
                }, 
                {
                    "component_name": "vm-mission-control", 
                    "component_type": "RWVM", 
                    "rwvm": {
                        "leader": True, 
                        "event_list": {
                            "event": [{
                                "name": "onentry", 
                                "action": [
                                    {
                                        "name": "Start the rw.colony", 
                                        "start": {
                                            "component_name": "rw.colony", 
                                            "config_ready": True
                                        }
                                    }, 
#                                   {
#                                       "name": "Start the RW.CLI", 
#                                       "start": {
#                                           "component_name": "RW.CLI", 
#                                           "config_ready": True
#                                       }
#                                   }, 
                                    {
                                        "name": "Start the RW.Proc_1.Restconf", 
                                        "start": {
                                            "component_name": "RW.Proc_1.Restconf", 
                                            "config_ready": True
                                        }
                                    }, 
                                    {
                                        "name": "Start the RW.Proc_2.RestPortForward", 
                                        "start": {
                                            "component_name": "RW.Proc_2.RestPortForward", 
                                            "config_ready": True
                                        }
                                    }, 
                                    {
                                        "name": "Start the RW.Proc_3.CalProxy", 
                                        "start": {
                                            "component_name": "RW.Proc_3.CalProxy", 
                                            "config_ready": True
                                        }
                                    }, 
                                    {
                                        "name": "Start the RW.Proc_4.mission-control", 
                                        "start": {
                                            "component_name": "RW.Proc_4.mission-control", 
                                            "config_ready": True
                                        }
                                    }, 
                                    {
                                        "name": "Start the RW.MC.UI", 
                                        "start": {
                                            "component_name": "RW.MC.UI", 
                                            "config_ready": True
                                        }
                                    }, 
                                    {
                                        "name": "Start the RW.uAgent", 
                                        "start": {
                                            "python_variable": ["cmdargs_str = '--confd-proto AF_INET --confd-ip 127.0.0.1'"],
                                            "component_name": "RW.uAgent", 
                                            "config_ready": True
                                        }
                                    }, 
                                    {
                                        "name": "Start the logd", 
                                        "start": {
                                            "component_name": "logd", 
                                            "config_ready": True
                                        }
                                    }
                                ]
                            }]
                        }
                    }
                }, 
#               {
#                   "component_name": "RW.CLI", 
#                   "component_type": "PROC", 
#                   "native_proc": {
#                       "exe_path": "./usr/bin/rwcli", 
#                       "args": "--netconf_host 127.0.0.1 --netconf_port 2022 --schema_listing cli_rwmc_schema_listing.txt", 
#                   }
#               }, 
                {
                    "component_name": "RW.Proc_1.Restconf", 
                    "component_type": "RWPROC", 
                    "rwproc": {
                        "tasklet": [{
                            "name": "Start RW.Restconf for RW.Proc_1.Restconf", 
                            "component_name": "RW.Restconf", 
                            "config_ready": True
                        }]
                    }
                }, 
                {
                    "component_name": "RW.Restconf", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/restconf", 
                        "plugin_name": "restconf"
                    }
                }, 
                {
                    "component_name": "RW.Proc_2.RestPortForward", 
                    "component_type": "RWPROC", 
                    "rwproc": {
                        "tasklet": [{
                            "name": "Start RW.RestPortForward for RW.Proc_2.RestPortForward", 
                            "component_name": "RW.RestPortForward", 
                            "config_ready": True
                        }]
                    }
                }, 
                {
                    "component_name": "RW.RestPortForward", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/restportforward", 
                        "plugin_name": "restportforward"
                    }
                }, 
                {
                    "component_name": "RW.Proc_3.CalProxy", 
                    "component_type": "RWPROC", 
                    "rwproc": {
                        "tasklet": [{
                            "name": "Start RW.CalProxy for RW.Proc_3.CalProxy", 
                            "component_name": "RW.CalProxy", 
                            "config_ready": True
                        }]
                    }
                }, 
                {
                    "component_name": "RW.CalProxy", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwcalproxytasklet", 
                        "plugin_name": "rwcalproxytasklet"
                    }
                }, 
                {
                    "component_name": "RW.Proc_4.mission-control", 
                    "component_type": "RWPROC", 
                    "rwproc": {
                        "tasklet": [{
                            "name": "Start mission-control for RW.Proc_4.mission-control", 
                            "component_name": "mission-control", 
                            "config_ready": True
                        }]
                    }
                }, 
                {
                    "component_name": "mission-control", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwmctasklet", 
                        "plugin_name": "rwmctasklet"
                    }
                }, 
                {
                    "component_name": "RW.MC.UI", 
                    "component_type": "PROC", 
                    "native_proc": {
                        "exe_path": "./usr/share/rw.ui/skyquake/scripts/launch_ui.sh", 
                    }
                }, 
                {
                    "component_name": "RW.uAgent", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwuagent-c", 
                        "plugin_name": "rwuagent-c"
                    }
                }, 
                {
                    "component_name": "logd", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwlogd-c", 
                        "plugin_name": "rwlogd-c"
                    }
                }, 
                {
                    "component_name": "msgbroker", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwmsgbroker-c", 
                        "plugin_name": "rwmsgbroker-c"
                    }
                }, 
                {
                    "component_name": "dtsrouter", 
                    "component_type": "RWTASKLET", 
                    "rwtasklet": {
                        "plugin_directory": "./usr/lib/rift/plugins/rwdtsrouter-c", 
                        "plugin_name": "rwdtsrouter-c"
                    }
                }
            ]
        })
        return manifest

    def tearDown(self):
        self.loop.stop()
        self.loop.close()

@unittest.skipUnless('FORCE' in os.environ, 'Useless until DTS can actually be deallocated, RIFT-7085')
class DtsTestCase(DtsPerf):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """

    def test_read_pub(self):
        """
        Verify that pub/sub are working when the publisher and subscriber are
        different Tasklets.

        The test will progress through stages defined by the events list:
            0:  Publisher registration hit on_ready()
        """
        print("{{{{{{{{{{{{{{{{{{{{STARTING - test_read_pub")
        stop_seq_exp = []
        stop_seq_act = []
#       confd_host="127.0.0.1"

        events = [asyncio.Event(loop=self.loop) for _ in range(2)]


        ret = toyyang.AContainer()
        ret.a_string = 'pub'
        os.environ['DTS_HA_TEST'] = '1'
        @asyncio.coroutine
        def sub():
            nonlocal stop_seq_exp
            nonlocal stop_seq_act

            tinfo = self.new_tinfo('sub')
            self.dts_mgmt = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            # Sleep for DTS registrations to complete
            yield from asyncio.sleep(1, loop=self.loop)

            @asyncio.coroutine
            def append_result_curr_list():
#               vcs_info = rwbase.VcsInfo() 
                res_iter = yield from self.dts_mgmt.query_read('/rw-base:vcs/rw-base:info', flags=0)
                for i in res_iter:
                    info_result = yield from i
                components = info_result.result.components.component_info
                recvd_list = {}
                for component in components:
                    recvd_list[component.instance_name] = component.rwcomponent_parent
                return  len(info_result.result.components.component_info), recvd_list

            @asyncio.coroutine
            def issue_vstart(component, parent, recover=False):
                # Issue show vcs info 
                vstart_input = rwvcs.VStartInput()
                vstart_input.component_name = component
                vstart_input.parent_instance = parent
                vstart_input.recover = recover
                query_iter = yield from self.dts_mgmt.query_rpc( xpath="/rw-vcs:vstart",
                                                      flags=0, msg=vstart_input)
                yield from asyncio.sleep(2, loop=self.loop)

            @asyncio.coroutine
            def issue_vstop(stop_instance_name, flag=0):
                vstop_input = rwvcs.VStopInput(instance_name=stop_instance_name)
                query_iter = yield from self.dts_mgmt.query_rpc( xpath="/rw-vcs:vstop",
                                                      flags=flag, msg=vstop_input)
                yield from asyncio.sleep(1, loop=self.loop)

            r_len, r_dict = yield from append_result_curr_list()
            old_dict = r_dict

            yield from issue_vstart('logd', 'rwdtsperf-c-vm-729');

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(r_dict.keys()) - set(old_dict.keys())
            logd_inst = new_inst.pop()
            old_dict = r_dict
            
            #logd_tinfo = rwmain.Gi.find_taskletinfo_by_name(self.rwmain, logd_inst)
            #logd_dtsha = rift.tasklets.DTS(logd_tinfo, self.schema, self.loop)
            #logd_regh = rwdts.Api.find_reg_handle_for_xpath(logd_dtsha._dts, "D,/rwdts:dts/rwdts:member")
            #print("tinfo is {}, dtsha {} raw_regh {}\n".format(logd_tinfo, logd_dtsha, logd_regh))


            yield from issue_vstart('rwdtsperf-c-proc', 'rwdtsperf-c-vm-729');
          
            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(r_dict.keys()) - set(old_dict.keys())
            pop_inst = new_inst.pop()
            if 'rwdtsperf-c-proc' in pop_inst:
                proc_inst = pop_inst
                proc_perf_inst = new_inst.pop()
            else:
                proc_perf_inst = pop_inst
                proc_inst = new_inst.pop()
            old_dict = r_dict

            yield from issue_vstart('RW.Proc_1.uAgent', 'rwdtsperf-c-vm-729');
          
            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(r_dict.keys()) - set(old_dict.keys())
            pop_inst = new_inst.pop()
            if 'RW.Proc_1.uAgent' in pop_inst:
                proc_u_inst = pop_inst
                u_inst = new_inst.pop()
            else:
                u_inst = pop_inst
                proc_u_inst = new_inst.pop()
            old_dict = r_dict

            yield from issue_vstart('rwdtsperf-c', 'rwdtsperf-c-vm-729');

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(r_dict.keys()) - set(old_dict.keys())
            pop_inst = new_inst.pop()
            if 'rwdtsperf-c' in pop_inst:
                perf_inst = pop_inst
                confd_inst = new_inst.pop()
            else:
                confd_inst = pop_inst
                perf_inst = new_inst.pop()
            old_dict = r_dict
            stop_seq_exp = [perf_inst, logd_inst, proc_inst, proc_perf_inst, u_inst, proc_u_inst, confd_inst, 'NOT_REMOVED', 'NOT_ADDED' ]

            yield from issue_vstop(perf_inst)

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(old_dict.keys()) - set(r_dict.keys())
            stop_seq_act.append(new_inst.pop())
            old_dict = r_dict

            yield from issue_vstop(logd_inst)

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(old_dict.keys()) - set(r_dict.keys())
            stop_seq_act.append(new_inst.pop())
            old_dict = r_dict

            yield from issue_vstop(proc_inst)

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(old_dict.keys()) - set(r_dict.keys())
            pop_inst = new_inst.pop()
            if 'rwdtsperf-c-proc' in pop_inst:
                stop_seq_act.append(pop_inst)
                stop_seq_act.append(new_inst.pop())
            else:
                stop_seq_act.append(new_inst.pop())
                stop_seq_act.append(pop_inst)
            old_dict = r_dict

            yield from issue_vstop(u_inst)

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(old_dict.keys()) - set(r_dict.keys())
            stop_seq_act.append(new_inst.pop())
            old_dict = r_dict

            yield from issue_vstop(proc_u_inst)

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(old_dict.keys()) - set(r_dict.keys())
            stop_seq_act.append(new_inst.pop())
            old_dict = r_dict

            yield from issue_vstop(confd_inst)

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(old_dict.keys()) - set(r_dict.keys())
            stop_seq_act.append(new_inst.pop())
            old_dict = r_dict

            yield from issue_vstart('rwdtsperf-c-proc-restart', 'rwdtsperf-c-vm-729');
          
            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(r_dict.keys()) - set(old_dict.keys())
            pop_inst = new_inst.pop()
            if 'rwdtsperf-c-proc-restart' in pop_inst:
                proc_r_inst = pop_inst
                proc_r_perf_inst = new_inst.pop()
            else:
                proc_r_perf_inst = pop_inst
                proc_r_inst = new_inst.pop()
            old_dict = r_dict

            yield from issue_vstop(proc_r_perf_inst)

            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(old_dict.keys()) - set(r_dict.keys())
            if len(new_inst) is 0:
                stop_seq_act.append('NOT_REMOVED')
            else:
                stop_seq_act.append(new_inst.pop())
            old_dict = r_dict

            yield from issue_vstart('rwdtsperf-c', proc_r_inst, True);
          
            r_len, r_dict = yield from append_result_curr_list()
            new_inst = set(r_dict.keys()) - set(old_dict.keys())
            if len(new_inst) is 0:
                stop_seq_act.append('NOT_ADDED')
            else:
                stop_seq_act.append(new_inst.pop())
            old_dict = r_dict

            events[1].set()

        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set, timeout=120)

        self.assertEqual(str(stop_seq_exp), str(stop_seq_act))
        print("}}}}}}}}}}}}}}}}}}}}DONE - test_pub")

@unittest.skipUnless('FORCE' in os.environ, 'Useless until DTS can actually be deallocated, RIFT-7085')
class DtsTrafgenTestCase(Trafgen):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """

    def trafgen(self):
        """
        Verify the trafgen setup recovery functions 
        The test will progress through stages defined by the events list:
            0:  Trafgen setup is brought up
            1:  Configuration fed by connecting through confd
            2:  Tasklet/PROC/VM restarts tested to confirm recovery is proper 
        """

        print("{{{{{{{{{{{{{{{{{{{{STARTING - trafgen recovery")
        confd_host="127.0.0.1"

        events = [asyncio.Event(loop=self.loop) for _ in range(2)]

#       xpath = '/rw-dts-toy-tasklet:a-container'

        ret = toyyang.AContainer()
        ret.a_string = 'pub'
        os.environ['DTS_HA_TEST'] = '1'
        @asyncio.coroutine
        def sub():

            tinfo = self.new_tinfo('sub')
            self.dts_mgmt = rift.tasklets.DTS(tinfo, self.schema, self.loop)
            # Sleep for DTS registrations to complete
            yield from asyncio.sleep(5, loop=self.loop)

            @asyncio.coroutine
            def issue_vstart(component, parent, recover=False):
                vstart_input = rwvcs.VStartInput()
                vstart_input.component_name = component
                vstart_input.parent_instance = parent
                vstart_input.recover = recover
                query_iter = yield from self.dts_mgmt.query_rpc( xpath="/rw-vcs:vstart",
                                                      flags=0, msg=vstart_input)
                yield from asyncio.sleep(2, loop=self.loop)

            @asyncio.coroutine
            def issue_vstop(stop_instance_name, flag=0):
                vstop_input = rwvcs.VStopInput(instance_name=stop_instance_name)
                query_iter = yield from self.dts_mgmt.query_rpc( xpath="/rw-vcs:vstop",
                                                      flags=flag, msg=vstop_input)
                yield from asyncio.sleep(1, loop=self.loop)

            yield from asyncio.sleep(10, loop=self.loop)

            #Confd 
            print('connecting confd')
            mgmt_session=rift.auto.session.NetconfSession(confd_host)
            mgmt_session.connect()
            print('CONNECTED CONFD')

            #logd_tinfo = rwmain.Gi.find_taskletinfo_by_name(self.rwmain, 'logd-103')
            #logd_dtsha = rift.tasklets.DTS(logd_tinfo, self.schema, self.loop)
            

            events[1].set()

        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set, timeout=120)

        print("}}}}}}}}}}}}}}}}}}}}DONE - trafgen")

@unittest.skipUnless('FORCE' in os.environ, 'Useless until DTS can actually be deallocated, RIFT-7085')
class MissionCTestCase(MissionControl):
    """
    DTS GI interface unittests

    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """
    def mission_control(self):
        """
        Verify the mission_control setup functions
        The test will progress through stages defined by the events list:
            0:  mission_control setup is brought up
            1:  Configuration fed by connecting through confd
            2:  Tasklet/PROC/VM restarts tested to confirm recovery is proper
        """

        print("{{{{{{{{{{{{{{{{{{{{STARTING - mano recovery test")
        confd_host="127.0.0.1"

        events = [asyncio.Event(loop=self.loop) for _ in range(2)]


        @asyncio.coroutine
        def sub():

            tinfo = self.new_tinfo('sub')
            self.dts_mgmt = rift.tasklets.DTS(tinfo, self.schema, self.loop)

            # Sleep for DTS registrations to complete
            yield from asyncio.sleep(240, loop=self.loop)

            #Confd
            print('connecting confd')
            mgmt_session=rift.auto.session.NetconfSession(confd_host)
            mgmt_session.connect()
            print('CONNECTED CONFD')

            #logd_tinfo = rwmain.Gi.find_taskletinfo_by_name(self.rwmain, 'logd-103')
            #logd_dtsha = rift.tasklets.DTS(logd_tinfo, self.schema, self.loop)

            events[1].set()

        asyncio.ensure_future(sub(), loop=self.loop)
        self.run_until(events[1].is_set, timeout=360)

def main():
    install_dir = os.environ['RIFT_INSTALL']
    plugin_dir = os.path.join(os.environ["RIFT_INSTALL"], "usr/lib/rift/plugins")

    if 'DTS_TEST_PUB_DIR' not in os.environ:
        os.environ['DTS_TEST_PUB_DIR'] = os.path.join(plugin_dir, 'dtstestpub')

    if 'RIFT_NO_SUDO_REAPER' not in os.environ:
        os.environ['RIFT_NO_SUDO_REAPER'] = '1'

    if 'MESSAGE_BROKER_DIR' not in os.environ:
        os.environ['MESSAGE_BROKER_DIR'] = os.path.join(plugin_dir, 'rwmsgbroker-c')

    if 'ROUTER_DIR' not in os.environ:
        os.environ['ROUTER_DIR'] = os.path.join(plugin_dir, 'rwdtsrouter-c')

    if 'RW_VAR_RIFT' not in os.environ:
        os.environ['RW_VAR_RIFT'] = '1'
    
    if 'INSTALLDIR' in os.environ:
        os.chdir(os.environ.get('INSTALLDIR')) 

#   if 'RWMSG_BROKER_SHUNT' not in os.environ:
#       os.environ['RWMSG_BROKER_SHUNT'] = '1'

    if 'TEST_ENVIRON' not in os.environ:
        os.environ['TEST_ENVIRON'] = '1'

    if 'RW_MANIFEST' not in os.environ:
        os.environ['RW_MANIFEST'] = os.path.join(install_dir, 'lptestmanifest.xml')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args, _ = parser.parse_known_args()

    DtsTestCase.log_level = logging.DEBUG if args.verbose else logging.WARN

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()

# vim: sw=4
