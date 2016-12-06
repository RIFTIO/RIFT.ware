#!/usr/bin/env python3
"""
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

@file rwdts-pytest.py
@author Sameer Nayak (sameer.nayak@riftio.com)
@date 06/28/2015
"""

import argparse
import logging
import os
import sys
import unittest
import xmlrunner
import socket
import tempfile
import re
import time
import weakref
import datetime

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('CF', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwDtsToyTaskletYang', '1.0')
gi.require_version('RwMain', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwTasklet', '1.0')
gi.require_version('RwTaskletPlugin', '1.0')
gi.require_version('RwDtsYang', '1.0')
gi.require_version('RwTypes', '1.0')

from gi.repository import RwDts
import gi.repository.RwBaseYang as rwbase
import gi.repository.RwDtsToyTaskletYang as toyyang
import gi.repository.rwlib as rwlib
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
from gi.repository import CF, RwTasklet, RwDts, RwDtsToyTaskletYang
from gi.repository import RwDtsYang
from gi.repository import GObject, RwTaskletPlugin
from gi.repository import RwBaseYang, RwTypes

"""
Test Strategy
"""
"""
Class: RwDtsScheduler
TBD:
RwDtsTestScheduler
Scheduler will have state machine
INIT : this state is pre DTS initialization
       Once the dts init state called event is received state moves to READY
READY: This is when a test can be loaded . Once test_loaded event is called it moves to RUNNING(NOT_READY)
RUNNING: This is when a test is running. There are two timers.
       a> Keep Alive timer. This is to schedule events in  the test cases.
       b> Abort timer. This timer is restarted when state moves to RUNNING .
          If the timer expires test_aborted event is called.
       Each Test has to explicitly call the exit_now() to successfully end the test case
"""

"""
Class: TestRwDts
"""
logger = logging.getLogger(__name__)
class TestRwDts(unittest.TestCase):
    @classmethod
    def setUpClass(self):
      logger.debug("setUpClass")
      rwmsg_broker_port = 0

      sock = socket.socket()
      # Get an available port from OS and pass it on to broker.
      sock.bind(('', 0))
      port = sock.getsockname()[1]
      sock.close()
      rwmsg_broker_port = port
      os.environ['RWMSG_BROKER_PORT'] = str(rwmsg_broker_port)

      logger.debug(os.getpid())
      logger.debug(os.getuid())
      logger.info("Message Broker using port %s" %os.environ.get('RWMSG_BROKER_PORT'))
      msgbroker_dir = os.environ.get('MESSAGE_BROKER_DIR')
      router_dir = os.environ.get('ROUTER_DIR')
      manifest = rwmanifest.Manifest()
      manifest.init_phase.settings.rwdtsrouter.single_dtsrouter.enable = True

      self.rwmain = rwmain.Gi.new(manifest)
      self.taskletinfo = self.rwmain.get_tasklet_info()
      self.rwmain.add_tasklet(msgbroker_dir, 'rwmsgbroker-c')
      self.rwmain.add_tasklet(router_dir, 'rwdtsrouter-c')

      # Setup the local Scheduler
      self.setUpScheduler(self)

      # start all members
      self.pub = Publisher(self)
      self.sub = Subscriber(self)
      self.agent = Agent(self)
      self.sched.wait_event("component-dts-started Agent")

      #start the tests
      self.sched.begin_testing()


    def setUpScheduler(self):
      logger.debug("setUpScheduler")
      instance_url = RwTaskletPlugin._RWExecURL()
      logger.debug("main: url=%s type=%s" % (instance_url, type(instance_url)))

      sched = RwDtsScheduler(self)
      component_handle = sched.component_init()
      sched_instance_handle = sched.instance_alloc(component_handle, self.taskletinfo, instance_url)
      sched.instance_start(component_handle, sched_instance_handle)
      self.sched = sched
      logger.debug("main: component=%s type=%s" % (component_handle, type(component_handle)))

    def setUp(self):
      logger.info("Test: %s "%self._testMethodName)
      self.sched.test_start()
      self.sched.passed = 0
      self.sched.aborted = 0
      self.sub.validate = None
      self.pub.validate = None

    def tearDown(self):
      logger.debug("Test teardown ")
      self.sched.test_stop()
      try:
        self.pub.cleanup()
        self.sub.cleanup()
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])
      logger.debug("}Test Stop at sched event")
      self.assertEqual(self.sched.passed,1)

    def exit_now(self, passed=1):
      logger.debug("exit_now %d"%passed)
      if passed:
        self.sched.passed = passed
      else:
        self.sched.aborted = 1
        print("abort, exiting\n")
        exit(1)

    @classmethod
    def tearDownClass(self):
      logger.debug("Done")
    """
    Add the test cases here
    """
    def test_pub_reg_ready(self):
      logger.debug("Test: test_pub_reg_ready")
      self.pub.register(self)
      self.pub.validate = "pub_reg_ready"

    def test_sub_reg_ready(self):
      logger.debug("Test: test_sub_reg_ready")
      self.sub.register(self)
      self.sub.validate = "sub_reg_ready"

    def test_read_query(self):
      """
      Step: Pub register
      Step: Pub Create Object
      Step: Sub Register
      Step: Sub read request create
      Validate: prepare response
      """
      try:
        logger.debug("Test: test_read_query")
        self.pub.register(self)
        self.sched.wait_event("pub-default-reg-done")
        self.sub.register(self)
        self.sched.wait_event("sub-default-reg-done")
        self.pub.create_data()
        self.sub.read_request()
        self.sub.validate = "read_query_response"
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])

    def test_pub_create_incorrect_entry(self):
      """
      Step: Pub register, key = Intel
      Step: Pub Create Object, key = intel
      Step: Pub Read list,
      Validate: No entry should be present

      JIRA: Rift -
      Read with Incorrrect entry is not allowed.
      Ideally create_entry should be prevented
      """
      try:
        logger.debug("Test: test_pub_create_incorrect_entry")
        self.pub.register(self)
        self.sched.wait_event("pub-default-reg-done")
        self.pub.create_incorrect_data()
        self.pub.validate = "pub_read_empty_list"
        self.pub.read_data()
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])

    def test_pub_create_multiple_entries(self):
      """
      Step: Pub register, key = Intel
      Step: Pub Create Object, key = intel
      Step: Pub Read list,
      Validate: 10 entries should be read
      """
      try:
        logger.debug("Test: test_pub_create_multiple_entries")
        xpath = ("C,/rw-dts-toy-tasklet:toytasklet-config"
                "/rw-dts-toy-tasklet:company")
        self.pub.register(self, xpath)
        self.sched.wait_event("pub-default-reg-done")
        self.pub.create_data_ks(10)
        self.pub.validate = "pub_read_list_entries10"
        self.pub.read_data()
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])

    def test_sub_multiple_entries_read(self):
      """
      Step: Pub register, key = Intel
      Step: Pub Create Object, key = intel
      Step: Sub Read list,
      Validate: Sub should read 10 entries
      """
      try:
        logger.debug("Test: test_sub_multiple_entries_read")
        xpath = ("C,/rw-dts-toy-tasklet:toytasklet-config"
                "/rw-dts-toy-tasklet:company")
        self.pub.register(self, xpath)
        self.sched.wait_event("pub-default-reg-done")
        self.sub.register(self, xpath)
        #self.sched.wait_event("sub-default-reg-done")
        self.pub.create_data_ks(10)
        self.sub.validate = "sub_read_list_entries10"
        self.sub.read_request(xpath)
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])

    def test_pub_empty_response(self):
      """
      Refer: rwdts/plugins/yang/rw-dts-toy-tasklet.yang
      Step: Pub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
      Step: Sub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
      Step: Sub read same xpath
      Validate: either zero stats or stats object
      """
      try:
        logger.debug("Test: JIRA-B-30 ")
        xpath = ("C,/rw-dts-toy-tasklet:stats-data")
        self.pub.register(self, xpath,
                          RwDts.Flag.PUBLISHER, self.pub.pub_reg_ready,
                          self.pub.pub_stats_prepare_cb)
        self.sched.wait_event("pub-default-reg-done")
        self.sub.register(self, xpath)
        self.sched.wait_event("sub-default-reg-done")
        self.sub.validate = "read_stats_response"
        self.sub.read_stats_request(xpath)
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])

    def test_block_response(self):
      """
      Description here:
      """
      try:
        logger.debug("Test: JIRA-Block ")
        self.agent.read_block_request()
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])


    def test_show_dts(self):
       """
       Step: Pub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
       Step: Sub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
       Step: Agent read "D,/rwdts:dts"
       Validate: 'show dts' command read response
       """
       logger.debug("Test: show dts member")
       xpath = ("C,/rw-dts-toy-tasklet:stats-data")
       self.pub.register(self, xpath)
       self.sched.wait_event("pub-default-reg-done")

       self.sub.register(self, xpath)
       self.sched.wait_event("sub-default-reg-done")

       self.sub.validate = "show_dts_results"
       self.agent.show_dts()

    def test_show_dts_router(self):
       """
       Step: Pub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
       Step: Sub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
       Step: Agent read "D,/rwdts:dts/rwdts:routers"
       Validate: 'show dts routers' command read response
       """
       logger.debug("Test: show dts router")
       xpath = ("C,/rw-dts-toy-tasklet:stats-data")
       self.pub.register(self, xpath)
       self.sched.wait_event("pub-default-reg-done")

       self.sub.register(self, xpath)
       self.sched.wait_event("sub-default-reg-done")

       self.sub.validate = "show_dts_routers_results"
       self.agent.show_dts_router()

    def test_test_big_read(self):
       """
       Step: Pub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
       Step: Pub Prepare Callback has a BIG read
       Step: Sub read "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
       Validate: read response having a big response
       """
       logger.debug("Test: test_test_big_read")
       xpath = ("C,/rw-dts-toy-tasklet:stats-data")
       self.pub.register(self, xpath,
                         RwDts.Flag.PUBLISHER, self.pub.pub_reg_ready,
                         self.pub.pub_bigpayload_prepare_cb)
       self.sched.wait_event("pub-default-reg-done")
       self.sub.read_bigpayload_request(xpath)

    def test_test_multiple_queries(self):
       try:
         """
         Step: Pub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
         Step: Sub register, key = "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
         Step: Sub xact create "C,/rw-dts-toy-tasklet:toytasklet-stats-data"
         Step: Sub block create
         Step: Sub add 10 queries
         Step: Sub execute
         Validate: Sub readresponse validate 10 corrid
         """
         logger.debug("Test: test_multiple_queries")
         xpath = ("C,/rw-dts-toy-tasklet:stats-data")
         self.pub.register(self, xpath,
                           RwDts.Flag.PUBLISHER, self.pub.pub_reg_ready,
                           self.pub.pub_stats_prepare_cb)
         self.sched.wait_event("pub-default-reg-done")
         self.sub.register(self, xpath)
         self.sched.wait_event("sub-default-reg-done")
         self.sub.validate = "response-123456789"

         xact = self.sub.apih.xact_create(0, None, None)
         blk = xact.block_create()
         for i in range (1,10):
           qcorrid = i
           status = blk.add_query(xpath, RwDts.QueryAction.READ, RwDts.XactFlag.STREAM, qcorrid, None)
           logger.debug("Added Block %d"%status)

         status = blk.execute(0, self.sub.read_query_response, self)
       except:
         logger.debug("Unexpected error:", sys.exc_info()[0])


class Agent(object):
    def dts_state_change_cb(self, apih, state, user_data):
      logger.debug("dts_state_change_cb : AGENT inited")
      self.test.sched.post_event("component-dts-started Agent")
      #self.test.exit_now(self.test)

    def __init__(self, test):
      logger.debug("Agent INIT")
      self.test = test
      self.ti = test.rwmain.new_tasklet_info("AGENT", 0)
      self.apih = RwDts.Api.new(self.ti,
                                RwDtsYang.get_schema(),
                                self.dts_state_change_cb,
                                self)
    """
    "show dts" request
    """
    def show_dts(self):
       try:
         xpath = ("D,/rwdts:dts/rwdts:member")
         logger.debug ("issued : show dts member%s" %xpath)
         xact = self.apih.query(xpath,
                            RwDts.QueryAction.READ,
                            0,
                            self.show_dts_resp,
                            self)
       except:
         logger.debug("Unexpected error:", sys.exc_info()[0])

    """
    "show dts" response
    """
    def show_dts_resp(self, xact, xact_status, user_data):
       try:
         logger.debug ("show_dts_resp")
         query_result = xact.query_result()
         resp_count = 0
         while query_result is not  None:
           resp_count = resp_count+1
           pbcm = query_result.protobuf
           dts_data = RwDtsYang.MemberInfo.from_pbcm(pbcm)
           query_result = xact.query_result()
           logger.debug ("dts response. Put some validation%s"%dts_data)

         if resp_count:
           self.test.exit_now(self.test)
         else:
           self.test.exit_now(self.test, passed = 0)
       except:
         logger.debug("Unexpected error:", sys.exc_info()[0])

    """
    "show dts routers" request
    """
    def show_dts_router(self):
       try:
         xpath = ("D,/rwdts:dts/rwdts:routers")
         logger.debug ("issued : show dts router xpath:%s" %xpath)
         xact = self.apih.query(xpath,
                            RwDts.QueryAction.READ,
                            0,
                            self.show_dts_router_resp,
                            self)
       except:
         logger.debug("Unexpected error:", sys.exc_info()[0])

    """
    "show dts routers" response
    """
    def show_dts_router_resp(self, xact, xact_status, user_data):
       try:
         logger.debug ("show_dts_router_resp")
         query_result = xact.query_result()
         resp_count = 0
         while query_result is not  None:
           resp_count = resp_count+1
           pbcm = query_result.protobuf
           dts_data = RwDtsYang.Routerinfo.from_pbcm(pbcm)
           query_result = xact.query_result()
           logger.debug ("dts router response. Put some validation%s"%dts_data)

         if resp_count:
           self.test.exit_now(self.test)
         else:
           self.test.exit_now(self.test, passed = 0)
       except:
         logger.debug("Unexpected error:", sys.exc_info()[0])

    def show_dts_block_resp(self, xact, xact_status, user_data):
       self.block_rsp_count += 1
       try:
         logger.info ("show_dts_block_resp")
         query_result = xact.query_result()
         resp_count = 0
         while query_result is not  None:
           resp_count = resp_count+1
           pbcm = query_result.protobuf
           dts_data = RwDtsYang.MemberInfo.from_pbcm(pbcm)
           query_result = xact.query_result()
           logger.debug ("dts response. Put some validation%s"%dts_data)

         if (self.block_rsp_count == 2) :
           self.test.exit_now(self.test)

       except:
         logger.debug("Unexpected error:", sys.exc_info()[0])

    def read_block_request(self):
      self.block_rsp_count = 0
      xact = self.apih.xact_create(0, None, None)
      xact.trace()
      block = xact.block_create()
      block.add_query("D,/rwdts:dts/rwdts:routers", RwDts.QueryAction.READ, 0,
                      1234, None)
      logger.debug("read_block_request .......D,/rwdts:dts/rwdts:routers");
      block.execute(0, self.show_dts_block_resp, self, None)
      block1 = xact.block_create()
      block1.add_query("D,/rwdts:dts/rwdts:member", RwDts.QueryAction.READ, 0,
                       3456, None)
      logger.debug("read_block_request .......D,/rwdts:dts/rwdts:member");
      block1.execute(0, self.show_dts_block_resp, self, None)
      xact.commit()

class Publisher(object):
    def dts_state_change_cb(self, apih, state, user_data):
      logger.debug("dts_state_change_cb : Publisher inited")
      #self.test.sched.post_event("component-dts-started")
      logger.debug("5 SET")
      self.test.exit_now(self.test)

    def pub_stats_prepare_cb(self, xact_info, action, keyspec, msg, user_data):
      logger.debug("pub_stats_prepare_cb%s"%dir(RwDtsToyTaskletYang.StatsData))
      xpath = 'C,/rw-dts-toy-tasklet:stats-data'
      stats = RwDtsToyTaskletYang.StatsData.new()
      xact_info.respond_xpath(RwDts.XactRspCode.ACK,
                              xpath,
                              stats.to_pbcm())
      return (RwDts.MemberRspCode.ACTION_OK);

    def pub_bigpayload_prepare_cb(self, xact_info, action, keyspec, msg, user_data):
      logger.info("pub_stats_prepare_cb")
      xpath = 'C,/rw-dts-toy-tasklet:stats-data'
      #Rift-8338
      #toggle the commented lines for this JIRA
      #stats = RwDtsToyTaskletYang.StatsData.new()
      #stats.payload = "x"*(1024*1024*8*10)
      #Rift-8338
      xact_info.respond_xpath(RwDts.XactRspCode.ACK,
                              xpath,
                              #stats.to_pbcm())
                              self.stats_pbcm)
      logger.info("pub_stats_prepare_cb Done")
      return (RwDts.MemberRspCode.ACTION_OK);


    def pub_prepare_cb(self, xact_info, action, keyspec, msg, user_data):
      logger.debug("pub_prepare_cb%s"%dir(RwDtsToyTaskletYang.StatsData))
      return (RwDts.MemberRspCode.ACTION_OK);

    def pub_reg_ready(self, regh, rw_status, user_data):
      logger.debug("pub_reg_ready called %s"%self.test)
      self.test.sched.post_event("pub-default-reg-done")
      #self.apih.print_()
      if self.validate == "pub_reg_ready":
        self.test.exit_now(self.test)

    def register(self,
             test,
             xpath = (
              "C,/rw-dts-toy-tasklet:toytasklet-config"
              "/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name='Intel']"
              ),
              flags = RwDts.Flag.PUBLISHER|RwDts.Flag.NO_PREP_READ,
              reg_ready_cb = None,
              prep_cb = None):
      logger.debug("Publisher Register Xpath:%s" % xpath)
      if reg_ready_cb is None:
        reg_ready_cb = self.pub_reg_ready
      if prep_cb is None:
        prep_cb = self.pub_prepare_cb
      status, reg = self.apih.member_register_xpath(xpath,
                                                None,
                                                flags,
                                                RwDts.ShardFlavor.NULL,
                                                0,
                                                0,
                                                -1,
                                                0,
                                                0,
                                                reg_ready_cb,
                                                prep_cb,
                                                None,
                                                None,
                                                None,
                                                self)
      self.default_regh = reg
      self.default_xpath = xpath
      self.default_reg_ready = 0
      logger.debug("registration ->  %s "%self.default_xpath)
      logger.debug("registration OBJECT->  %s "%dir(self.default_regh))

    def cleanup(self):
      if self.default_regh is not None:
        logger.debug("Remove all Registrations %s" % dir(self.default_regh))
        self.default_regh.deregister()
      self.default_regh = None

    def create_incorrect_data(self):
      logger.debug("Publisher create incorrect entry")
      company_entry = RwDtsToyTaskletYang.CompanyConfig.new()
      company_entry.name = 'Rift'
      company_entry.description = 'cloud technologies'
      company_property = company_entry.property.add()
      company_property.name = 'test-property'
      company_property.value = 'test-value'
      #self.apih.trace(self.default_xpath)
      status = self.default_regh.create_element_xpath(self.default_xpath, company_entry.to_pbcm())
      logger.debug("Publishing xpath (%s): %s", self.default_xpath, company_entry)

      company_entry = RwDtsToyTaskletYang.CompanyConfig.new()
      company_entry.name = 'intel'
      company_entry.description = 'cloud technologies'
      company_property = company_entry.property.add()
      company_property.name = 'test-property'
      company_property.value = 'test-value'
      #self.apih.trace(self.default_xpath)
      status = self.default_regh.create_element_xpath(self.default_xpath, company_entry.to_pbcm())
      logger.debug("Publishing xpath (%s): %s", self.default_xpath, company_entry)


    def read_data(self):
      logger.debug("Read_data")
      cursor =  self.default_regh.get_cursor()
      cursor.reset()
      msg, ks = self.default_regh.get_next_element(cursor)
      count = 0

      while msg is not None:
        count = count+1
        company_entry = RwDtsToyTaskletYang.CompanyConfig.from_pbcm(msg)
        logger.debug("Read record using get next api:  %s" %company_entry)
        msg, ks = self.default_regh.get_next_element(cursor)

      logger.debug(" READ data record = %d Validate %s" %(count, self.validate))
      if self.validate == "pub_read_empty_list" and count == 0:
        self.test.exit_now(self.test)
      elif self.validate == "pub_read_list_entries10" and count == 10:
        self.test.exit_now(self.test)
      else:
        self.test.exit_now(self.test, passed=0)
      self.default_regh.delete_cursors()

      
    def create_data_ks(self, entries=1):
      logger.debug("Publisher create entry")
      try:
        for i in range(0,entries):
          company_entry = RwDtsToyTaskletYang.CompanyConfig.new()
          if i:
            company_entry.name = 'Intel%d'%i
          else:
            company_entry.name = 'Intel'
          company_entry.description = 'cloud technologies'
          company_property = company_entry.property.add()
          company_property.name = 'test-property'
          company_property.value = 'test-value'
          xpath = self.default_xpath + "[rw-dts-toy-tasklet:name='" + company_entry.name + "']"
          logger.debug("XPAATH = %s " %xpath)
          #self.apih.trace(self.default_xpath)
          status = self.default_regh.create_element_xpath(xpath, company_entry.to_pbcm())
          logger.debug("Publishing xpath (%s): %s", xpath, company_entry)

        logger.debug("Publisher create entry Done %d"% i)
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])

    # this API is for exact key
    def create_data(self, entries=1):
      logger.debug("Publisher create entry")
      try:
        for i in range(0,entries):
          company_entry = RwDtsToyTaskletYang.CompanyConfig.new()
          company_entry.name = 'Intel'
          company_entry.description = 'cloud technologies'
          company_property = company_entry.property.add()
          company_property.name = 'test-property'
          company_property.value = 'test-value'

          #self.apih.trace(self.default_xpath)
          status = self.default_regh.create_element_xpath(self.default_xpath, company_entry.to_pbcm())
          logger.debug("Publishing xpath (%s): %s", self.default_xpath,company_entry)
        logger.debug("Publisher create entry Done %d"% i)
      except:
        logger.debug("Unexpected error:", sys.exc_info()[0])

    def __init__(self, test):
      logger.debug("Publisher INIT")
      self.test = test
      self.default_regh = None
      self.ti = test.rwmain.new_tasklet_info("PUBLISHER", 0)
      self.apih = RwDts.Api.new(self.ti,
                                RwDtsToyTaskletYang.get_schema(),
                                self.dts_state_change_cb,
                                self)
      #Rift-8338
      self.stats = RwDtsToyTaskletYang.StatsData.new()
      self.stats.payload = "x"*(1024*1024*8*10)
      self.stats_pbcm = self.stats.to_pbcm()
      #Rift-8338

class Subscriber(object):
    ready_count = 0
    def dts_state_change_cb(self, apih, state, user_data):
      logger.info("dts_state_change_cb :Subscriber Inited")
      #self.test.sched.post_event("component-dts-started")
      Subscriber.ready_count = Subscriber.ready_count + 1
      logger.debug("5 SET")
      self.test.exit_now(self.test)

    def __init__(self, test):
      logger.debug("Subscriber INIT")
      self.test = test
      self.default_regh = None
      self.ti = test.rwmain.new_tasklet_info("SUBSCRIBER", Subscriber.ready_count)
      self.apih = RwDts.Api.new(self.ti,
                                RwDtsToyTaskletYang.get_schema(),
                                self.dts_state_change_cb,
                                self)
    def register(self, test, xpath = (
              "C,/rw-dts-toy-tasklet:toytasklet-config"
              "/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name='Intel']"
              )):
      logger.debug("Subscriber Register Xpath:%s" % xpath)
      flags = RwDts.Flag.SUBSCRIBER|RwDts.Flag.CACHE
      status, regh = self.apih.member_register_xpath(xpath,
                                                None,
                                                flags,
                                                RwDts.ShardFlavor.NULL,
                                                0,
                                                0,
                                                -1,
                                                0,
                                                0,
                                                self.sub_reg_ready,
                                                self.sub_prepare_cb,
                                                None,
                                                None,
                                                None,
                                                self)
      self.default_regh = regh
      self.default_xpath = xpath
      #self.apih.print_()

    def read_query_response(self, xact, xact_status, user_data):
     try:
      logger.debug("read_query_response %s"% self.validate)
      query_result = xact.query_result()
      num_results = 0

      response_capture = "response-"
      while (query_result is not None):
        pbcm = query_result.protobuf
        company = RwDtsToyTaskletYang.CompanyConfig.from_pbcm(pbcm)
        if company == None:
          logger.debug("Empty response")
        else:
          logger.debug("Sub Read %s" %company)
        num_results = num_results+1
        response_capture = response_capture+str(query_result.corrid)
        logger.debug("Corrid = %d"%query_result.corrid)
        query_result = xact.query_result()

      logger.info("read_query_response results %s"%response_capture)
      logger.debug("read_query_response results %d"%num_results)

      if self.validate == "sub_read_list_entries10":
        if num_results is not 10:
          self.test.exit_now(self.test, passed=0)
        else:
          self.test.exit_now(self.test)

      if self.validate == "read_query_response":
        if num_results == 0:
          self.test.exit_now(self.test, passed=0)
        else:
          self.test.exit_now(self.test)

      if self.validate == "response-123456789":
        if response_capture == "response-123456789":
          self.test.exit_now(self.test)
        else:
          self.test.exit_now(self.test, passed=0)
     except:
      logger.debug("Unexpected error:", sys.exc_info()[0])

    def read_request(self, xpath = None):
      if xpath is None:
        xpath = self.default_xpath
      else:
        self.apih.trace(xpath)
      logger.debug("read_request .......%s"%xpath)
      xact = self.apih.query(xpath,
                          RwDts.QueryAction.READ,
                          1,
                          self.read_query_response,
                          self)


    def read_stats_response(self, xact, xact_status, user_data):
     try:
      logger.debug("read_stats_response %s"% self.validate)
      query_result = xact.query_result()
      num_results = 0
      while (query_result is not None):
        pbcm = query_result.protobuf
        stats = RwDtsToyTaskletYang.StatsData.from_pbcm(pbcm)
        if stats == None:
          logger.debug("Empty response")
        else:
          logger.debug("Some response%s"%stats.stats3)
        num_results = num_results+1
        query_result = xact.query_result()

      if self.validate == "read_stats_response":
        logger.debug("read_stats_response %s" % query_result)
        if num_results == 0:
          self.test.exit_now(self.test, passed=0)
        else:
          self.test.exit_now(self.test)
     except:
       logger.debug("Unexpected error:", sys.exc_info()[0])

    def read_stats_request(self,xpath = None):
      if xpath is None:
        xpath = self.default_xpath
      logger.debug("read_stats_request .......%s"%xpath)
      xact = self.apih.query(xpath,
                          RwDts.QueryAction.READ,
                          0,
                          self.read_stats_response,
                          self)

    def read_bigpayload_request(self,xpath = None):
      if xpath is None:
        xpath = self.default_xpath
      logger.info("read_bigpayload_request .......%s"%xpath)
      xact = self.apih.query(xpath,
                          RwDts.QueryAction.READ,
                          RwDts.XactFlag.TRACE,
                          self.read_bigpayload_response,
                          self)

    def read_bigpayload_response(self, xact, xact_status, user_data):
     try:
      logger.info("read_bigpayload_response")
      query_result = xact.query_result()
      num_results = 0
      while (query_result is not None):
        stats = RwDtsToyTaskletYang.StatsData.from_pbcm(query_result.protobuf)
        if stats == None:
          logger.debug("Empty response")
        else:
          logger.debug("Some response%s"%stats.stats3)
          #payload_len = len(stats.payload)
          #if payload_len > (1024*1024):
            #logger.info ("Response str of size %d" %payload_len)
            #self.test.exit_now(self.test)
        num_results = num_results+1
        query_result = xact.query_result()
      self.test.exit_now(self.test)
      logger.info("read_bigpayload_response DONE ")
     except:
      logger.debug("Unexpected error:", sys.exc_info()[0])

    def cleanup(self):
      logger.debug("Remove all Registrations")
      if self.default_regh is not None:
        logger.debug("Remove all Registrations %s" % dir(self.default_regh))
        self.default_regh.deregister()
      self.default_regh = None

    def sub_prepare_cb(self, xact_info, action, keyspec, msg, user_data):
      logger.debug("sub_prepare_cb")
      return (RwDts.MemberRspCode.ACTION_OK);

    def sub_reg_ready(self, regh, rw_status, user_data):
      logger.debug("sub_reg_ready")
      self.test.sched.post_event("sub-default-reg-done")
      if self.validate == "sub_reg_ready":
        self.test.exit_now(self.test)

class RwDtsScheduler(GObject.Object, RwTaskletPlugin.Component):
    def begin_testing(self):
       self.taskletinfo.rwsched_tasklet.CFRunLoopTimerSetNextFireDate(
                    self.abort_timer,
                    CF.CFAbsoluteTimeGetCurrent() + 60)
       self.post_event("scheduler-ready")
       self.taskletinfo.rwsched_instance.CFRunLoopRun()

    def test_start(self):
       self.wait_event("scheduler-ready")
       logger.debug("Test start { ")
       self.post_event("test-in-progress")
       self.taskletinfo.rwsched_tasklet.CFRunLoopTimerSetNextFireDate(
                    self.abort_timer,
                    CF.CFAbsoluteTimeGetCurrent() + 60)

    def test_stop(self):
       logger.debug("Stop Loop ")
       self.wait_results()
       self.taskletinfo.rwsched_instance.CFRunLoopStop()
       self.post_event("scheduler-ready")

    def wait_results(self):
       self.post_event("wait_results")
       while 1:
         logger.debug("Looping for results %d"%self.passed)
         if self.passed or self.aborted:
           logger.debug("OUT OF LOOP%d "%self.passed)
           break
         self.taskletinfo.rwsched_instance.CFRunLoopRun()
       logger.debug("WAIT IS OVER")

    def post_event(self, event):
        logger.debug(" Post event %s" %event)
        self.current_event = event
        self.taskletinfo.rwsched_instance.CFRunLoopStop()

    def wait_event(self, event):
        logger.debug("Start wait for Event %s"%event)
        self.pending_event = event
        while 1:
          if self.current_event == event:
            logger.debug("Event Received %s"%event)
            break
          else:
           logger.debug("Waiting for %s Current %s " %(event, self.current_event))
          self.taskletinfo.rwsched_instance.CFRunLoopRun()
        logger.debug("WAIT IS OVER")

    def abort(self, *args):
       logger.error("ABORTING ..........Current %s Waiting %s"%(self.current_event, self.pending_event))
       self.aborted = 1

    def keepalive(self, *args):
       """logger.debug("Ticking......running?%d"%self.running)
       """
       self.taskletinfo.rwsched_instance.CFRunLoopStop()
       logger.debug("....\r")

    def __init__(self, test):
        logger.debug("RwDtsScheduler: __init__ function called")
        GObject.Object.__init__(self)
        self.running  = 0
        self.current_event = "INIT"
        self.test = test
        self.abort_timer = None

    def do_component_init(self):
        """This function is called once during the compoenent
        initialization.
        """
        logger.debug("RwDtsScheduler: do_component_init function called")
        component_handle = RwTaskletPlugin.ComponentHandle()
        return component_handle

    def do_component_deinit(self, component_handle):
        logger.debug("RwDtsScheduler: do_component_deinit function called")

    def do_instance_alloc(self, component_handle, tasklet_info, instance_url):
        """This function is called for each new instance of the tasklet.
        The tasklet specific information such as scheduler instance,
        trace context, logging context are passed in 'tasklet_info' variable.
        This function stores the tasklet information locally.
        """
        logger.debug("RwDtsScheduler: do_instance_alloc function called")
        self.taskletinfo = tasklet_info
        instance_handle = RwTaskletPlugin.InstanceHandle()
        return instance_handle

    def do_instance_free(self, component_handle, instance_handle):
        logger.debug("RwDtsScheduler: do_instance_free function called")

    def do_instance_stop(self, component_handle, instance_handle):
        logger.debug("RwDtsScheduler: do_instance_stop called")

    def do_instance_start(self, component_handle, instance_handle):
        """This function is called to start the tasklet operations.
        Typically DTS initialization is done in this function.
        In this example a periodic timer is added to the tasklet
        scheduler.
        """
        logger.debug("RwDtsScheduler: do_instance_start function called")
        # Create an instance of DTS API - This object is needed by all DTS
        # member and query APIs directly or indirectly.
        # DTS invokes the callback to notify the tasklet that the DTS API instance is ready
        # for use.
        self.sched = self.taskletinfo.rwsched_tasklet.CFRunLoopTimer(
            CF.CFAbsoluteTimeGetCurrent(),
            0.1,
            self.keepalive,
            None)
        self.taskletinfo.rwsched_tasklet.CFRunLoopAddTimer(
            self.taskletinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.sched,
            self.taskletinfo.rwsched_instance.CFRunLoopGetMainMode())

        self.abort_timer = self.taskletinfo.rwsched_tasklet.CFRunLoopTimer(
            CF.CFAbsoluteTimeGetCurrent(),
            20,
            self.abort,
            None)
        self.taskletinfo.rwsched_tasklet.CFRunLoopAddTimer(
            self.taskletinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.abort_timer,
            self.taskletinfo.rwsched_instance.CFRunLoopGetMainMode())


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='->%(asctime)s;%(levelname)s;%(message)s')
    plugin_dir = os.path.join(os.environ["RIFT_INSTALL"], "usr/lib/rift/plugins")

    if 'MESSAGE_BROKER_DIR' not in os.environ:
        os.environ['MESSAGE_BROKER_DIR'] = os.path.join(plugin_dir, 'rwmsgbroker-c')

    if 'ROUTER_DIR' not in os.environ:
        os.environ['ROUTER_DIR'] = os.path.join(plugin_dir, 'rwdtsrouter-c')

    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--test')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.INFO)
    testname = args.test
    logging.info(testname)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__], defaultTest=testname,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))


if __name__ == '__main__':
    main()

