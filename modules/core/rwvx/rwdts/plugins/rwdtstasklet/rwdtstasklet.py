#!/usr/bin/python

# STANDARD_RIFT_IO_COPYRIGHT

# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:

import gi
gi.require_version('CF', '1.0')
gi.require_version('RwTaskletPlugin', '1.0')
gi.require_version('RwTasklet', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwDtsToyTaskletYang', '1.0')
gi.require_version('RwVcsYang', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwTypes', '1.0')

import time
from gi.repository import GObject, RwTaskletPlugin
from gi.repository import CF, RwTasklet, RwDts, RwDtsToyTaskletYang
from gi.repository import RwVcsYang
from gi.repository import RwBaseYang, RwTypes

"""This is a basic example of python tasklet. The name
of the Python tasklet class doesn't matter, however it 
MUST be derived from GObject.Object and RwTaskletPlugin.Component.

Also note that the methods of the component interface 
must be prefixed with "do_". The Peas interface appends this
prefix before invoking the interface functions. 

NOTE: As a convention DONT use the do_ prefix for functions that 
are not part of the interface.

The VCS framework call the interface functions in the following order
    do_component_init
    do_instance_alloc
    do_instance_start
"""

import logging
import rwlogger

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger()
check_list = [ 'Started' ]

class AppConfGroupCreateFailed(Exception):
    pass

class XPathRegistrationFailed(Exception):
    pass


class SubscribeInsideXactExample(object):
    intel_company_xpath = (
            "C,/rw-dts-toy-tasklet:toytasklet-config"
            "/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name='intel']"
            )

    all_company_xpath = (
            "C,/rw-dts-toy-tasklet:toytasklet-config"
            "/rw-dts-toy-tasklet:company"
            )


    def __init__(self, tasklet):
        self.tasklet = tasklet
        self.dts_api_h = None

        self.all_app_group = None
        self.one_app_group = None

    def _on_all_company_config_xact_init(self, appconf, xact, ctx):
        xact_scratch = {"all_company":1, "apply": 0, "prepare":0, "validate":0 }
        logger.info("%s Got appconf callback for xact init company config ALL (%s): %s", self.__class__.__name__, xact, locals());
        return xact_scratch

    def _on_one_company_config_xact_init(self, appconf, xact, ctx):
        xact_scratch = {"one_company":1, "apply": 0, "prepare":0, "validate":0  }
        logger.info("%s Got appconf callback for xact init company config ONE (%s): %s", self.__class__.__name__, xact, locals())
        return xact_scratch

    def _on_config_apply(self, apih, appconf, xact, action, ctx, xact_scratch):
        logger.info("%s Got appconf callback for applying xact (%s): %s", self.__class__.__name__, xact, locals())
        if xact_scratch is not None:
            xact_scratch["apply"] = 1
        appconf.xact_add_issue(xact, RwTypes.RwStatus.FAILURE, "This is an add_issue error string")

    def _on_config_validate(self, apih, appconf, xact, action, ctx, xact_scratch):
        logger.info("%s Got appconf callback for validating xact (%s): %s", self.__class__.__name__, xact, locals())
        if xact_scratch is not None:
            xact_scratch["validate"] = 1

    def _on_one_company_config_prepare(self, apih, ac, xact, xact_info, keyspec, msg, ctx, scratch, prepud):
        logger.info("Got one company config prepare callback: %s", locals())
        scratch["prepare"] += 1
        ac.prepare_complete_ok(xact_info)

    def intel_regready(self, regh, rw_status, user_data):
        logger.info("DTS Intel reg_ready called ")
        check_list.append ("multiblock_regready")
        logger.debug(regh)
        logger.debug(rw_status)
        logger.debug(user_data)
        riftiomsg = RwDtsToyTaskletYang.CompanyConfig.new()
        riftiomsg.name = 'Intel'
        riftiomsg.description = 'cloud technologies'
        property1 = riftiomsg.property.add()
        property1.name = 'test-property'
        property1.value = 'test-value'

        self.dts_api_h.trace(self.intel_company_xpath)

        status = self.regh2.create_element_xpath(self.intel_company_xpath, riftiomsg.to_pbcm())
        logger.info("Publishing Intel config at xpath (%s): %s",
                self.intel_company_xpath, riftiomsg)

    def _on_all_company_config_prepare(self, apih, ac, xact, xact_info, keyspec, msg, ctx, scratch, prepud):
        logger.info("Got all company config prepare callback: %s, self.one_app_group=%s self.all_app_group=%s", locals(), self.one_app_group, self.all_app_group)

        scratch["prepare"] += 1

        logger.info("Point 1 in all company config prepare callback: %s, self.one_app_group=%s self.all_app_group=%s", locals(), self.one_app_group, self.all_app_group)

        # Take a ref on the other tasklet(?)'s one_app_group so it doesn't go away on us FIX FIX
        self.one_app_group_other = self.one_app_group

        status, self.one_app_group = self.dts_api_h.appconf_group_create(
                None,
                self._on_one_company_config_xact_init,
                None,
                None,
                self._on_config_apply,
                self,
                )

        logger.info("Point 2 in all company config prepare callback: %s, self.one_app_group=%s self.all_app_group=%s", locals(), self.one_app_group, self.all_app_group)

        if status != RwTypes.RwStatus.SUCCESS:
            raise AppConfGroupCreateFailed("Failed to create {} appconf group: status={}".format(
                self.__class__.__name__, status))

        status, cfgreg = self.one_app_group.register_xpath(
                self.intel_company_xpath,
                RwDts.Flag.SUBSCRIBER|RwDts.Flag.CACHE,
                self._on_one_company_config_prepare,
                self
                )

        if status != RwTypes.RwStatus.SUCCESS:
            raise XPathRegistrationFailed("{} failed to register as a subscriber for xpath: {}",
                    self.__class__.__name__, self.all_company_xpath)

        logger.debug("%s registered for xpath inside config prepare: %s",
                self.__class__.__name__, self.intel_company_xpath)

        # signal config registration completion
        self.one_app_group.phase_complete(RwDts.AppconfPhase.REGISTER)

        self.all_app_group.prepare_complete_ok(xact_info)

    def register_appconf(self):
        logger.debug("%s registering appconf", self.__class__.__name__)

        # Take extra ref in case we're in the two "tasklet" scenario
        self.all_app_group_other = self.all_app_group

        status, self.all_app_group = self.dts_api_h.appconf_group_create(
                None,
                self._on_all_company_config_xact_init,
                None,
                None,
                self._on_config_apply,
                self,
                )

        if status != RwTypes.RwStatus.SUCCESS:
            raise AppConfGroupCreateFailed("Failed to create {} appconf group: status={}".format(
                self.__class__.__name__, status))

        logger.debug("%s appconf_group created", self.__class__.__name__)

        status, cfgreg = self.all_app_group.register_xpath(
                self.all_company_xpath,
                RwDts.Flag.SUBSCRIBER,
                self._on_all_company_config_prepare,
                self
                )
        if status != RwTypes.RwStatus.SUCCESS:
            raise XPathRegistrationFailed("{} failed to register as a subscriber for xpath: {}",
                    self.__class__.__name__, self.all_company_xpath)

        logger.debug("%s registered for xpath: %s", self.__class__.__name__,
                self.all_company_xpath)

        # signal config registration completion
        self.all_app_group.phase_complete(RwDts.AppconfPhase.REGISTER)

        logger.debug("Creating timer to publish one company in 1 second")
        #timer = self.tasklet.taskletinfo.rwsched_tasklet.CFRunLoopTimer(
        #        CF.CFAbsoluteTimeGetCurrent() + 1,
        #        0,
        #        self.publish_one_company,
        #        self.tasklet.taskletinfo.rwsched_instance,
        #        )

        #self.tasklet.taskletinfo.rwsched_tasklet.CFRunLoopAddTimer(
        #        self.tasklet.taskletinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
        #        timer,
        #        self.tasklet.taskletinfo.rwsched_instance.CFRunLoopGetMainMode(),
        #        )

    def publish_one_company_config_callback(self, xact, xact_status, user_data):
        logger.info("publish_one_company_config_callback: %s", locals())

        return (RwDts.MemberRspCode.ACTION_OK);

    def publish_one_company(self, *args, **kwargs):
        logger.info("Publishing a single company")

        riftiomsg = RwDtsToyTaskletYang.CompanyConfig.new()
        riftiomsg.name = 'Intel'
        riftiomsg.description = 'cloud technologies'
        property1 = riftiomsg.property.add()
        property1.name = 'test-property'
        property1.value = 'test-value'

        self.dts_api_h.trace(self.intel_company_xpath)

        # Begin a DTS transaction to send the config
        xact = self.dts_api_h.query(
                self.intel_company_xpath,
                RwDts.QueryAction.UPDATE,
                RwDts.XactFlag.ADVISE,
                self.publish_one_company_config_callback,
                self,
                riftiomsg.to_pbcm()
                )

        xact.trace()

        logger.info("Publishing one company config at xpath (%s): %s",
                self.intel_company_xpath, riftiomsg)

    def rwdts_tasklet_state_change_cb(self, apih, state, user_data):
        logger.info("DTS Api init callback: %s", locals())

        self.dts_api_h = apih
        if state == RwDts.State.CONFIG:
          #Call some code here 
          self.dts_api_h.state = RwDts.State.RUN
          return
        if not state == RwDts.State.INIT:
          return
        self.register_appconf()
        if 1 :
          intel_company_xpath = ( "C,/rw-dts-toy-tasklet:toytasklet-config" "/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name='intel']")
          flags = ( RwDts.Flag.PUBLISHER | RwDts.Flag.CACHE | RwDts.Flag.NO_PREP_READ)
          status, regh2 = apih.member_register_xpath(intel_company_xpath,
                                                    None,
                                                    flags,
                                                    RwDts.ShardFlavor.NULL,
                                                    0,
                                                    0,
                                                    -1,
                                                    0,
                                                    0,
                                                    self.intel_regready,
                                                    None,
                                                    None,
                                                    None,
                                                    None,
                                                    self)
          logger.debug("member_register_xpath(cfgpath) returned")
          logger.debug(status);
          if regh2 is None :
            logger.debug("Registration handle is none")
          else:
            logger.debug('registration handle is')
          self.regh2 = regh2

        self.dts_api_h.state = RwDts.State.REGN_COMPLETE

class Callback(object):
    """This is a class to illustrate the callbacks through
    pygoject interface.
    """
    def rwsched_timer_callback(self, timer, user_data):
        """This number of arguments in the function must match
        the argument count to the function prototype in C.
        """
        logger.info("**** Voila1 Python TIMER callback ****")
        logger.debug(timer)
        logger.debug(user_data)
        logger.debug("\n\n")

    def rwdts_api_xact_blk_callback(self, xact, xact_status, user_data):
        """This number of arguments in the function must match
        the argument count to the function prototype in C.
        """
        logger.info("**** Voila Python DTS Api Query -block- callback ****")
        logger.info("xact = %s", xact)
        logger.info("user_data = %s", user_data)
        logger.info("xact_status = %s", xact_status)

        qcorrid = 1
        # Get the query result of the transaction, needs to be against block...
        query_result = xact.query_result(qcorrid)

        # Iterate through the result until  there are no more results
        while (query_result is not None):

          # Get the result as a protobuf - this returns a generic protobuf message
          pbcm = query_result.protobuf 

          # Convert the gerneric type to a type that we understand
          company = RwDtsToyTaskletYang.Company.from_pbcm(pbcm)

          # Log the results now
          logger.info("**** Query Result -block- ***** :Corrid %d", qcorrid)
          logger.info("XPath       : %s", query_result.xpath)
          logger.info("Query Result is")
          logger.info(query_result)
          logger.info(company)

          #Get the next result
          qcorrid = qcorrid + 1
          query_result = xact.query_result(qcorrid)

        # Get any errors
        logger.debug("**** Voila Python DTS Api Block Query - Check for errors ****")
        qcorrid = 0
        query_error = xact.query_error(qcorrid)

        #Iterate through the errors
        while (query_error is not None):
          #Log the error now
          logger.info("**** Query Error -xact- ***** :Corrid %d", qcorrid)
          logger.info("**** Query Error ****")
          #logger.info(dir(query_error));
          logger.info("XPath : %s", query_error.xpath)
          logger.info("Keystr : %s", query_error.keystr)
          logger.info("Query error cause is : %d", query_error.cause)
          logger.info("Error description : %s", query_error.errstr)
          #logger.info(query_error);

          #Get the next error
          #qcorrid = qcorrid + 1
          query_error = xact.query_error(qcorrid)

        #End this transaction
        #xact_status = xact.get_status()
        #logger.info("xact_status=%s", xact_status)

        #xact = None;
        check_list.append ("rwdts_api_xact_blk_callback got " + str(qcorrid))
        logger.info("Transaction blk ended ")

    def multiblock_regready(self, regh, rw_status, user_data):
        logger.info("DTS multiblock reg_ready called ")
        check_list.append ("multiblock_regready")
        logger.debug(regh)
        logger.debug(rw_status)
        logger.debug(user_data)

    def rwdts_api_xact_callback(self, xact, xact_status, user_data):
        """This number of arguments in the function must match
        the argument count to the function prototype in C.
        """
        logger.info("**** Voila Python DTS Api Query -xact- callback ****")
        logger.debug(xact)
        logger.debug(user_data)
        logger.info(xact_status)

        # Get the status of the xact.  Maybe ditch the status?
        #xact_status = xact.get_status()
        #logger.info("xact_status=%s", xact_status)

        #more = xact.get_more_results()
        #logger.debug("blk more flag for xact = %d",more)

        qcorrid = 0
        # Get the query result of the transaction
        query_result = xact.query_result(qcorrid)
        logger.info(query_result);

        # Iterate through the result until  there are no more results
        while (query_result is not None):

          # Get the result as a protobuf - this returns a generic protobuf message
          pbcm = query_result.protobuf 

          # Convert the gerneric type to a type that we understand
          company = RwDtsToyTaskletYang.Company.from_pbcm(pbcm)

          # Log the results now
          logger.info("**** Query Result -xact- ***** :Corrid %d", qcorrid)
          logger.info("**** Query Result ****")
          logger.info("XPath       : %s", query_result.xpath)
          logger.info("Query Result is")
          logger.info(query_result)
          logger.info("Company is :")
          logger.info(company)

          qcorrid = qcorrid +1
          #Get the next result
          query_result = xact.query_result(qcorrid)

        # Get any errors
        logger.debug("**** Voila Python DTS Api Query - Check for errors ****")
        qcorrid = 0
        query_error = xact.query_error(qcorrid)

        #Iterate through the errors
        while (query_error is not None):
          #Log the error now
          logger.info("**** Query Error -xact- ***** :Corrid %d", qcorrid)
          logger.info("**** Query Error ****")
          #logger.info(dir(query_error));
          logger.info("XPath : %s", query_error.xpath)
          logger.info("Keystr : %s", query_error.keystr)
          logger.info("Query error cause is : %d", query_error.cause)
          logger.info("Error description : %s", query_error.errstr)
          #logger.info(query_error);

          #Get the next error
          #qcorrid = qcorrid + 1
          query_error = xact.query_error(qcorrid)

        check_list.append("rwdts_api_xact_callback got" + str(qcorrid))
        xact = None
        logger.debug("Trasaction ended, xact unref in rwdts_api_xact_callback")


    def rwdts_api_config_callback(self, xact, xact_status, user_data):
        """This number of arguments in the function must match
        the argument count to the function prototype in C.
        """
        logger.info("!!!! Voila Python DTS Api config advise/publish callback !!!!")
        logger.debug(xact)
        logger.info(xact_status)
        logger.debug(user_data)
        #xact_status = xact.get_status()
        #logger.info("xact_status=%s", xact_status)
        # Delete this xact  ??
        xact = None
        logger.debug("Trasaction ended, xact unref in rwdts_api_config_callback")
        logger.info(check_list)

    def dts_dummy_cb(self, xact_info, user_data):
        logger.info("Dummy callback called")
        logger.debug(xact_info)
        logger.debug(user_data)

    def dts_single_query_reg_ready(self, regh, rw_status, user_data):
        logger.info("DTS single_query_reg_ready is ready *****")
        logger.debug("regh = %s", regh)
        logger.debug("rw_status = %s", rw_status)
        logger.debug("user_data = %s", user_data)
        # Begin a DTS transaction from a single query
        single_query_xpath = 'C,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'ub\']'
        #apih.trace(xpath)
        xact = self.dts_api.query(single_query_xpath,
                          RwDts.QueryAction.READ,
                          0,
                          self.rwdts_api_xact_callback,
                          self)
        logger.debug("xact = apih.query()")
        logger.debug(xact)

    def dts_single_query_prepare_cb(self, xact_info, action, keyspec, msg, user_data):
        logger.info("**** DTS single_query_prepare_cb is called *****")
        logger.debug("xact_info is %s", xact_info )
        logger.debug("action is %s", action)
        logger.debug("keyspec is %s", keyspec)
        logger.debug("msg is %s", msg)
        logger.debug("user_data is %s", user_data)
        logger.info("**** DTS sending another xact to create  multiblock xact *****")

        multiblock = 'C,/rw-dts-toy-tasklet:toytasklet-config/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'pasta\']'
        flags = RwDts.Flag.PUBLISHER
        apih = xact_info.get_api()
        #apih.trace(multiblock)
        xact = xact_info.get_xact()
        xact.trace()
        if action == RwDts.QueryAction.READ:
          xact_info.send_error_xpath(RwTypes.RwStatus.FAILURE,
                                     multiblock, 
                                     "PYTHON TASKLET THROWING ERRORERRORERROR dts_single_query_prepare_cb")
        check_list.append("Append Xact")
        status, regh2 = apih.member_register_xpath(multiblock,
                                                   xact,
                                                   flags,
                                                   RwDts.ShardFlavor.NULL,
                                                   0,
                                                   0,
                                                   -1,
                                                   0,
                                                   0,
                                                   self.multiblock_regready,
                                                   None,
                                                   None,
                                                   None,
                                                   None,
                                                   self)
        return (RwDts.MemberRspCode.ACTION_OK);


    def dts_reg_ready(self, regh, rw_status, user_data):
        logger.info("**** DTS registration is ready (are regh, self, keyspec, msg, user_data all different or all self?) *****")
        logger.debug("regh is %s", regh)
        logger.debug("rw_status is %s", rw_status)
        logger.debug("user_data is %s", user_data)

    def dts_cfg_reg_ready(self, regh, rw_status, user_data):
        logger.info("**** DTS cfg registration is ready *****")
        logger.debug(regh)
        logger.debug(rw_status)
        logger.debug(user_data)

        riftiopath = 'C,/rw-dts-toy-tasklet:toytasklet-config/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'riftio\']'
        riftiomsg = RwDtsToyTaskletYang.CompanyConfig.new()
        riftiomsg.name = 'Riftio'
        riftiomsg.description = 'cloud technologies'
        property1 = RwDtsToyTaskletYang.CompanyConfig_Property.new()
        property1.name = 'test-property'
        property1.value = 'test-value'
        riftiomsg.property = [property1]

        # Begin a DTS transaction to send the config
        configxact = self.dts_api.query(riftiopath, 
                                RwDts.QueryAction.UPDATE, 
                                RwDts.XactFlag.ADVISE,
                                self.rwdts_api_config_callback,
                                self,
                                riftiomsg.to_pbcm())
        logger.info("Sending config; configxact=%s", configxact)
        logger.info("Config path=%s", riftiopath)
        self.configxact = configxact


    def dts_cfg_prepare_cb(self, apih, xact, xactinfo, keyspec, msg, user_data):
        logger.info("**** DTS cfg prepare is called *****")
        logger.debug("apih is %s", apih)
        logger.debug("keyspec is %s", keyspec)
        logger.debug("msg is %s", msg)
        logger.debug("xactinfo is %s", xactinfo)
        logger.debug("userdata is %s", user_data)

        #Create a company object and return o/p
        company = RwDtsToyTaskletYang.Company()
        company.name = 'riftio'
        company.profile.industry='technology'
        company.profile.revenue = '$100,000,000,000'

        # Convert the gerneric type to a type that we understand
        if msg is not None :
          company = RwDtsToyTaskletYang.Company.from_pbcm(msg)

        if keyspec is not None :
          schema = RwDtsToyTaskletYang.Company.schema()
          pathentry = schema.keyspec_to_entry(keyspec)
          logger.debug(pathentry)
          if pathentry is not None:
            logger.debug("Received keyspec with path key %s", pathentry.key00.name)
          else:
            logger.debug("Oh-Oh  ----     Could not find path entry")

        logger.debug("member_register_xpath(cfgpath) returned")
        return (RwDts.MemberRspCode.ACTION_OK);


    def dts_prepare_cb(self, xact_info, action, keyspec, msg, user_data):
        logger.info("**** DTS prepare is called  (are regh, self, keyspec, msg, user_data all different or all self?) *****")
        logger.debug("xact_info is %s", xact_info)
        logger.debug("action is %s", action)
        logger.debug("keyspec is %s", keyspec)
        logger.debug("msg is %s", msg)
        logger.debug("user_data is %s", user_data)


        if keyspec is not None :
          schema = RwDtsToyTaskletYang.Company.schema()
          pathentry = schema.keyspec_to_entry(keyspec)
          logger.debug("Received path entry")
          logger.debug(pathentry)
          logger.debug(dir(pathentry))

        # Convert the gerneric type to a type that we understand
        xpath = 'C,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'riftio\']'
        if action == RwDts.QueryAction.READ:
          xact_info.send_error_xpath(RwTypes.RwStatus.FAILURE,
                                     xpath, 
                                     "PYTHON TASKLET THROWING ERRORERRORERROR dts_prepare_cb")
          #Create a company object and return o/p
          company = RwDtsToyTaskletYang.Company()
          company.name = 'riftio'
          company.profile.industry='technology'
          company.profile.revenue = '$100,000,000,000'
          check_list.append("dts_prepare_cb read " + company.name)

          xact_info.respond_xpath(RwDts.XactRspCode.ACK,
                                  xpath,
                                  company.to_pbcm())
        else:
          if msg is not None :
            company = RwDtsToyTaskletYang.Company.from_pbcm(msg)

        # We ret async.  How does this end? Oh, it's ack above...
        logger.info("**** DTS dts_prepare_cb done, ret ASYNC *****")
        return (RwDts.MemberRspCode.ACTION_ASYNC)

    # called on a new transaction
    def config_xact_init(self, ac, xact, ctx):
       logger.info("**** config_xact_init() called ****")
       logger.info("self=%s", self)
       logger.info("ac=%s", ac)
       logger.info("xact=%s", xact)
       logger.info("ctx=%s", ctx)
       # Note no local reference, the dts api holds the only required reference except when executing appconf-config callbacks
       scratch = { 'validatect':0, 'applyct':0, 'preparect':0, 'val1':'test scratch val1' }
       return scratch

    # called on the end of a transaction 
    def config_xact_deinit(self, ac, xact, ctx, scratch):
       logger.info("**** config_xact_deinit() called ****")
       logger.info("self=%s", self)
       logger.info("ac=%s", ac)
       logger.info("xact=%s", xact)
       logger.info("ctx=%s", ctx)
       logger.info("scratch=%s", scratch)
       scratch=None
       return

    # called when validating a config
    def config_validate(self, apih, ac, xact, ctx, scratch):
       logger.info("**** config_validate() called ****")
       logger.info("self=%s", self)
       logger.info("apih=%s", apih)
       logger.info("ac=%s", ac)
       logger.info("xact=%s", xact)
       logger.info("ctx=%s", ctx)            # should be self, Callback object, handed as context to appconf_create
       logger.info("scratch=%s", scratch)
       scratch['validatect'] = scratch['validatect'] + 1
       logger.info("scratch=%s", scratch)
       return

    # called to apply a config
    def config_apply(self, apih, ac, xact, action, ctx, scratch):
       logger.info("**** config_apply() called ****")
       logger.info("self=%s", self)
       logger.info("apih=%s", apih)
       logger.info("ac=%s", ac)
       logger.info("xact=%s", xact)
       logger.info("action=%s", action)
       logger.info("ctx=%s", ctx)
       logger.info("scratch=%s", scratch)
       # apply won't always have a xact with its scratch!
       if scratch:
           scratch['applyct'] = scratch['applyct'] + 1
       logger.info("scratch=%s", scratch)
       return

    # called on a config prepare
    def config_prepare(self, apih, ac, xact, xact_info, keyspec, msg, ctx, scratch, prepud):
       logger.info("**** config_prepare() called ****")
       logger.info("self=%s", self)
       logger.info("apih=%s", apih)
       logger.info("ac=%s", ac)
       logger.info("xact=%s", xact)
       logger.info("xact_info=%s", xact_info)     # this is nominally a query handle, I think there is a bug here?
       logger.info("keyspec=%s", keyspec)
       logger.info("msg=%s", msg)
       logger.info("ctx=%s", ctx)           # should be this Callback / self
       logger.info("prepud=%s", prepud)     # should be this Callback / self
       logger.info("scratch=%s", scratch)   # should be our scratch
       scratch['preparect'] = scratch['preparect'] + 1
       logger.info("scratch=%s", scratch)

       ac.prepare_complete_ok(xact_info)

       # ac.prepare_complete_fail(xact_info, RW_STATUS_MEH, 'No such nozzle.'

       return


    def rwdts_tasklet_state_change_cb(self, apih, state, user_data):
        """This number of arguments in the function must match
        the argument count to the function prototype in C.
        """
        logger.info("**** Voila Python DTS Api init callback ****")
        logger.debug("apih=")
        logger.debug(apih)
        logger.debug("state=")
        logger.debug(state)
        logger.debug("user_date=")
        logger.debug(user_data)

        self.dts_api = apih
        self.rwdtstaskletpython = user_data

        print("Sleeping 5s, hit ^C to set bp\n")
        time.sleep(5)
        if state == RwDts.State.CONFIG:
          #Call recovery reconcile code here
          self.dts_api.state = RwDts.State.RUN
          return
        if not state == RwDts.State.INIT:
          return
        # register for configuration
        status, appgrp = apih.appconf_group_create(None, self.config_xact_init, self.config_xact_deinit, self.config_validate, self.config_apply, self)
        logger.info("appconf_group_create returned status %d", status)
        self.appgrp = appgrp

        cfgpath = 'C,/rw-dts-toy-tasklet:toytasklet-config/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'riftio\']'
        status, cfgreg = appgrp.register_xpath(cfgpath, RwDts.Flag.SUBSCRIBER|RwDts.Flag.CACHE, self.config_prepare, self)
        logger.info("appgrp.register_xpath returned status %d", status)
        #logger.info(cfgreg)
        self.cfgreg = cfgreg

        # signal config registration completion
        appgrp.phase_complete(RwDts.AppconfPhase.REGISTER)

        # define the callbacks that we want to receive
        xpath = 'C,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'riftio\']'
        flags = RwDts.Flag.PUBLISHER
        status, regh = apih.member_register_xpath(xpath,
                                                  None,
                                                  flags,
                                                  RwDts.ShardFlavor.NULL,
                                                  0,
                                                  0,
                                                  -1,
                                                  0,
                                                  0,
                                                  self.dts_reg_ready,
                                                  self.dts_prepare_cb,
                                                  None,
                                                  None,
                                                  None,
                                                  self)

        logger.debug("member_register_xpath() returned")
        logger.debug(status);
        if regh is None :
          logger.debug("Registration handle is none")
        else:
          logger.debug('registration handle is')
        self.regh = regh

        # Single query . Call on reg_ready
        single_query_xpath = 'C,/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'ub\']'
        flags = RwDts.Flag.PUBLISHER | RwDts.Flag.CACHE
        status, regh = apih.member_register_xpath(single_query_xpath,
                                                  None,
                                                  flags,
                                                  RwDts.ShardFlavor.NULL,
                                                  0,
                                                  0,
                                                  -1,
                                                  0,
                                                  0,
                                                  self.dts_single_query_reg_ready,
                                                  self.dts_single_query_prepare_cb,
                                                  None,
                                                  None,
                                                  None,
                                                  self)
        logger.debug("member_register_xpath() returned")
        logger.debug(status);
        if regh is None :
          logger.debug("Registration handle is none")
        else:
          logger.debug('registration handle is')
        self.sq_regh = regh


        # now waiting for dts_reg_ready()

        # register config publisher
        if 1 :
          cfgpath2 = 'C,/rw-dts-toy-tasklet:toytasklet-config/rw-dts-toy-tasklet:company[rw-dts-toy-tasklet:name=\'noodle\']'
          flags = ( RwDts.Flag.PUBLISHER | RwDts.Flag.CACHE )
          status, regh2 = apih.member_register_xpath(cfgpath2,
                                                    None,
                                                    flags, 
                                                    RwDts.ShardFlavor.NULL,
                                                    0,
                                                    0,
                                                    -1,
                                                    0,
                                                    0,
                                                    self.dts_cfg_reg_ready,
                                                    self.dts_cfg_prepare_cb,
                                                    None,
                                                    None,
                                                    None,
                                                    self)
          logger.debug("member_register_xpath(cfgpath) returned")
          logger.debug(status);
          if regh2 is None :
            logger.debug("Registration handle is none")
          else:
            logger.debug('registration handle is')
          self.regh2 = regh2

        # now waiting for dts_cfg_reg_ready()

        # this ought to be later, as well

        if 1 :

          # Construct an (empty) transaction.  0 is flags
          xact2 = apih.xact_create(0, None, None)
          #xact2.trace()
          logger.debug("xact2=apih.xact_create");
          logger.debug("xact2=%s", xact2);

          # Construct a query-block in / from the transaction
          blk = xact2.block_create(0, None, None)
          logger.debug("xact2.block_create")
          logger.debug(blk)

          # Add some query(ies) to the block
          # Corrid is a nonzero int, useful for getting back specific query's results
          qcorrid = 1
          status = blk.add_query(xpath, RwDts.QueryAction.READ, RwDts.XactFlag.STREAM, qcorrid, None)
          logger.debug(status)

          qcorrid = qcorrid + 1
          status = blk.add_query(xpath, RwDts.QueryAction.READ, RwDts.XactFlag.STREAM, qcorrid, None)

          qcorrid = qcorrid + 1
          status = blk.add_query(xpath, RwDts.QueryAction.READ, RwDts.XactFlag.STREAM, qcorrid, None) 

          # Pull the trigger.  Callback+userdata are here.  The final "after" argument, 
          # not included here, is a block to be after; otherwise at end
          # with after block: status = blk.execute(0, self.rwdts_api_xact_blk_callback, self, earlier_block)
          status = blk.execute(0, self.rwdts_api_xact_blk_callback, self)
          logger.debug("blk.execute")
          check_list.append("blk.execute queries sent " + str(qcorrid))
          logger.debug(status)

          # gads, imm should just be a flag
#broken          status = blk.execute_immediate(0, self.rwdts_api_xact_blk_callback, self)

#??          status , blkstate = blk.get_status()

          #more = blk.get_more_results()
          #logger.debug("blk more flag = %d\n",more)

          #more = blk.for_query_get_more_results(qcorrid)
          #logger.debug("blk more flag for query = %d\n",more)

          status = xact2.commit()
          logger.debug("xact2.commit()")
          logger.debug(status)


          # Now let's test the member data APIs
          self.memberd = RwdtsMemberDataAPIs(apih)
        self.dts_api.state = RwDts.State.REGN_COMPLETE

class RwdtsMemberDataAPIs(object):
    """This class  implements the APIs to demo member data  APIs
    """
    def __init__(self, apih):
        #commented this to use the system-wide logger
        #self.logger = logging.getLogger(name="dtstasklet_logger")
        logger.debug("RwdtsMemberDataAPIs: __init__ function called")

        self.apih = apih
        xpath = 'C,/rw-dts-toy-tasklet:employee]'
        status, keyspec = apih.keyspec_from_xpath(xpath)
        flags = RwDts.Flag.PUBLISHER
        status, self.mbdreg = apih.member_register_keyspec(keyspec,
                                                           None,
                                                           flags,
                                                           RwDts.ShardFlavor.NULL,
                                                           0,
                                                           0,
                                                           -1,
                                                           0,
                                                           0,
                                                           self.create_member_objects,
                                                           self.create_member_data_prepare,
                                                           None,
                                                           None,
                                                           None,
                                                           self)
        logger.debug("RwdtsMemberDataAPIs - apih.member_register_xpath returned %s", status)


    def create_member_objects(self, regh, keyspec, msg, user_data):
        """This function  implements the data member APIs  to create, update and read objects
        """
        logger.info("Creating member data objects !!!")

        id = 1
        for id in range(1, 10):
          emp = RwDtsToyTaskletYang.Employee()
          emp.name = 'jdoe' + str(id)
          emp.age = 30 + id
          emp.phone = '978-863-00' + str(id)
          emp.ssn = '123-45-000' + str(id)
          path = '/rw-dts-toy-tasklet:employee[rw-dts-toy-tasklet:name=\'jdoe' + str(id) + '\']'
          status, ks = self.apih.keyspec_from_xpath(path)
          logger.debug("keyspec_from_xpath returned %s for path %s", status, ks)
          status = self.mbdreg.create_element_keyspec(ks, emp.to_pbcm())
          logger.debug("create_element_keyspec returned %s for path %s", status, ks)

        #Update an element
        path = '/rw-dts-toy-tasklet:employee[rw-dts-toy-tasklet:name=\'jdoe9\']'
        status, ks = self.apih.keyspec_from_xpath(path)
        logger.debug("keyspec_from_xpath returned %s for path %s", status, ks)
        emp = RwDtsToyTaskletYang.Employee()
        emp.name = 'jdoe9' + str(id)
        emp.age = 41
        emp.phone = '978-863-099'
        emp.ssn = '123-45-0099'
        status = self.mbdreg.update_element_keyspec(ks, emp.to_pbcm(), RwDts.XactFlag.REPLACE)
        logger.info("Updated the object with key = %s status = %s", path, status)

        # Now read it back
        status, out_ks, pbcm = self.mbdreg.get_element_keyspec(ks)
        logger.info("Get returned status=%s, pbcm=%s out_ks = %s", status, pbcm, out_ks)
        employee = RwDtsToyTaskletYang.Employee.from_pbcm(pbcm)
        logger.info("Read record is %s", employee)

        # Now read with xpath
        status, pbcm,out_ks = self.mbdreg.get_element_xpath('C,/rw-dts-toy-tasklet:employee[rw-dts-toy-tasklet:name=\'jdoe8\']')
        logger.info("Get returned using xpath status=%s pbcm=%s out_ks = %s", status, pbcm, out_ks)
        employee = RwDtsToyTaskletYang.Employee.from_pbcm(pbcm)
        logger.info("Read record  using xpath is %s", employee)

        # Get a cursor and walk the list
        cursor =  self.mbdreg.get_cursor()
        msg, ks = self.mbdreg.get_next_element(cursor)

        while msg is not None:
          employee = RwDtsToyTaskletYang.Employee.from_pbcm(msg)
          logger.info("Read record  using get next api  %s", employee)
          msg, ks = self.mbdreg.get_next_element(cursor)
     
        self.mbdreg.delete_cursors()

    def create_member_data_prepare(self, userdata):
        """This function  implements the prepare callback for the data member API tests
        """
        logger.info("create_member_data_prepare called!!!")


class RwdtstaskletPython(GObject.Object, RwTaskletPlugin.Component):
    """This class implements the 'RwTaskletPlugin.Component' interface.
    """
    def __init__(self):
        #commented this to use the system-wide logger
        #self.logger = logging.getLogger(name="dtstasklet_logger")
        logger.debug("RwdtstaskletPython: __init__ function called")
        GObject.Object.__init__(self)

    def do_component_init(self):
        """This function is called once during the compoenent
        initialization.
        """
        logger.debug("RwdtstaskletPython: do_component_init function called")
        component_handle = RwTaskletPlugin.ComponentHandle()
        return component_handle

    def do_component_deinit(self, component_handle):
        logger.debug("RwdtstaskletPython: do_component_deinit function called")

    def do_instance_alloc(self, component_handle, tasklet_info, instance_url):
        """This function is called for each new instance of the tasklet.
        The tasklet specific information such as scheduler instance,
        trace context, logging context are passed in 'tasklet_info' variable.
        This function stores the tasklet information locally.
        """
        logger.debug("RwdtstaskletPython: do_instance_alloc function called")

        self.taskletinfo = tasklet_info

        # Save the scheduler instance and tasklet instance objects
        #self.rwsched = tasklet_info.rwsched_instance
        #self.tasklet = tasklet_info.rwsched_tasklet_info
        #self.rwlog_instance = tasklet_info.rwlog_instance


        tasklet_logger = rwlogger.RwLogger(subcategory="rw-vcs",
                                           log_hdl=self.taskletinfo.rwlog_ctx)
        logger.addHandler(tasklet_logger)

        # After this point, all logger calls will log events to rw_vcs using
        # the tasklets rwlog handle
        logger.debug("Added rwlogger handler to tasklet logger")

        instance_handle = RwTaskletPlugin.InstanceHandle()
        return instance_handle

    def do_instance_free(self, component_handle, instance_handle):
        logger.debug("RwdtstaskletPython: do_instance_free function called")

    def do_instance_start(self, component_handle, instance_handle):
        """This function is called to start the tasklet operations.
        Typically DTS initialization is done in this function.
        In this example a periodic timer is added to the tasklet
        scheduler.
        """
        logger.debug("RwdtstaskletPython: do_instance_start function called")

        # Create an instance of DTS API - This object is needed by all DTS
        # member and query APIs directly or indirectly.
        # DTS invokes the callback to notify the tasklet that the DTS API instance is ready 
        # for use.

        foo = Callback()
        #sub = SubscribeInsideXactExample(self)
        self.dts_api = RwDts.Api.new(self.taskletinfo,                 # tasklet object
                                     RwDtsToyTaskletYang.get_schema(), # Schema object
                                     foo.rwdts_tasklet_state_change_cb,      # The callback for DTS state change
                                     #sub.rwdts_tasklet_state_change_cb,
                                     self)                             # user data in the callback - in this case self


    def do_instance_stop(self, component_handle, instance_handle):
        logger.debug("RwdtstaskletPython: do_instance_stop function called")

if __name__ == "__main__":
    #add your test code to execute this as a standalone program
    tasklet = RwdtstaskletPython()

    component_handle = tasklet.component_init()
    logger.debug("main: componente=%s" % (component_handle))
    logger.debug("main: component-type=%s" % type(component_handle))
    tasklet_info = RwTaskletPlugin._RWTaskletInfo()
    instance_url = RwTaskletPlugin._RWExecURL()
    logger.debug("main: tasklet=%s" % (tasklet_info))
    logger.debug("main: tasklet-type=%s" % type(tasklet_info))
    logger.debug("main: url=%s" % (instance_url))
    logger.debug("main: url-type=%s" % type(instance_url))
    instance_handle = tasklet.instance_alloc(component_handle, tasklet_info, instance_url)
    logger.debug("main: instance=%s" % (instance_handle))
    logger.debug("main: instance-type=%s" % type(instance_handle))
    tasklet.instance_start(component_handle, instance_handle)
