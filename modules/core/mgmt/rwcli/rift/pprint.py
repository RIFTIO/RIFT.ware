#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# -*- coding: utf-8 -*-

from __future__ import print_function
import os
import sys
import re
import time
# pylint: disable=W0402
import string
import xml.etree.ElementTree as ET
import hashlib

from collections import namedtuple

import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwDebugYang', '1.0')
gi.require_version('RwmsgDataYang', '1.0')
gi.require_version('RwMemlogYang', '1.0')


import gi.repository.GLib as glib
import gi.repository.RwYang as rwyang
import gi.repository.RwBaseYang as rwbase
import gi.repository.RwDebugYang as rwdebug
import gi.repository.RwmsgDataYang as rwmsg
import gi.repository.RwMemlogYang as rwmemlog


class RwMsgMessaging(object):
    """Helper class for parsing messing info
    This class is used during the pretty printing of messaging info"""
    #brokers = []

    def __init__(self):
        self.brokers = list()

    def parse(self, pp, root):
        for child in root:
            # strip name space if it is present in the tag
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                if tag == "broker":
                    broker = RwMsgBroker()
                    self.brokers.append(broker)
                    broker.parse(pp, child)
                else:
                    self.parse(pp, child)
            else:
                if tag == "tasklet-name":
                    broker.tasklet_name = child.text
                elif tag == "instance_uri":
                    broker.instance_uri = child.text

    def print(self):
        for broker in self.brokers:
            broker.print()
            print("\n")

class RwMsgBroker(object):
    """Helper class for parsing messing info
    This class is used during the pretty printing of messaging info"""
    #tasklet_name = "-"
    #instance_uri = "-"
    #channels = []

    def __init__(self):
        self.tasklet_name = "-"
        self.instance_uri = "-"
        self.channels = []

    def parse(self, pp, root):
        for child in root:
            # strip name space if it is present in the tag
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                if tag == "channels":
                    channel = RwMsgChannel()
                    self.channels.append(channel)
                channel.parse(pp, child)
            else:
                if tag == "tasklet-name":
                    self.tasklet_name = child.text
                elif tag == "instance_uri":
                    self.instance_uri = child.text

    def print(self):
        print("BROKER: tasklet-name: ", self.tasklet_name, "instance-uri:", self.instance_uri)
        for channel in self.channels:
            channel.print()

class RwMsgChannel(object):
    """Helper class for parsing messing info
    This class is used during the pretty printing of messaging info"""

    def __init__(self):
        self.chan_type = "-"
        self.chan_clict = "-"
        self.chan_id = "-"
        self.peer_id = "-"
        self.peer_pid = "-"
        self.peer_ip = "-"
        self.methbindings = []
        self.sockets = []

    def parse(self, pp, root):
        for child in root:
            # strip name space if it is present in the tag
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                if tag == "methbindings":
                    methbinding = RwMsgMethod()
                    self.methbindings.append(methbinding)
                    methbinding.parse(pp, child)
                elif tag == "sockets":
                    socket = RwMsgSocket()
                    self.sockets.append(socket)
                    socket.parse(pp, child)
            else:
                if tag == "type":
                    self.chan_type = {"BROCLI":"C", "BROSRV":"S", "PEERCLI":"p", "PEERSRV":"P"}[child.text]
                elif tag == "clict":
                    self.chan_clict = child.text
                elif tag == "id":
                    self.chan_id = child.text
                elif tag == "peer_id":
                    self.peer_id = child.text
                elif tag == "peer_pid":
                    self.peer_pid = child.text
                elif tag == "peer_ip":
                    self.peer_ip = child.text

    def print(self):
        if self.chan_type == 'P':
            channel_str = self.chan_type + self.chan_id + ']'
        else:
            channel_str = self.chan_type + self.chan_id + ']' + ' peer=' + self.peer_ip + ':' + self.peer_pid + '/' + self.peer_id
        print("  CHANNEL: [channel", channel_str)
        socket_dict = {}
        for socket in self.sockets:
            socket_dict[socket.pri] = (socket.paused, socket.connected, socket.state)
        socket_str = ""
        for i in [0,1,2,3,4]:
            try:
              socket_str += 'pri-'+str(i)+' '+str(['-', 'P'][socket_dict[0][0]])+'/'+str(['-','C'][socket_dict[0][1]])+'/'+str(socket_dict[0][2])+'  '
            except:
                pass
        print("    SOCKETS:", socket_str)
        if len(self.methbindings) > 0:
            print("    METHODS:")
        method_dict = {}
        for methbinding in self.methbindings:
            try:
                    method_dict[hex(methbinding.pathhash).rstrip("L")][1].append(tuple((methbinding.payt, methbinding.methno)))
            except KeyError:
                method_dict[hex(methbinding.pathhash).rstrip("L")] = tuple((methbinding.path, [tuple((methbinding.payt, methbinding.methno))]))
            #THE FOLLOWING CODE PRINTS METHODS IN A DIFFERENT STYLE
            #method_dict[methbinding.pathhash] = method_dict[methbinding.pathhash].append((methbinding.payt, methbinding.methno))
            #method_str = hex(methbinding.pathhash).rstrip("L") + ' payt ' + str(methbinding.payt) + ' methno ' + str(methbinding.methno) + ' path=' + methbinding.path
            #print("      ", method_str)
        for phash in method_dict.keys():
            method_str = ""
            count = 0
            for i in method_dict[phash][1]:
                count+=1
                method_str += str(i) + ['\n                       ', ' '][int(count%4!=0)]
            print("      path=%s (%s)\n      (payt, methno)s: %s" %(method_dict[phash][0], phash, method_str))


class RwMsgMethod(object):
    """Helper class for parsing messing info
    This class is used during the pretty printing of messaging info"""

    def __init__(self):
        self.btype = "-"
        self.path = "-"
        self.payfmt = "-"
        self.payt = None
        self.methno = None
        self.pbapi_srvno = "-"
        self.pbapi_methno = "-"
        self.srvchanct = "-"
        self.pathhash = None

    def parse(self, pp, root):
        for child in root:
            # strip name space if it is present in the tag
            tag = pp.strip_tag_namespace(child.tag)

            if tag == "btype":
                try:
                    self.btype = {"SRVCHAN":"S", "BROSRVCHAN":"L"}[child.text]
                except Exception as err:
                    print(child.text)
            elif tag == "path":
                self.path = child.text
            elif tag == "pathhash":
                self.pathhash = int(child.text)
            elif tag == "methno":
                self.methno = int(child.text)
            elif tag == "payfmt":
                self.payfmt = child.text
                self.payt = {"RAW":0, "PBAPI":1, "TEXT":2, "MSGCTL":3}[child.text]
            elif tag == "pbapi_srvno":
                self.pbapi_srvno = child.text
            elif tag == "pbapi_methno":
                self.pbapi_methno = child.text
            elif tag == "srvchanct":
                self.srvchanct = child.text

class RwMsgSocket(object):
    """Helper class for parsing messing info
    This class is used during the pretty printing of messaging info"""

    def __init__(self):
        self.pri = None
        self.paused = None
        self.connected = None
        self.state = None

    def parse(self, pp, root):
        for child in root:
            # strip name space if it is present in the tag
            tag = pp.strip_tag_namespace(child.tag)

            if tag == "pri":
                self.pri = int(child.text)
            elif tag == "paused":
                self.paused = {"true":True, "false":False}[child.text]
            elif tag == "connected":
                self.connected = {"true":True, "false":False}[child.text]
            elif tag == "state":
                self.state = {"NNSK_CONNECTED":"C", "NNSK_IDLE":"I"}[child.text]

class RwTaskletHeapMemory(object):
    def __init__(self):
        self.allocated = 0
        self.chunks = 0

    def parse(self, pp, root):
        for child in root:
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                print("What??")
            else:
                if tag == "allocated":
                    self.allocated = int(child.text)
                elif tag == "chunks":
                    self.chunks = int(child.text)
        return (self.allocated, self.chunks)

class RwTaskletHeapCaller(object):
    def __init__(self):
        self.info = ''

    def parse(self, pp, root):
        for child in root:
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                print("What??")
            else:
                if tag == "info":
                    self.info = child.text

class RwTaskletHeapAllocation(object):
    def __init__(self):
        self.caller = []

    def parse(self, pp, root):
        for child in root:
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                if tag == "caller":
                    caller = RwTaskletHeapCaller()
                    caller.parse(pp, child)
                    self.caller.append(caller.info)
                else:
                    self.parse(pp, child)
            else:
                if tag == "address":
                    self.address = int(child.text)
                elif tag == "type":
                    self.atype = child.text
                elif tag == "size":
                    self.size = int(child.text)

        digest_string = ''.join(self.caller) + str(self.size) + self.atype
        self.digest = hashlib.md5(digest_string.encode()).hexdigest()

class RwTaskletHeapTasklet(object):
    def __init__(self):
        self.allocations = {}
        self.memory = None

    def parse(self, pp, root):
        #print("i m inside tasklet.parse()")
        for child in root:
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                if tag == "allocation":
                    allocation = RwTaskletHeapAllocation()
                    allocation.parse(pp, child)
                    #self.allocations.append(allocation)
                    try:
                        self.allocations[allocation.digest][1].append(allocation.address)
                    except KeyError:
                        self.allocations[allocation.digest] = tuple((tuple((allocation.atype, allocation.size, allocation.caller)), [ allocation.address ]))
                elif tag == "memory":
                    memory = RwTaskletHeapMemory()
                    self.memory = memory.parse(pp, child)
                else:
                    self.parse(pp, child)
            else:
                if tag == "name":
                    #print("i found name %s" %(child.text))
                    self.name = child.text

    def print_callers(self, alloc):
        print("  allocation:\n",
              "   size: %-12u type: %s"
              %( alloc[1], alloc[0] ))
        for i in range(0, len(alloc[2])):
            print("    caller%u: %s" %(i, alloc[2][i]))

    def print(self):
        print("Tasklet-name: %s" % (self.name))
        #for allocation in self.allocations:
            #allocation.print()
        for digest in self.allocations.keys():
            self.print_callers(self.allocations[digest][0])
            count = 0
            addresses_str = ''
            for address in self.allocations[digest][1]:
                addresses_str +=  ['\n      ', ' '][int(count%7!=0)] + hex(address).rstrip("L")
                count+=1
            print("    address-list: (count:%u)" % len(self.allocations[digest][1]))
            print(addresses_str)
            print("\n")
        print("  total memory - allocated: %-10u chunks: %-10u\n" % self.memory)

    def print_diff(self, prev):
        print("Tasklet-name: %s" % (self.name))
        found_diff = False
        for digest in set(self.allocations.keys()) | set(prev.allocations.keys()) :
            count = 0
            found_diff = False
            addresses_str = ''
            tasklet = None
            try:
                new_set = set(self.allocations[digest][1])
                tasklet = self
            except KeyError:
                new_set = set()

            try:
                old_set = set(prev.allocations[digest][1])
                if tasklet is None:
                    tasklet = prev
            except KeyError:
                old_set = set()

            #print(new_set)
            #print(old_set)
            for addr_i in  list(new_set - old_set):
                found_diff = True
                addresses_str +=  ['\n      >>', ' '][int(count%7!=0)] + hex(addr_i).rstrip("L")
                count+=1
            count = 0
            for addr_i in list(old_set - new_set):
                found_diff = True
                addresses_str +=  ['\n      <<', ' '][int(count%7!=0)] + hex(addr_i).rstrip("L")
                count+=1

            if found_diff:
                self.print_callers(tasklet.allocations[digest][0])
                print("    address-list: (prev.count:%u, new.count:%u)" % (
                        len(old_set), len(new_set)))
                print(addresses_str)
                print("\n")
        print("  prev total memory - allocated: %-10u chunks: %-10u" % prev.memory)
        print("  new  total memory - allocated: %-10u chunks: %-10u" % self.memory)
        print('\n')


class RwTaskletHeap(object):
    """Helper class for parsing tasklet heap
    This class is used during the pretty printing of tasklet heap"""
    prev_tasklets = []

    def __init__(self):
        self.tasklets = []
        self.heap_found = False

    def parse(self, pp, root):
        for child in root:
            # strip name space if it is present in the tag
            tag = pp.strip_tag_namespace(child.tag)

            if len(child) > 0:
                if tag == "heap":
                    self.heap_found = True
                    self.parse(pp, child)
                elif self.heap_found and tag == "tasklet":
                    tasklet = RwTaskletHeapTasklet()
                    self.tasklets.append(tasklet)
                    tasklet.parse(pp, child)
                else:
                    self.parse(pp, child)

    def print(self):
        for tasklet in self.tasklets:
            tasklet.print()

    def print_diff(self):
        prev_tasklets = RwTaskletHeap.prev_tasklets
        RwTaskletHeap.prev_tasklets = self.tasklets
        if len(prev_tasklets) != 0:
            this_t = {}
            prev_t = {}
            if len(self.tasklets) != len(prev_tasklets):
                self.print()
                return

            print("Difference from last run\n")

            for t in self.tasklets:
                this_t[t.name] = t
            for t in prev_tasklets:
                prev_t[t.name] = t
            for t_name in set(this_t.keys()) | set(prev_t.keys()):
                try:
                    this_t[t_name].print_diff(prev_t[t_name])
                except KeyError:
                    try:
                        this_t[t_name].print()
                    except KeyError:
                        pass
        else:
            self.print()


class DebugHeap(object):
    """Helper class for parsing debug heap
    This class is used during the pretty printing of debug heap"""

    prev_tasklets = {}
    DHTasklet = namedtuple('DHTasklet', 'memory allocations')

    def __init__(self, model):
        self._model = model

    def _print_callers(caller_info):
       print("  allocation:\n",
             "   size: %-12u type: %s"
             %(caller_info[1], caller_info[0]))
       for i, caller in enumerate(caller_info[2]):
           print("    caller%u: %s" %(i, caller.info))

    def _print_this(name, this):
        print('Tasklet-name: ', name)
        allocations = this.allocations
        for digest in allocations.keys():
            DebugHeap._print_callers(this.allocations[digest][0])
            """
            print("  allocation:\n",
                  "   size: %-12u type: %s"
                  %(allocations[digest][0][1], allocations[digest][0][0]))
            for i, caller in enumerate(allocations[digest][0][2]):
                print("    caller%u: %s" %(i, caller.info))
            """
            addresses_str = ''
            for count, address in enumerate(allocations[digest][1]):
                addresses_str +=  ['\n      ', ' '][int(count%7!=0)] + hex(address).rstrip("L")
            print("    address-list: (count:%u)" % len(allocations[digest][1]))
            print(addresses_str)
            print("\n")

        print("  total memory - allocated: %-10u chunks: %-10u" % (this.memory.allocated, this.memory.chunks))
        print('\n')


    def _print_diff(name, this, prev):
        print('Tasklet-name: ', name)
        this_allocations = this.allocations
        prev_allocations = prev.allocations
        for digest in set(this.allocations.keys()) | set(prev.allocations.keys()):
            count = 0
            found_diff = False
            addresses_str = ''
            tasklet = None
            try:
                new_set = set(this.allocations[digest][1])
                tasklet = this
            except KeyError:
                new_set = set()

            try:
                old_set = set(prev.allocations[digest][1])
                if tasklet is None:
                    tasklet = prev
            except KeyError:
                old_set = set()

            for addr_i in  list(new_set - old_set):
                found_diff = True
                addresses_str +=  ['\n      >>', ' '][int(count%7!=0)] + hex(addr_i).rstrip("L")
                count+=1
            count = 0
            for addr_i in list(old_set - new_set):
                found_diff = True
                addresses_str +=  ['\n      <<', ' '][int(count%7!=0)] + hex(addr_i).rstrip("L")
                count+=1

            if found_diff:
                DebugHeap._print_callers(tasklet.allocations[digest][0])
                print("    address-list: (prev.count:%u, new.count:%u)"
                      %(len(old_set), len(new_set)))
                print(addresses_str)
                print('\n')

        print("  prev total memory - allocated: %-10u chunks: %-10u" % (prev.memory.allocated, prev.memory.chunks))
        print("  new  total memory - allocated: %-10u chunks: %-10u" % (this.memory.allocated, this.memory.chunks))
        print('\n')

    def print(self, xml):
        heap = rwdebug.YangData_RwDebug_RwDebug_Heap()
        try:
          heap.from_xml_v2(self._model, xml)
        except glib.Error:
          return

        tasklets = {}
        prev_tasklets = DebugHeap.prev_tasklets

        for tasklet in heap.tasklet:

            allocations = {}
            for allocation in tasklet.allocation:
                digest_string = str(allocation.size) + allocation.type_yang
                for caller in allocation.caller:
                    digest_string += caller.info
                digest = hashlib.md5(digest_string.encode()).hexdigest()
                try:
                    allocations[digest][1].append(allocation.address)
                except KeyError:
                    allocations[digest] = tuple((tuple((allocation.type_yang, allocation.size, allocation.caller)), [allocation.address]))

            tasklets[tasklet.name] = DebugHeap.DHTasklet(tasklet.memory, allocations)

            try:
                DebugHeap._print_diff(tasklet.name, tasklets[tasklet.name], prev_tasklets[tasklet.name])
            except KeyError:
                DebugHeap._print_this(tasklet.name, tasklets[tasklet.name])

        DebugHeap.prev_tasklets = tasklets

class VcsInstance(object):
    def __init__(self, model):
        self._model = model

    def print(self, xml):
        info = rwbase.VcsInstance()
        try:
          info.from_xml_v2(self._model, xml)
        except glib.Error:
          return

        for instance in info.instance:
            print(instance.instance_name, "Admin-Stop:", instance.admin_stop)
            for child in instance.child_n:
                print("  child: instance-name: %s component-type: %s admin-command: %s state: %s config-ready:%d"
                      %(child.instance_name, child.component_type, child.admin_command, child.publish_state.state, child.publish_state.config_ready))
        print()

        #raise ValueError('JUST LIKE THAT')

class MessagingDebugInfo(object):
    def __init__(self, model):
        self._model = model

    def print(self, xml):
        info = rwmsg.YangData_RwmsgData_Messaging_DebugInfo()
        try:
          info.from_xml_v2(self._model, xml)
        except glib.Error:
          return

        brokers = []

        for broker in info.broker:
            print ('BROKER: tasklet-name:  ', broker.tasklet_name)
            for channel in broker.channels:
                channel_str = '  CHANNEL:  [channel %s%u]' %({'BROCLI':'C', 'BROSRV':'S', 'PEERCLI':'p', 'PEERSRV':'P'}[channel.chan_type], channel.chan_id)
                stats = None
                stats_str = ''
                recv_tot = 0
                bnc_tot = 0
                ack_tot = 0
                ok_count = 0

                if channel.has_field('clichan_stats'):
                    stats = channel.clichan_stats
                elif channel.has_field('srvchan_stats') :
                    stats = channel.srvchan_stats

                if stats is not None and any(stats.has_field(field) for field in stats.fields):
                    bnc_print = ''
                    for attr in stats.fields:
                        if attr != 'bnc' and getattr(stats, attr):
                            stats_str += '\n      %s: %u' %(attr, getattr(stats, attr))
                            if attr == 'recv':
                                recv_tot = getattr(stats, attr)
                            elif attr[:3] == 'ack':
                                ack_tot += getattr(stats, attr)
                        elif attr == 'bnc':
                            for bnc in stats.bnc:
                                if bnc.count:
                                    if bnc_print != '':
                                        bnc_print += ', '
                                    bnc_print +=  "'%s':%u" %(bnc.bnc_type, bnc.count)
                                    if bnc.bnc_type == 'OK':
                                        ok_count = bnc.count
                                    bnc_tot += bnc.count
                    if bnc_print != '':
                        stats_str += '\n      BOUNCEs: {%s}' %(bnc_print)

                if stats_str != '':
                    print (channel_str,
                           'req:%u!=rsp:%u' %(recv_tot, bnc_tot) if (recv_tot!=bnc_tot and channel.chan_type=='BROCLI') else '',
                           'ok:%u!=ack:%u' %(ok_count, ack_tot) if (ok_count!=ack_tot and channel.chan_type=='BROCLI') else '',
                           stats_str)
            print ('\n')

class ShowSystemInfoPrinter(object):
    def __init__(self):
        self.string = ""

    @staticmethod
    def strip_tag_namespace(tag):
        if tag.startswith('{'):
            try:
                return tag.split('}', 1)[1]
            except IndexError:
                pass
        return tag

    def _print_rsp_string(self, root):
        exclude_from_print = ["response", "data", "ReturnStatus", "ok"]
        for child in root:
            # strip name space if it is present in the tag
            tag = self.strip_tag_namespace(child.tag)
            if tag not in exclude_from_print:
               print(child.text)
        return

    def print(self, xml):
        try:
            root = ET.fromstring(xml)
            for child in root:
               tag = self.strip_tag_namespace(child.tag)
               if tag == "rpc-error":
                  print(xml)
                  return
            self._print_rsp_string(root)
        except Exception as err:
            sys.stderr.write('ERROR: %s\n' % str(err))
            print("BAD XML")
            print(xml)
        return

class TaskletInfoPrinter(object):
    def __init__(self, model):
        self._model = model

    def print(self, xml):
        info = rwbase.VcsInfo()
        info.from_xml_v2(self._model, xml)

        root = None
        components = {}

        for component in info.components.component_info:
            if component.rwcomponent_parent is None:
                root = component
            components[component.instance_name] = component

        if root is None:
            raise ValueError('No root component found in the tasklet info tree')

        # Workaround RIFT-6621 - Keep track of the components we've already printed.
        printed = []
        self.print_components(root, printed, components)


    @staticmethod
    def print_components(component, printed, components, indent=''):
        if component.instance_name in printed:
            return

        if component.component_type == 'RWCOLLECTION':
            print("%s COLLECTION[%s]:: %s%s" % (
                   indent,
                   component.collection_info.collection_type,
                   component.instance_name,
                   ' (%s)' % component.state if component.state != 'RUNNING' else ''))
        elif component.component_type == "RWVM":
            if component.vm_info.vm_state == 'MGMTACTIVE':
                vm_state = '<active>'
            elif component.vm_info.vm_state == 'MGMTSTANDBY':
                vm_state = '<standby>'
            else :
                vm_state = ''
            print("%s VM:: %s [%s %d]%s%s%s" % (
                    indent,
                    component.instance_name,
                    component.vm_info.vm_ip_address,
                    component.vm_info.pid,
                    '%s' % ' <leader>' if component.vm_info.leader else '',
                    '%s' % str(vm_state),
                    ' (%s)' % component.state if component.state != 'RUNNING' else ''))
        elif component.component_type in ('RWPROC', 'PROC'):
            print("%s %s:: %s [%d]%s" % (
                    indent,
                    component.component_type,
                    component.instance_name,
                    component.proc_info.pid,
                    ' (%s)' % component.state if component.state != 'RUNNING' else ''))
        else:
            print("%s %s:: %s%s" % (
                    indent,
                    component.component_type.lstrip('RW'),
                    component.instance_name,
                    ' (%s)' % component.state if component.state != 'RUNNING' else ''))

        printed.append(component.instance_name)

        def type_key(a):
            types = ['RWTASKLET', 'RWPROC', 'PROC', 'RWVM', 'RWCOLLECTION']
            return types.index(components[a].component_type)

        try:
            for child in sorted(component.rwcomponent_children, key=type_key):
                TaskletInfoPrinter.print_components(components[child], printed, components, indent+'    ')
                if indent == '':
                    print('')
        except KeyError:
            raise KeyError('component with missing data found')


class RwMemlogInstancePrinter(object):
    def __init__(self, model):
        self._model = model

    def print(self, xml):
        state = rwmemlog.MemlogState()
        state.from_xml_v2(self._model, xml)

        buffers = {}

        for tasklet in state.tasklet:
            for instance in tasklet.instance:
                for buffer in instance.buffer:
                    # Skip empty buffers
                    if buffer.time == 0:
                        continue

                    time_s = buffer.time / 1000000000
                    time_ns = buffer.time % 1000000000

                    key = "%s:%s:%s:%d @ %u.%9.9u" % (
                        tasklet.tasklet_name,
                        instance.instance_name,
                        buffer.object_name,
                        buffer.object_id,
                        time_s,
                        time_ns )

                    buffers[key] = buffer

        for key,buffer in sorted(buffers.items()):
            extra = ""
            time_s = buffer.time / 1000000000
            time_ns = buffer.time % 1000000000
            time_us = time_ns / 1000
            time_str = time.strftime("%FT%T", time.gmtime(time_s))
            time_str += ".%06u" % (time_us)

            if buffer.is_active:
                extra += "(Active %s)" % (time_str)
            else:
                extra += "(Retired %s)" % (time_str)
            if buffer.is_keep:
                extra += " (Keep)"

            print( "%s: %s" % (key, extra) )
            for entry in buffer.entry:
                file = entry.file
                file = re.sub(r'.*?\/', '', file)
                string = "%s:%u:" % (file, entry.line)
                if entry.has_field("arg0"):
                    string += " %s" % (entry.arg0)
                if entry.has_field("arg1"):
                    string += ", %s" % (entry.arg1)
                if entry.has_field("arg2"):
                    string += ", %s" % (entry.arg2)
                if entry.has_field("arg3"):
                    string += ", %s" % (entry.arg3)
                if entry.has_field("arg4"):
                    string += ", %s" % (entry.arg4)
                if entry.has_field("arg5"):
                    string += ", %s" % (entry.arg5)
                if entry.has_field("arg6"):
                    string += ", %s" % (entry.arg6)
                if entry.has_field("arg7"):
                    string += ", %s" % (entry.arg7)
                if entry.has_field("arg8"):
                    string += ", %s" % (entry.arg8)
                if entry.has_field("arg9"):
                    string += ", %s" % (entry.arg9)

                print( "    %s" % (string) )

            print('')

class PrettyPrinter(object):
    # Class instance as the model is not properly ref-counted
    _rwbase_model = None
    _rwdebug_model = None
    _rwmsg_model = None
    _rwmemlog_model = None

    def __new__(cls, *args, **kwds):
        if cls._rwbase_model is None:
            cls._rwbase_model = rwyang.Model.create_libncx()
            cls._rwbase_model.load_schema_ypbc(rwbase.get_schema())
        if cls._rwdebug_model is None:
            cls._rwdebug_model = rwyang.Model.create_libncx()
            cls._rwdebug_model.load_schema_ypbc(rwdebug.get_schema())
        if cls._rwmsg_model is None:
            cls._rwmsg_model = rwyang.Model.create_libncx()
            cls._rwmsg_model.load_schema_ypbc(rwmsg.get_schema())
        if cls._rwmemlog_model is None:
            cls._rwmemlog_model = rwyang.Model.create_libncx()
            cls._rwmemlog_model.load_schema_ypbc(rwmemlog.get_schema())
        return super(PrettyPrinter, cls).__new__(cls, *args, **kwds)

    """Pretty printer class for the CAL and other components"""
    def __init__(self):
        self._tasklet_info_printer = TaskletInfoPrinter(PrettyPrinter._rwbase_model)
        self._debug_heap_printer = DebugHeap(PrettyPrinter._rwdebug_model)
        self._messaging_debug_info_printer = MessagingDebugInfo(PrettyPrinter._rwmsg_model)
        self._memlog_instance_printer = RwMemlogInstancePrinter(PrettyPrinter._rwmemlog_model)
        self._vcs_instsnce_printer = VcsInstance(PrettyPrinter._rwbase_model)

    @staticmethod
    def wrap_xml_with_root_node(xml_str):
        """This function wraps the xml string under a root node
        (<data></data>). This is helpful when the original xml string
        has multiple nodes, but not enclosed in a top level node"""
        match = re.match(r'(.*version.*?>)(.*)', xml_str, re.M|re.I|re.S)
        if match:
            return match.group(1) + "<data>" + match.group(2) + "</data>"
        else:
            return xml_str

    @staticmethod
    def strip_tag_namespace(tag):
        if tag.startswith('{'):
            try:
                return tag.split('}', 1)[1]
            except IndexError:
                pass

        return tag

    def print_tasklet_info(self, xml_str):
        try:
            self._tasklet_info_printer.print(xml_str)
        except (ValueError, KeyError, glib.Error) as e:
            self.print_xml_str(xml_str)
            raise

    def print_messaging_debug_info(self, xml_str):
        try:
            self._messaging_debug_info_printer.print(xml_str)
            #self.default_print(xml_str)
        except:
            self.print_xml_str(xml_str)
            raise

    def print_rwmemlog_state(self, xml_str):
        try:
            self._memlog_instance_printer.print(xml_str)
        except:
            self.print_xml_str(xml_str)
            raise

    def print_vcs_instance(self, xml_str):
        try:
            self._vcs_instsnce_printer.print(xml_str)
            #self.default_print(xml_str)
        except:
            self.print_xml_str(xml_str)
            raise

    def print_messaging_info(self, xml_str):
        root = ET.fromstring(xml_str)
        messaging = RwMsgMessaging()
        messaging.parse(self, root)
        messaging.print()

    def print_show_system_info(self, xml_str):
        ssi = ShowSystemInfoPrinter()
        ssi.print(xml_str)

    #def print_tasklet_heap(self, xml_str):
    def print_debug_heap(self, xml_str):
        try:
            self._debug_heap_printer.print(xml_str)
            #self.default_print(xml_str)
        except:
            self.print_xml_str(xml_str)
            raise

    def print_tasklet_heap(self, xml_str):
    #def print_debug_heap(self, xml_str):
        try:
            root = ET.fromstring(xml_str)
            heap = RwTaskletHeap()
            heap.parse(self, root)
            heap.print_diff()
        except:
            self.print_xml_str(xml_str)
            raise

    def print_xml_tree(self, root, indent):
        exclude_from_print = ["response", "data", "ReturnStatus", "ok"]
        for child in root:
            # strip name space if it is present in the tag
            tag = self.strip_tag_namespace(child.tag)

            if len(child) > 0:
                if tag not in exclude_from_print:
                    print("\n%s%s:" % (indent, tag))
                self.print_xml_tree(child, indent+"    ")
            else:
                if tag not in exclude_from_print:
                    print("%s%-24s:%s" % (indent, tag, child.text))
        return

    def print_xml_str(self, xml_str):
        try:
            root = ET.fromstring(xml_str)
            self.print_xml_tree(root, "")
        except Exception as err:
            sys.stderr.write('ERROR: %s\n' % str(err))
            print("BAD XML")
            print(xml_str)
        return

    def default_print(self, xml_str):
        """This is a default print routine."""
        # wrap the xml with root node
        # needed if the root node is missing
        # xml_str = self.wrap_xml_with_root_node(xml_str)
        self.print_xml_str(xml_str)
        return

    def pretty_print(self, xml_str, pretty_print_routine):
        # Check if the class has the print function
        if hasattr(self.__class__, pretty_print_routine):
            func = getattr(self.__class__, pretty_print_routine)
            if callable(func):
                func(self, xml_str)
                return

        # The pretty format function is not available
        # in either the class or globals, use default print function
        self.default_print(xml_str)

# vim: ts=4 et sw=4
