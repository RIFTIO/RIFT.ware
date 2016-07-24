#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import sys
import os
import argparse
import uuid

import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('RwvcsTypesYang', '1.0')

from gi.repository import (
    RwYang,
    RwVnfdYang,
    RwvcsTypesYang)

class RwVcsComponent(object):
    """Base class for VCS component"""
    def __init__(self, name):
        self.component = RwVnfdYang.VcsComponent()
        self.component.component_name = name
        self.python_variable = None

class RwVcsTasklet(RwVcsComponent):
    """Base class for RWTASKLET component"""
    def __init__(self, name, plugin_name):
        super(RwVcsTasklet, self).__init__(name)
        self.component.component_type = 'RWTASKLET'
        self.component.rwtasklet.plugin_directory = './usr/lib/rift/plugins/%s' % plugin_name
        self.component.rwtasklet.plugin_name = plugin_name
        self.component.rwtasklet.plugin_version = '1.0'

class RwVcsCliTasklet(RwVcsTasklet):
    """CLI tasklet compoenent"""
    def __init__(self, manifest_file=None):
        super(RwVcsCliTasklet, self).__init__('rwcli_tasklet', 'rwcli-c')
        if manifest_file is not None:
            self.python_variable = [ '&quot;load cli manifest %s&quot;' % manifest_file ]

class RwVcsDtsRouterTasklet(RwVcsTasklet):
    def __init__(self):
        super(RwVcsDtsRouterTasklet, self).__init__('rwdtsrouter_tasklet',
                                                    'rwdtsrouter-c')
class RwVcsMgmtTasklet(RwVcsTasklet):
    def __init__(self):
        super(RwVcsMgmtTasklet, self).__init__('rwuagent_tasklet',
                                               'rwuagent-c')

class RwVcsLogdTasklet(RwVcsTasklet):
    def __init__(self):
        super(RwVcsLogdTasklet, self).__init__('rwlogd_tasklet',
                                               'rwlogd-c')

class RwVcsMsgBrokerTasklet(RwVcsTasklet):
    def __init__(self):
        super(RwVcsMsgBrokerTasklet, self).__init__('rwmsgbroker_tasklet',
                                                    'rwmsgbroker-c')

class RwVcsRestconfTasklet(RwVcsTasklet):
    def __init__(self):
        super(RwVcsRestconfTasklet, self).__init__('rwrestconf_tasklet',
                                                   'restconf')

class RwVcsPingTasklet(RwVcsTasklet):
    def __init__(self):
        super(RwVcsPingTasklet, self).__init__('rwping_tasklet',
                                               'rwpingtasklet')

class RwVcsPongTasklet(RwVcsTasklet):
    def __init__(self):
        super(RwVcsPongTasklet, self).__init__('rwpong_tasklet',
                                               'rwpongtasklet')

class RwVcsProc(RwVcsComponent):
    """Base class for RWPROC VCS component"""
    def __init__(self, name, rwtasklet_list):
        super(RwVcsProc, self).__init__(name)
        self.component.component_type = 'RWPROC'
        self.rwtasklet_list = rwtasklet_list
        for rwtasklet in rwtasklet_list:
            tasklet = self.component.rwproc.tasklet.add()
            tasklet.name = rwtasklet.component.component_name
            tasklet.component_name = rwtasklet.component.component_name
            if rwtasklet.python_variable is not None:
                tasklet.python_variable = rwtasklet.python_variable

    def get_sub_components(self):
        return self.rwtasklet_list

class RwVcsCliProc(RwVcsProc):
    def __init__(self, manifest_file=None):
        rwtasklet_list = [ RwVcsCliTasklet(manifest_file) ]
        super(RwVcsCliProc, self).__init__('rwcli_proc', rwtasklet_list)

class RwVcsDtsRouterProc(RwVcsProc):
    def __init__(self, manifest_file=None):
        rwtasklet_list = [ RwVcsDtsRouterTasklet() ]
        super(RwVcsDtsRouterProc, self).__init__('rwdtsrouter_proc', rwtasklet_list)

class RwVcsRestconfProc(RwVcsProc):
    def __init__(self, manifest_file=None):
        rwtasklet_list = [ RwVcsRestconfTasklet() ]
        super(RwVcsRestconfProc, self).__init__('rwrestconf_proc', rwtasklet_list)

class RwVcsPingProc(RwVcsProc):
    def __init__(self, manifest_file=None):
        rwtasklet_list = [ RwVcsPingTasklet() ]
        super(RwVcsPingProc, self).__init__('rwping_proc', rwtasklet_list)

class RwVcsPongProc(RwVcsProc):
    def __init__(self, manifest_file=None):
        rwtasklet_list = [ RwVcsPongTasklet() ]
        super(RwVcsPongProc, self).__init__('rwpong_proc', rwtasklet_list)

class RwVcsNativeProc(RwVcsComponent):
    """Base class for VCS native PROC"""
    def __init__(self, name, exe_path):
        super(RwVcsNativeProc, self).__init__(name)
        self.component.component_type = 'PROC'
        self.component.native_proc.exe_path = exe_path

class RwVcsConfdNativeProc(RwVcsNativeProc):
    def __init__(self):
        super(RwVcsConfdNativeProc, self).__init__('confd', './usr/bin/rw_confd')

class RwVcsVM(RwVcsComponent):
    def __init__(self, name, rwproc_list, rwtasklet_list=[], nativeproc_list=[]):
        super(RwVcsVM, self).__init__(name)
        self.component.component_type = 'RWVM'
        event = self.component.rwvm.event_list.event.add()
        event.name = 'onentry'

        self.rwtasklet_list = rwtasklet_list
        for rwtasklet in rwtasklet_list:
            action = event.action.add()
            action.name = "start-%s" % rwtasklet.component.component_name
            action.start.component_name = rwtasklet.component.component_name

        self.nativeproc_list = nativeproc_list
        for nativeproc in nativeproc_list:
            action = event.action.add()
            action.name = "start-%s" % nativeproc.component.component_name
            action.start.component_name = nativeproc.component.component_name

        self.rwproc_list = rwproc_list
        for rwproc in rwproc_list:
            action = event.action.add()
            action.name = "start-%s" % rwproc.component.component_name
            action.start.component_name = rwproc.component.component_name

    def get_sub_components(self):
        sub_list = self.rwtasklet_list
        for rwproc in self.rwproc_list:
            sub_list = sub_list + rwproc.get_sub_components()

        sub_list = sub_list + self.rwproc_list + self.nativeproc_list
        return sub_list

class RwVcsPingVM(RwVcsVM):
    def __init__(self, manifest_file=None):
        rwproc_list = [ RwVcsPingProc() ]
        super(RwVcsPingVM, self).__init__('rwping_vm', rwproc_list)

class RwVcsPongVM(RwVcsVM):
    def __init__(self, manifest_file=None):
        rwproc_list = [ RwVcsPongProc() ]
        super(RwVcsPongVM, self).__init__('rwpong_vm', rwproc_list)

class RwVcsCollection(RwVcsComponent):
    def __init__(self, name, rwvm_list, rwproc_list=[], rwtasklet_list=[], nativeproc_list=[]):
        super(RwVcsCollection, self).__init__(name)
        self.component.component_type = 'RWCOLLECTION'
        event = self.component.rwcollection.event_list.event.add()
        self.component.rwcollection.collection_type = 'rwcluster'
        event.name = 'onentry'

        self.rwtasklet_list = rwtasklet_list
        for rwtasklet in rwtasklet_list:
            action = event.action.add()
            action.name = "start-%s" % rwtasklet.component.component_name
            action.start.component_name = rwtasklet.component.component_name

        self.nativeproc_list = nativeproc_list
        for nativeproc in nativeproc_list:
            action = event.action.add()
            action.name = "start-%s" % nativeproc.component.component_name
            action.start.component_name = nativeproc.component.component_name

        self.rwproc_list = rwproc_list
        for rwproc in rwproc_list:
            action = event.action.add()
            action.name = "start-%s" % rwproc.component.component_name
            action.start.component_name = rwproc.component.component_name

        self.rwvm_list = rwvm_list
        for rwvm in rwvm_list:
            action = event.action.add()
            action.name = "start-%s" % rwvm.component.component_name
            action.start.component_name = rwvm.component.component_name

    def get_sub_components(self):
        sub_list = self.rwtasklet_list
        for rwproc in self.rwproc_list:
            sub_list = sub_list + rwproc.get_sub_components()
        for rwvm in self.rwvm_list:
            sub_list = sub_list + rwvm.get_sub_components()

        sub_list = sub_list + self.rwproc_list + self.nativeproc_list + self.rwvm_list
        return sub_list

