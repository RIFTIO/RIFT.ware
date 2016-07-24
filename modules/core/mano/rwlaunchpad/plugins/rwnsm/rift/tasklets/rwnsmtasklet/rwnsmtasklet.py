# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
import asyncio
import os
import shutil
import sys
import tempfile
import time
import uuid
import yaml

from collections import deque
from collections import defaultdict
from enum import Enum

import gi
gi.require_version('RwNsdYang', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwNsmYang', '1.0')
gi.require_version('RwNsrYang', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwVlrYang', '1.0')
gi.require_version('RwVnfrYang', '1.0')
from gi.repository import (
    RwNsrYang,
    NsrYang,
    NsdYang,
    RwVlrYang,
    VnfrYang,
    RwVnfrYang,
    RwNsmYang,
    RwsdnYang,
    RwDts as rwdts,
    RwTypes,
    ProtobufC,
)

import rift.tasklets
import rift.mano.config_data.config

from . import rwnsm_conman as conman
from . import cloud
from . import publisher
from . import xpath
from . import config_value_pool
from . import rwvnffgmgr
from . import scale_group


class NetworkServiceRecordState(Enum):
    """ Network Service Record State """
    INIT = 101
    VL_INIT_PHASE = 102
    VNF_INIT_PHASE = 103
    VNFFG_INIT_PHASE = 104
    RUNNING = 106
    SCALING_OUT = 107
    SCALING_IN = 108
    TERMINATE = 109
    TERMINATE_RCVD = 110
    VL_TERMINATE_PHASE = 111
    VNF_TERMINATE_PHASE = 112
    VNFFG_TERMINATE_PHASE = 113
    TERMINATED = 114
    FAILED = 115


class NetworkServiceRecordError(Exception):
    """ Network Service Record Error """
    pass


class NetworkServiceDescriptorError(Exception):
    """ Network Service Descriptor Error """
    pass


class VirtualNetworkFunctionRecordError(Exception):
    """ Virtual Network Function Record Error """
    pass


class NetworkServiceDescriptorNotFound(Exception):
    """ Cannot find Network Service Descriptor"""
    pass


class NetworkServiceDescriptorRefCountExists(Exception):
    """ Network Service Descriptor reference count exists """
    pass


class NetworkServiceDescriptorUnrefError(Exception):
    """ Failed to unref a network service descriptor """
    pass


class NsrInstantiationFailed(Exception):
    """ Failed to instantiate network service """
    pass


class VnfInstantiationFailed(Exception):
    """ Failed to instantiate virtual network function"""
    pass


class VnffgInstantiationFailed(Exception):
    """ Failed to instantiate virtual network function"""
    pass


class VnfDescriptorError(Exception):
    """Failed to instantiate virtual network function"""
    pass


class ScalingOperationError(Exception):
    pass


class ScaleGroupMissingError(Exception):
    pass

class PlacementGroupError(Exception):
    pass


class VlRecordState(Enum):
    """ VL Record State """
    INIT = 101
    INSTANTIATION_PENDING = 102
    ACTIVE = 103
    TERMINATE_PENDING = 104
    TERMINATED = 105
    FAILED = 106


class VnffgRecordState(Enum):
    """ VNFFG Record State """
    INIT = 101
    INSTANTIATION_PENDING = 102
    ACTIVE = 103
    TERMINATE_PENDING = 104
    TERMINATED = 105
    FAILED = 106


class VnffgRecord(object):
    """ Vnffg Records class"""
    SFF_DP_PORT = 4790
    SFF_MGMT_PORT = 5000
    def __init__(self, dts, log, loop, vnffgmgr, nsr, nsr_name, vnffgd_msg, sdn_account_name):

        self._dts = dts
        self._log = log
        self._loop = loop
        self._vnffgmgr = vnffgmgr
        self._nsr = nsr
        self._nsr_name = nsr_name
        self._vnffgd_msg = vnffgd_msg
        if sdn_account_name is None:
            self._sdn_account_name = ''
        else:
            self._sdn_account_name = sdn_account_name

        self._vnffgr_id = str(uuid.uuid4())
        self._vnffgr_rsp_id = list()
        self._vnffgr_state = VnffgRecordState.INIT

    @property
    def id(self):
        """ VNFFGR id """
        return self._vnffgr_id

    @property
    def state(self):
        """ state of this VNF """
        return self._vnffgr_state

    def fetch_vnffgr(self):
        """
        Get VNFFGR message to be published
        """

        if self._vnffgr_state == VnffgRecordState.INIT:
            vnffgr_dict = {"id": self._vnffgr_id,
                           "nsd_id": self._nsr.nsd_id,
                           "vnffgd_id_ref": self._vnffgd_msg.id,
                           "vnffgd_name_ref": self._vnffgd_msg.name,
                           "sdn_account": self._sdn_account_name,
                           "operational_status": 'init',
                           }
            vnffgr = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_Vnffgr.from_dict(vnffgr_dict)
        elif self._vnffgr_state == VnffgRecordState.TERMINATED:
            vnffgr_dict = {"id": self._vnffgr_id,
                           "nsd_id": self._nsr.nsd_id,
                           "vnffgd_id_ref": self._vnffgd_msg.id,
                           "vnffgd_name_ref": self._vnffgd_msg.name,
                           "sdn_account": self._sdn_account_name,
                           "operational_status": 'terminated',
                           }
            vnffgr = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_Vnffgr.from_dict(vnffgr_dict)
        else:
            try:
                vnffgr = self._vnffgmgr.fetch_vnffgr(self._vnffgr_id)
            except Exception:
                self._log.exception("Fetching VNFFGR for VNFFG with id %s failed", self._vnffgr_id)
                self._vnffgr_state = VnffgRecordState.FAILED
                vnffgr_dict = {"id": self._vnffgr_id,
                               "nsd_id": self._nsr.nsd_id,
                               "vnffgd_id_ref": self._vnffgd_msg.id,
                               "vnffgd_name_ref": self._vnffgd_msg.name,
                               "sdn_account": self._sdn_account_name,
                               "operational_status": 'failed',
                               }
                vnffgr = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_Vnffgr.from_dict(vnffgr_dict)

        return vnffgr

    @asyncio.coroutine
    def vnffgr_create_msg(self):
        """ Virtual Link Record message for Creating VLR in VNS """
        vnffgr_dict = {"id": self._vnffgr_id,
                       "nsd_id": self._nsr.nsd_id,
                       "vnffgd_id_ref": self._vnffgd_msg.id,
                       "vnffgd_name_ref": self._vnffgd_msg.name,
                       "sdn_account": self._sdn_account_name,
                    }
        vnffgr = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_Vnffgr.from_dict(vnffgr_dict)
        for rsp in self._vnffgd_msg.rsp:
            vnffgr_rsp = vnffgr.rsp.add()
            vnffgr_rsp.id = str(uuid.uuid4())
            vnffgr_rsp.name = self._nsr.name + '.' + rsp.name
            self._vnffgr_rsp_id.append(vnffgr_rsp.id)
            vnffgr_rsp.vnffgd_rsp_id_ref =  rsp.id
            vnffgr_rsp.vnffgd_rsp_name_ref = rsp.name
            for rsp_cp_ref in rsp.vnfd_connection_point_ref:
                vnfd =  [vnfr.vnfd for vnfr in self._nsr.vnfrs.values() if vnfr.vnfd.id == rsp_cp_ref.vnfd_id_ref]
                self._log.debug("VNFD message during VNFFG instantiation is %s",vnfd)
                if len(vnfd) > 0 and vnfd[0].has_field('service_function_type'):
                    self._log.debug("Service Function Type for VNFD ID %s is %s",rsp_cp_ref.vnfd_id_ref, vnfd[0].service_function_type)
                else:
                    self._log.error("Service Function Type not available for VNFD ID %s; Skipping in chain",rsp_cp_ref.vnfd_id_ref)
                    continue

                vnfr_cp_ref =  vnffgr_rsp.vnfr_connection_point_ref.add()
                vnfr_cp_ref.member_vnf_index_ref = rsp_cp_ref.member_vnf_index_ref
                vnfr_cp_ref.hop_number = rsp_cp_ref.order
                vnfr_cp_ref.vnfd_id_ref =rsp_cp_ref.vnfd_id_ref
                vnfr_cp_ref.service_function_type = vnfd[0].service_function_type
                for nsr_vnfr in self._nsr.vnfrs.values():
                   if (nsr_vnfr.vnfd.id == vnfr_cp_ref.vnfd_id_ref and
                      nsr_vnfr.member_vnf_index == vnfr_cp_ref.member_vnf_index_ref):
                       vnfr_cp_ref.vnfr_id_ref = nsr_vnfr.id
                       vnfr_cp_ref.vnfr_name_ref = nsr_vnfr.name
                       vnfr_cp_ref.vnfr_connection_point_ref = rsp_cp_ref.vnfd_connection_point_ref

                       vnfr = yield from self._nsr.fetch_vnfr(nsr_vnfr.xpath)
                       self._log.debug(" Received VNFR is %s", vnfr)
                       while vnfr.operational_status != 'running':
                           self._log.info("Received vnf op status is %s; retrying",vnfr.operational_status)
                           if vnfr.operational_status == 'failed':
                               self._log.error("Fetching VNFR for  %s failed", vnfr.id)
                               raise NsrInstantiationFailed("Failed NS %s instantiation due to VNFR %s failure" % (self.id, vnfr.id))
                           yield from asyncio.sleep(2, loop=self._loop)
                           vnfr = yield from self._nsr.fetch_vnfr(nsr_vnfr.xpath)
                           self._log.debug("Received VNFR is %s", vnfr)

                       vnfr_cp_ref.connection_point_params.mgmt_address =  vnfr.mgmt_interface.ip_address
                       for cp in vnfr.connection_point:
                           if cp.name == vnfr_cp_ref.vnfr_connection_point_ref:
                               vnfr_cp_ref.connection_point_params.port_id = cp.connection_point_id
                               vnfr_cp_ref.connection_point_params.name = self._nsr.name + '.' + cp.name
                               for vdu in vnfr.vdur:
                                   for ext_intf in vdu.external_interface:
                                       if ext_intf.name == vnfr_cp_ref.vnfr_connection_point_ref:
                                           vnfr_cp_ref.connection_point_params.vm_id =  vdu.vim_id
                                           self._log.debug("VIM ID for CP %s in VNFR %s is %s",cp.name,nsr_vnfr.id,
                                                            vnfr_cp_ref.connection_point_params.vm_id)
                                           break

                               vnfr_cp_ref.connection_point_params.address =  cp.ip_address
                               vnfr_cp_ref.connection_point_params.port = VnffgRecord.SFF_DP_PORT

        for vnffgd_classifier in self._vnffgd_msg.classifier:
            _rsp =  [rsp for rsp in vnffgr.rsp if rsp.vnffgd_rsp_id_ref == vnffgd_classifier.rsp_id_ref]
            if len(_rsp) > 0:
                rsp_id_ref = _rsp[0].id
                rsp_name = _rsp[0].name
            else:
                self._log.error("RSP with ID %s not found during classifier creation for classifier id %s",vnffgd_classifier.rsp_id_ref,vnffgd_classifier.id)
                continue
            vnffgr_classifier = vnffgr.classifier.add()
            vnffgr_classifier.id = vnffgd_classifier.id
            vnffgr_classifier.name =  self._nsr.name + '.' + vnffgd_classifier.name
            _rsp[0].classifier_name = vnffgr_classifier.name
            vnffgr_classifier.rsp_id_ref = rsp_id_ref
            vnffgr_classifier.rsp_name = rsp_name
            for nsr_vnfr in self._nsr.vnfrs.values():
               if (nsr_vnfr.vnfd.id == vnffgd_classifier.vnfd_id_ref and
                      nsr_vnfr.member_vnf_index == vnffgd_classifier.member_vnf_index_ref):
                       vnffgr_classifier.vnfr_id_ref = nsr_vnfr.id
                       vnffgr_classifier.vnfr_name_ref = nsr_vnfr.name
                       vnffgr_classifier.vnfr_connection_point_ref = vnffgd_classifier.vnfd_connection_point_ref

                       if nsr_vnfr.vnfd.service_function_chain == 'CLASSIFIER':
                           vnffgr_classifier.sff_name = nsr_vnfr.name

                       vnfr = yield from self._nsr.fetch_vnfr(nsr_vnfr.xpath)
                       self._log.debug(" Received VNFR is %s", vnfr)
                       while vnfr.operational_status != 'running':
                           self._log.info("Received vnf op status is %s; retrying",vnfr.operational_status)
                           if vnfr.operational_status == 'failed':
                               self._log.error("Fetching VNFR for  %s failed", vnfr.id)
                               raise NsrInstantiationFailed("Failed NS %s instantiation due to VNFR %s failure" % (self.id, vnfr.id))
                           yield from asyncio.sleep(2, loop=self._loop)
                           vnfr = yield from self._nsr.fetch_vnfr(nsr_vnfr.xpath)
                           self._log.debug("Received VNFR is %s", vnfr)

                       for cp in vnfr.connection_point:
                           if cp.name == vnffgr_classifier.vnfr_connection_point_ref:
                               vnffgr_classifier.port_id = cp.connection_point_id
                               vnffgr_classifier.ip_address = cp.ip_address
                               for vdu in vnfr.vdur:
                                   for ext_intf in vdu.external_interface:
                                       if ext_intf.name == vnffgr_classifier.vnfr_connection_point_ref:
                                           vnffgr_classifier.vm_id =  vdu.vim_id
                                           self._log.debug("VIM ID for CP %s in VNFR %s is %s",cp.name,nsr_vnfr.id,
                                                            vnfr_cp_ref.connection_point_params.vm_id)
                                           break

        self._log.info("VNFFGR msg to be sent is %s", vnffgr)
        return vnffgr

    @asyncio.coroutine
    def vnffgr_nsr_sff_list(self):
        """ SFF List for VNFR """
        sff_list = {}
        sf_list = [nsr_vnfr.name for nsr_vnfr in self._nsr.vnfrs.values() if nsr_vnfr.vnfd.service_function_chain == 'SF']

        for nsr_vnfr in self._nsr.vnfrs.values():
            if (nsr_vnfr.vnfd.service_function_chain == 'CLASSIFIER' or nsr_vnfr.vnfd.service_function_chain == 'SFF'):
                vnfr = yield from self._nsr.fetch_vnfr(nsr_vnfr.xpath)
                self._log.debug(" Received VNFR is %s", vnfr)
                while vnfr.operational_status != 'running':
                    self._log.info("Received vnf op status is %s; retrying",vnfr.operational_status)
                    if vnfr.operational_status == 'failed':
                       self._log.error("Fetching VNFR for  %s failed", vnfr.id)
                       raise NsrInstantiationFailed("Failed NS %s instantiation due to VNFR %s failure" % (self.id, vnfr.id))
                    yield from asyncio.sleep(2, loop=self._loop)
                    vnfr = yield from self._nsr.fetch_vnfr(nsr_vnfr.xpath)
                    self._log.debug("Received VNFR is %s", vnfr)

                sff =  RwsdnYang.VNFFGSff()
                sff_list[nsr_vnfr.vnfd.id] = sff
                sff.name = nsr_vnfr.name
                sff.function_type = nsr_vnfr.vnfd.service_function_chain

                sff.mgmt_address = vnfr.mgmt_interface.ip_address
                sff.mgmt_port = VnffgRecord.SFF_MGMT_PORT
                for cp in vnfr.connection_point:
                    sff_dp = sff.dp_endpoints.add()
                    sff_dp.name = self._nsr.name + '.' + cp.name
                    sff_dp.address = cp.ip_address
                    sff_dp.port  = VnffgRecord.SFF_DP_PORT
                if nsr_vnfr.vnfd.service_function_chain == 'SFF':
                    for sf_name in sf_list:
                        _sf = sff.vnfr_list.add()
                        _sf.vnfr_name = sf_name

        return sff_list

    @asyncio.coroutine
    def instantiate(self):
        """ Instantiate this VNFFG """

        self._log.info("Instaniating VNFFGR with vnffgd %s",
                       self._vnffgd_msg)


        vnffgr_request = yield from self.vnffgr_create_msg()
        vnffg_sff_list = yield from self.vnffgr_nsr_sff_list()

        try:
            vnffgr = self._vnffgmgr.create_vnffgr(vnffgr_request,self._vnffgd_msg.classifier,vnffg_sff_list)
        except Exception as e:
            self._log.exception("VNFFG instantiation failed: %s", str(e))
            self._vnffgr_state = VnffgRecordState.FAILED
            raise NsrInstantiationFailed("Failed NS %s instantiation due to VNFFGR %s failure" % (self.id, vnffgr_request.id))

        self._vnffgr_state = VnffgRecordState.INSTANTIATION_PENDING

        self._log.info("Instantiated VNFFGR :%s", vnffgr)
        self._vnffgr_state = VnffgRecordState.ACTIVE

        self._log.info("Invoking update_state to update NSR state for NSR ID: %s", self._nsr.id)
        yield from self._nsr.update_state()

    def vnffgr_in_vnffgrm(self):
        """ Is there a VNFR record in VNFM """
        if (self._vnffgr_state == VnffgRecordState.ACTIVE or
                self._vnffgr_state == VnffgRecordState.INSTANTIATION_PENDING or
                self._vnffgr_state == VnffgRecordState.FAILED):
            return True

        return False

    @asyncio.coroutine
    def terminate(self):
        """ Terminate this VNFFGR """
        if not self.vnffgr_in_vnffgrm():
            self._log.error("Ignoring terminate request for id %s in state %s",
                            self.id, self._vnffgr_state)
            return

        self._log.info("Terminating VNFFGR id:%s", self.id)
        self._vnffgr_state = VnffgRecordState.TERMINATE_PENDING

        self._vnffgmgr.terminate_vnffgr(self._vnffgr_id)

        self._vnffgr_state = VnffgRecordState.TERMINATED
        self._log.debug("Terminated VNFFGR id:%s", self.id)


class VirtualLinkRecord(object):
    """ Virtual Link Records class"""
    @staticmethod
    @asyncio.coroutine
    def create_record(dts, log, loop, nsr_name, vld_msg, cloud_account_name, nsr_id, restart_mode=False):
        """Creates a new VLR object based on the given data.

        If restart mode is enabled, then we look for existing records in the
        DTS and create a VLR records using the exiting data(ID)

        Returns:
            VirtualLinkRecord
        """
        vlr_obj = VirtualLinkRecord(
                      dts,
                      log,
                      loop,
                      nsr_name,
                      vld_msg,
                      cloud_account_name,
                      nsr_id,
                      )

        if restart_mode:
            res_iter = yield from dts.query_read(
                              "D,/vlr:vlr-catalog/vlr:vlr",
                              rwdts.XactFlag.MERGE)

            for fut in res_iter:
                response = yield from fut
                vlr = response.result

                # Check if the record is already present, if so use the ID of
                # the existing record. Since the name of the record is uniquely
                # formed we can use it as a search key!
                if vlr.name == vlr_obj.name:
                    vlr_obj.reset_id(vlr.id)
                    break

        return vlr_obj

    def __init__(self, dts, log, loop, nsr_name, vld_msg, cloud_account_name, nsr_id):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsr_name = nsr_name
        self._vld_msg = vld_msg
        self._cloud_account_name = cloud_account_name
        self._assigned_subnet = None
        self._nsr_id = nsr_id

        self._vlr_id = str(uuid.uuid4())
        self._state = VlRecordState.INIT

    @property
    def xpath(self):
        """ path for this object """
        return "D,/vlr:vlr-catalog/vlr:vlr[vlr:id = '{}']".format(self._vlr_id)

    @property
    def id(self):
        """ VLR id """
        return self._vlr_id

    @property
    def nsr_name(self):
        """ Get NSR name for this VL """
        return self.nsr_name

    @property
    def vld_msg(self):
        """ Virtual Link Desciptor """
        return self._vld_msg

    @property
    def assigned_subnet(self):
        """ Subnet assigned to this VL"""
        return self._assigned_subnet

    @property
    def name(self):
        """
        Get the name for this VLR.
        VLR name is "nsr name:VLD name"
        """
        if self.vld_msg.name == "multisite":
            # This is a temporary hack to identify manually provisioned inter-site network
            return self.vld_msg.name
        else:
            return self._nsr_name + "." + self.vld_msg.name

    @property
    def cloud_account_name(self):
        """ Cloud account that this VLR should be created in """
        return self._cloud_account_name

    @staticmethod
    def vlr_xpath(vlr):
        """ Get the VLR path from VLR """
        return (VirtualLinkRecord.XPATH + "[vlr:id = '{}']").format(vlr.id)

    @property
    def vlr_msg(self):
        """ Virtual Link Record message for Creating VLR in VNS """
        vld_fields = ["short_name",
                      "vendor",
                      "description",
                      "version",
                      "type_yang",
                      "provider_network"]

        vld_copy_dict = {k: v for k, v in self.vld_msg.as_dict().items()
                         if k in vld_fields}
        vlr_dict = {"id": self._vlr_id,
                    "nsr_id_ref": self._nsr_id,
                    "name": self.name,
                    "cloud_account": self.cloud_account_name,
                    }

        vlr_dict.update(vld_copy_dict)

        vlr = RwVlrYang.YangData_Vlr_VlrCatalog_Vlr.from_dict(vlr_dict)
        return vlr

    def reset_id(self, vlr_id):
        self._vlr_id = vlr_id

    def create_nsr_vlr_msg(self, vnfrs):
        """ The VLR message"""
        nsr_vlr = RwNsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_Vlr()
        nsr_vlr.vlr_ref = self._vlr_id
        nsr_vlr.assigned_subnet = self.assigned_subnet

        for conn in self.vld_msg.vnfd_connection_point_ref:
            for vnfr in vnfrs:
                if (vnfr.vnfd.id == conn.vnfd_id_ref and
                        vnfr.member_vnf_index == conn.member_vnf_index_ref):
                    cp_entry = nsr_vlr.vnfr_connection_point_ref.add()
                    cp_entry.vnfr_id = vnfr.id
                    cp_entry.connection_point = conn.vnfd_connection_point_ref

        return nsr_vlr

    @asyncio.coroutine
    def instantiate(self):
        """ Instantiate this VL """

        self._log.debug("Instaniating VLR key %s, vld %s",
                        self.xpath, self._vld_msg)
        vlr = None
        self._state = VlRecordState.INSTANTIATION_PENDING
        self._log.debug("Executing VL create path:%s msg:%s",
                        self.xpath, self.vlr_msg)

        with self._dts.transaction(flags=0) as xact:
            block = xact.block_create()
            block.add_query_create(self.xpath, self.vlr_msg)
            self._log.debug("Executing VL create path:%s msg:%s",
                            self.xpath, self.vlr_msg)
            res_iter = yield from block.execute(now=True)
            for ent in res_iter:
                res = yield from ent
                vlr = res.result

            if vlr is None:
                self._state = VlRecordState.FAILED
                raise NsrInstantiationFailed("Failed NS %s instantiation due to empty response" % self.id)

        if vlr.operational_status == 'failed':
            self._log.debug("NS Id:%s VL creation failed for vlr id %s", self.id, vlr.id)
            self._state = VlRecordState.FAILED
            raise NsrInstantiationFailed("Failed NS %s instantiation due to VL %s failure" % (self.id, vlr.id))

        self._log.info("Instantiated VL with xpath %s and vlr:%s",
                       self.xpath, vlr)
        self._state = VlRecordState.ACTIVE
        self._assigned_subnet = vlr.assigned_subnet

    def vlr_in_vns(self):
        """ Is there a VLR record in VNS """
        if (self._state == VlRecordState.ACTIVE or
                self._state == VlRecordState.INSTANTIATION_PENDING or
                self._state == VlRecordState.FAILED):
            return True

        return False

    @asyncio.coroutine
    def terminate(self):
        """ Terminate this VL """
        if not self.vlr_in_vns():
            self._log.debug("Ignoring terminate request for id %s in state %s",
                            self.id, self._state)
            return

        self._log.debug("Terminating VL id:%s", self.id)
        self._state = VlRecordState.TERMINATE_PENDING

        with self._dts.transaction(flags=0) as xact:
            block = xact.block_create()
            block.add_query_delete(self.xpath)
            yield from block.execute(flags=0, now=True)

        self._state = VlRecordState.TERMINATED
        self._log.debug("Terminated VL id:%s", self.id)


class VnfRecordState(Enum):
    """ Vnf Record State """
    INIT = 101
    INSTANTIATION_PENDING = 102
    ACTIVE = 103
    TERMINATE_PENDING = 104
    TERMINATED = 105
    FAILED = 106


class VirtualNetworkFunctionRecord(object):
    """ Virtual Network Function Record class"""
    XPATH = "D,/vnfr:vnfr-catalog/vnfr:vnfr"

    @staticmethod
    @asyncio.coroutine
    def create_record(dts, log, loop, vnfd, const_vnfd_msg, nsd_id, nsr_name,
                cloud_account_name, nsr_id, group_name, group_instance_id,
                placement_groups, restart_mode=False):
        """Creates a new VNFR object based on the given data.

        If restart mode is enabled, then we look for existing records in the
        DTS and create a VNFR records using the exiting data(ID)

        Returns:
            VirtualNetworkFunctionRecord
        """
        vnfr_obj = VirtualNetworkFunctionRecord(
                          dts,
                          log,
                          loop,
                          vnfd,
                          const_vnfd_msg,
                          nsd_id,
                          nsr_name,
                          cloud_account_name,
                          nsr_id,
                          group_name,
                          group_instance_id,
                          placement_groups,
                          restart_mode=restart_mode)

        if restart_mode:
            res_iter = yield from dts.query_read(
                              "D,/vnfr:vnfr-catalog/vnfr:vnfr",
                              rwdts.XactFlag.MERGE)

            for fut in res_iter:
                response = yield from fut
                vnfr = response.result

                if vnfr.name == vnfr_obj.name:
                    vnfr_obj.reset_id(vnfr.id)
                    break

        return vnfr_obj

    def __init__(self,
                 dts,
                 log,
                 loop,
                 vnfd,
                 const_vnfd_msg,
                 nsd_id,
                 nsr_name,
                 cloud_account_name,
                 nsr_id,
                 group_name=None,
                 group_instance_id=None,
                 placement_groups = [],
                 restart_mode = False):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._vnfd = vnfd
        self._const_vnfd_msg = const_vnfd_msg
        self._nsd_id = nsd_id
        self._nsr_name = nsr_name
        self._nsr_id = nsr_id
        self._cloud_account_name = cloud_account_name
        self._group_name = group_name
        self._group_instance_id = group_instance_id
        self._placement_groups = placement_groups
        self._config_status = NsrYang.ConfigStates.INIT

        self._mon_params = {}
        self._prev_state = VnfRecordState.INIT
        self._state = VnfRecordState.INIT

        self.config_store = rift.mano.config_data.config.ConfigStore(self._log)
        self.configure()

        self._vnfr_id = str(uuid.uuid4())
        self._vnfr_msg = self.create_vnfr_msg()
        self._log.debug("Set VNFR {} config type to {}".
                        format(self.name, self.config_type))
        self.restart_mode = restart_mode


        if group_name is None and group_instance_id is not None:
            raise ValueError("Group instance id must not be provided with an empty group name")

    @property
    def id(self):
        """ VNFR id """
        return self._vnfr_id

    @property
    def xpath(self):
        """ VNFR xpath """
        return "D,/vnfr:vnfr-catalog/vnfr:vnfr[vnfr:id = '{}']".format(self.id)

    @property
    def mon_param_xpath(self):
        """ VNFR monitoring param xpath """
        return self.xpath + "/vnfr:monitoring-param"

    @property
    def vnfr_msg(self):
        """ VNFR message """
        return self._vnfr_msg

    @property
    def const_vnfr_msg(self):
        """ VNFR message """
        return NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_ConstituentVnfrRef(vnfr_id=self.id)

    @property
    def vnfd(self):
        """ vnfd """
        return self._vnfd

    @property
    def active(self):
        """ Is this VNF actve """
        return True if self._state == VnfRecordState.ACTIVE else False

    @property
    def state(self):
        """ state of this VNF """
        return self._state

    @property
    def member_vnf_index(self):
        """ Member VNF index """
        return self._const_vnfd_msg.member_vnf_index

    @property
    def nsr_name(self):
        """ NSR name"""
        return self._nsr_name

    @property
    def name(self):
        """ Name of this VNFR """
        name_tags = [self._nsr_name]
        if self._group_name is not None:
            name_tags.append(self._group_name)

        if self._group_instance_id is not None:
            name_tags.append(str(self._group_instance_id))

        name_tags.extend([self.vnfd.name, str(self.member_vnf_index)])

        return ".".join(name_tags)

    @staticmethod
    def vnfr_xpath(vnfr):
        """ Get the VNFR path from VNFR """
        return (VirtualNetworkFunctionRecord.XPATH + "[vnfr:id = '{}']").format(vnfr.id)

    @property
    def config_type(self):
        cfg_types = ['netconf', 'juju', 'script']
        for method in cfg_types:
            if self._vnfd.mgmt_interface.vnf_configuration.has_field(method):
                return method
        return 'none'

    @property
    def config_status(self):
        """Return the config status as YANG ENUM string"""
        self._log.debug("Map VNFR {} config status {} ({})".
                        format(self.name, self._config_status, self.config_type))
        if self.config_type == 'none':
            return 'config_not_needed'
        elif self._config_status == NsrYang.ConfigStates.CONFIGURED:
            return 'configured'
        elif self._config_status == NsrYang.ConfigStates.FAILED:
            return 'failed'

        return 'configuring'

    def set_state(self, state):
        """ set the state of this object """
        self._prev_state = self._state
        self._state = state

    def reset_id(self, vnfr_id):
        self._vnfr_id = vnfr_id
        self._vnfr_msg = self.create_vnfr_msg()

    def configure(self):
        self.config_store.merge_vnfd_config(
                    self._nsd_id,
                    self._vnfd,
                    self.member_vnf_index,
                    )

    def create_vnfr_msg(self):
        """ VNFR message for this VNFR """
        vnfd_fields = [
                "short_name",
                "vendor",
                "description",
                "version",
                "type_yang",
                ]
        vnfd_copy_dict = {k: v for k, v in self._vnfd.as_dict().items() if k in vnfd_fields}
        vnfr_dict = {
                "id": self.id,
                "nsr_id_ref": self._nsr_id,
                "vnfd_ref": self.vnfd.id,
                "name": self.name,
                "cloud_account": self._cloud_account_name,
                "config_status": self.config_status
                }
        vnfr_dict.update(vnfd_copy_dict)

        vnfr = RwVnfrYang.YangData_Vnfr_VnfrCatalog_Vnfr.from_dict(vnfr_dict)
        vnfr.member_vnf_index_ref = self.member_vnf_index
        vnfr.vnf_configuration.from_dict(self._vnfd.mgmt_interface.vnf_configuration.as_dict())

        if self._vnfd.mgmt_interface.has_field("port"):
            vnfr.mgmt_interface.port = self._vnfd.mgmt_interface.port

        for group_info in self._placement_groups:
            group = vnfr.placement_groups_info.add()
            group.from_dict(group_info.as_dict())

        # UI expects the monitoring param field to exist
        vnfr.monitoring_param = []

        self._log.debug("Get vnfr_msg for VNFR {} : {}".format(self.name, vnfr))
        return vnfr

    @asyncio.coroutine
    def update_vnfm(self):
        self._log.debug("Send an update to VNFM for VNFR {} with {}".
                        format(self.name, self.vnfr_msg))
        yield from self._dts.query_update(
                self.xpath,
                0,
                self.vnfr_msg
                )

    def get_config_status(self):
        """Return the config status as YANG ENUM"""
        return self._config_status

    @asyncio.coroutine
    def set_config_status(self, status):

        def status_to_string(status):
            status_dc = {
                NsrYang.ConfigStates.INIT : 'init',
                NsrYang.ConfigStates.CONFIGURING : 'configuring',
                NsrYang.ConfigStates.CONFIG_NOT_NEEDED : 'config_not_needed',
                NsrYang.ConfigStates.CONFIGURED : 'configured',
                NsrYang.ConfigStates.FAILED : 'failed',
            }

            return status_dc[status]

        self._log.debug("Update VNFR {} from {} ({}) to {}".
                        format(self.name, self._config_status,
                               self.config_type, status))
        if self._config_status == NsrYang.ConfigStates.CONFIGURED:
            self._log.error("Updating already configured VNFR {}".
                            format(self.name))
            return

        if self._config_status != status:
            try:
                self._config_status = status
                # I don't think this is used. Original implementor can check.
                # Caused Exception, so corrected it by status_to_string
                # But not sure whats the use of this variable?
                self.vnfr_msg.config_status = status_to_string(status)
            except Exception as e:
                self._log.error("Exception=%s", str(e))
                pass

            self._log.debug("Updated VNFR {} status to {}".format(self.name, status))

            if self._config_status != NsrYang.ConfigStates.INIT:
                try:
                    # Publish only after VNFM has the VNFR created
                    yield from self.update_vnfm()
                except Exception as e:
                    self._log.error("Exception updating VNFM with new status {} of VNFR {}: {}".
                                format(status, self.name, e))
                    self._log.exception(e)

    def is_configured(self):
        if self.config_type == 'none':
            return True

        if self._config_status == NsrYang.ConfigStates.CONFIGURED:
            return True

        return False

    @asyncio.coroutine
    def instantiate(self, nsr):
        """ Instantiate this VNFR"""

        self._log.debug("Instaniating VNFR key %s, vnfd %s",
                        self.xpath, self._vnfd)

        self._log.debug("Create VNF with xpath %s and vnfr %s",
                        self.xpath, self.vnfr_msg)

        self.set_state(VnfRecordState.INSTANTIATION_PENDING)

        def find_vlr_for_cp(conn):
            """ Find VLR for the given connection point """
            for vlr in nsr.vlrs:
                for vnfd_cp in vlr.vld_msg.vnfd_connection_point_ref:
                    if (vnfd_cp.vnfd_id_ref == self._vnfd.id and
                            vnfd_cp.vnfd_connection_point_ref == conn.name and
                            vnfd_cp.member_vnf_index_ref == self.member_vnf_index):
                        self._log.debug("Found VLR for cp_name:%s and vnf-index:%d",
                                        conn.name, self.member_vnf_index)
                        return vlr
            return None

        # For every connection point in the VNFD fill in the identifier
        for conn_p in self._vnfd.connection_point:
            cpr = VnfrYang.YangData_Vnfr_VnfrCatalog_Vnfr_ConnectionPoint()
            cpr.name = conn_p.name
            cpr.type_yang = conn_p.type_yang
            vlr_ref = find_vlr_for_cp(conn_p)
            if vlr_ref is None:
                msg = "Failed to find VLR for cp = %s" % conn_p.name
                self._log.debug("%s", msg)
#                raise VirtualNetworkFunctionRecordError(msg)
                continue

            cpr.vlr_ref = vlr_ref.id
            self.vnfr_msg.connection_point.append(cpr)
            self._log.debug("Connection point [%s] added, vnf id=%s vnfd id=%s",
                            cpr, self.vnfr_msg.id, self.vnfr_msg.vnfd_ref)

        if not self.restart_mode:
            yield from self._dts.query_create(self.xpath,
                                              0,   # this is sub
                                              self.vnfr_msg)
        else:
            yield from self._dts.query_update(self.xpath,
                                              0,
                                              self.vnfr_msg)

        self._log.info("Created VNF with xpath %s and vnfr %s",
                       self.xpath, self.vnfr_msg)

        self._log.info("Instantiated VNFR with xpath %s and vnfd %s, vnfr %s",
                       self.xpath, self._vnfd, self.vnfr_msg)

    @asyncio.coroutine
    def update_state(self, vnfr_msg):
        """ Update this VNFR"""
        if vnfr_msg.operational_status == "running":
            if self.vnfr_msg.operational_status != "running":
                yield from self.is_active()
        elif vnfr_msg.operational_status == "failed":
            yield from self.instantiation_failed()

    @asyncio.coroutine
    def is_active(self):
        """ This VNFR is active """
        self._log.debug("VNFR %s is active", self._vnfr_id)
        self.set_state(VnfRecordState.ACTIVE)

    @asyncio.coroutine
    def instantiation_failed(self):
        """ This VNFR instantiation failed"""
        self._log.error("VNFR %s instantiation failed", self._vnfr_id)
        self.set_state(VnfRecordState.FAILED)

    def vnfr_in_vnfm(self):
        """ Is there a VNFR record in VNFM """
        if (self._state == VnfRecordState.ACTIVE or
                self._state == VnfRecordState.INSTANTIATION_PENDING or
                self._state == VnfRecordState.FAILED):
            return True

        return False

    @asyncio.coroutine
    def terminate(self):
        """ Terminate this VNF """
        if not self.vnfr_in_vnfm():
            self._log.debug("Ignoring terminate request for id %s in state %s",
                            self.id, self._state)
            return

        self._log.debug("Terminating VNF id:%s", self.id)
        self.set_state(VnfRecordState.TERMINATE_PENDING)
        with self._dts.transaction(flags=0) as xact:
            block = xact.block_create()
            block.add_query_delete(self.xpath)
            yield from block.execute(flags=0)
        self.set_state(VnfRecordState.TERMINATED)
        self._log.debug("Terminated VNF id:%s", self.id)

    @asyncio.coroutine
    def get_monitoring_param(self):
        """ Fetch monitoring params """
        res_iter = yield from self._dts.query_read(self.mon_param_xpath, rwdts.XactFlag.MERGE)
        monp_list = []
        for ent in res_iter:
            res = yield from ent
            monp = res.result
            if monp.id in self._mon_params:
                if monp.has_field("value_integer"):
                    self._mon_params[monp.id].value_integer = monp.value_integer
                if monp.has_field("value_decimal"):
                    self._mon_params[monp.id].value_decimal = monp.value_decimal
                if monp.has_field("value_string"):
                    self._mon_params[monp.id].value_string = monp.value_string
            else:
                self._mon_params[monp.id] = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_VnfMonitoringParam_MonitoringParam.from_dict(monp.as_dict())
            monp_list.append(self._mon_params[monp.id])
        return monp_list


class NetworkServiceStatus(object):
    """ A class representing the Network service's status """
    MAX_EVENTS_RECORDED = 10
    """ Network service Status class"""
    def __init__(self, dts, log, loop):
        self._dts = dts
        self._log = log
        self._loop = loop

        self._state = NetworkServiceRecordState.INIT
        self._events = deque([])

    @asyncio.coroutine
    def create_notification(self, evt, evt_desc):
        xp = "N,/rw-nsr:nsm-notification"
        notif = RwNsrYang.YangNotif_RwNsr_NsmNotification()
        notif.event = evt
        notif.description = evt_desc

        yield from self._dts.query_create(xp, rwdts.XactFlag.ADVISE, notif)
        self._log.info("Notification called by creating dts query: %s", notif)

    def record_event(self, evt, evt_desc):
        """ Record an event """
        self._log.debug("Recording event - evt %s, evt_descr %s len = %s",
                        evt, evt_desc, len(self._events))
        if len(self._events) >= NetworkServiceStatus.MAX_EVENTS_RECORDED:
            self._events.popleft()
        self._events.append((int(time.time()), evt, evt_desc))

        self._loop.create_task(self.create_notification(evt,evt_desc))

    def set_state(self, state):
        """ set the state of this status object """
        self._state = state

    def yang_str(self):
        """ Return the state as a yang enum string """
        state_to_str_map = {"INIT": "init",
                            "VL_INIT_PHASE": "vl_init_phase",
                            "VNF_INIT_PHASE": "vnf_init_phase",
                            "VNFFG_INIT_PHASE": "vnffg_init_phase",
                            "SCALING_GROUP_INIT_PHASE": "scaling_group_init_phase",
                            "RUNNING": "running",
                            "SCALING_OUT": "scaling_out",
                            "SCALING_IN": "scaling_in",
                            "TERMINATE_RCVD": "terminate_rcvd",
                            "TERMINATE": "terminate",
                            "VL_TERMINATE_PHASE": "vl_terminate_phase",
                            "VNF_TERMINATE_PHASE": "vnf_terminate_phase",
                            "VNFFG_TERMINATE_PHASE": "vnffg_terminate_phase",
                            "TERMINATED": "terminated",
                            "FAILED": "failed"}
        return state_to_str_map[self._state.name]

    @property
    def state(self):
        """ State of this status object """
        return self._state

    @property
    def msg(self):
        """ Network Service Record as a message"""
        event_list = []
        idx = 1
        for entry in self._events:
            event = RwNsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_OperationalEvents()
            event.id = idx
            idx += 1
            event.timestamp, event.event, event.description = entry
            event_list.append(event)
        return event_list


class NetworkServiceRecord(object):
    """ Network service record """
    XPATH = "D,/nsr:ns-instance-opdata/nsr:nsr"

    def __init__(self, dts, log, loop, nsm, nsm_plugin, nsr_cfg_msg, sdn_account_name, restart_mode=False):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm
        self._nsr_cfg_msg = nsr_cfg_msg
        self._nsm_plugin = nsm_plugin
        self._sdn_account_name = sdn_account_name

        self._nsd = None
        self._nsr_msg = None
        self._nsr_regh = None
        self._vlrs = []
        self._vnfrs = {}
        self._vnfds = {}
        self._vnffgrs = {}
        self._param_pools = {}
        self._scaling_groups = {}
        self._create_time = int(time.time())
        self._op_status = NetworkServiceStatus(dts, log, loop)
        self._mon_params = defaultdict(NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_VnfMonitoringParam)
        self._config_status = NsrYang.ConfigStates.CONFIGURING
        self._job_id = 0
        self.restart_mode = restart_mode
        self.config_store = rift.mano.config_data.config.ConfigStore(self._log)
        self._debug_running = False
        self._is_active = False
        self._vl_phase_completed = False
        self._vnf_phase_completed = False


        # Initalise the state to init
        # The NSR moves through the following transitions
        # 1. INIT -> VLS_READY once all the VLs in the NSD are created
        # 2. VLS_READY - VNFS_READY when all the VNFs in the NSD are created
        # 3. VNFS_READY - READY when the NSR is published

        self.set_state(NetworkServiceRecordState.INIT)

        self.substitute_input_parameters = InputParameterSubstitution(self._log)

    @property
    def nsm_plugin(self):
        """ NSM Plugin """
        return self._nsm_plugin

    def set_state(self, state):
        """ Set state for this NSR"""
        self._log.debug("Setting state to %s", state)
        # We are in init phase and is moving to the next state
        # The new state could be a FAILED state or VNF_INIIT_PHASE
        if self.state == NetworkServiceRecordState.VL_INIT_PHASE:
            self._vl_phase_completed = True

        if self.state == NetworkServiceRecordState.VNF_INIT_PHASE:
            self._vnf_phase_completed = True

        self._op_status.set_state(state)

    @property
    def id(self):
        """ Get id for this NSR"""
        return self._nsr_cfg_msg.id

    @property
    def name(self):
        """ Name of this network service record """
        return self._nsr_cfg_msg.name

    @property
    def nsd_id(self):
        """ Get nsd id for this NSR"""
        return self._nsr_cfg_msg.nsd_ref

    @property
    def cloud_account_name(self):
        return self._nsr_cfg_msg.cloud_account

    @property
    def state(self):
        """State of this NetworkServiceRecord"""
        return self._op_status.state

    @property
    def active(self):
        """ Is this NSR active ?"""
        return True if self._op_status.state == NetworkServiceRecordState.RUNNING else False

    @property
    def vlrs(self):
        """ VLRs associated with this NSR"""
        return self._vlrs

    @property
    def vnfrs(self):
        """ VNFRs associated with this NSR"""
        return self._vnfrs

    @property
    def vnffgrs(self):
        """ VNFFGRs associated with this NSR"""
        return self._vnffgrs

    @property
    def scaling_groups(self):
        """ Scaling groups associated with this NSR """
        return self._scaling_groups

    @property
    def param_pools(self):
        """ Parameter value pools associated with this NSR"""
        return self._param_pools

    @property
    def nsd(self):
        """ NSD for this NSR """
        return self._nsd

    @property
    def nsd_msg(self):
        return self._nsd.msg

    @property
    def job_id(self):
        ''' Get a new job id for config primitive'''
        self._job_id += 1
        return self._job_id

    @property
    def config_status(self):
        """ Config status for NSR """
        return self._config_status

    def resolve_placement_group_cloud_construct(self, input_group):
        """
        Returns the cloud specific construct for placement group
        """
        copy_dict = ['name', 'requirement', 'strategy']

        for group_info in self._nsr_cfg_msg.nsd_placement_group_maps:
            if group_info.placement_group_ref == input_group.name:
                group = VnfrYang.YangData_Vnfr_VnfrCatalog_Vnfr_PlacementGroupsInfo()
                group_dict = {k:v for k,v in
                              group_info.as_dict().items() if k != 'placement_group_ref'}
                for param in copy_dict:
                    group_dict.update({param: getattr(input_group, param)})
                group.from_dict(group_dict)
                return group
        return None


    def __str__(self):
        return "NSR(name={}, nsd_id={}, cloud_account={})".format(
                self.name, self.nsd_id, self.cloud_account_name
                )

    def _get_vnfd(self, vnfd_id, config_xact):
        """  Fetch vnfd msg for the passed vnfd id """
        return self._nsm.get_vnfd(vnfd_id, config_xact)

    def _get_constituent_vnfd_msg(self, vnf_index):
        for const_vnfd in self.nsd_msg.constituent_vnfd:
            if const_vnfd.member_vnf_index == vnf_index:
                return const_vnfd

        raise ValueError("Constituent VNF index %s not found" % vnf_index)

    def record_event(self, evt, evt_desc, state=None):
        """ Record an event """
        self._op_status.record_event(evt, evt_desc)
        if state is not None:
            self.set_state(state)

    def scaling_trigger_str(self, trigger):
        SCALING_TRIGGER_STRS = {
            NsdYang.ScalingTrigger.PRE_SCALE_IN : 'pre-scale-in',
            NsdYang.ScalingTrigger.POST_SCALE_IN : 'post-scale-in',
            NsdYang.ScalingTrigger.PRE_SCALE_OUT : 'pre-scale-out',
            NsdYang.ScalingTrigger.POST_SCALE_OUT : 'post-scale-out',
        }
        try:
            return SCALING_TRIGGER_STRS[trigger]
        except Exception as e:
            self._log.error("Scaling trigger mapping error for {} : {}".
                            format(trigger, e))
            self._log.exception(e)
            return "Unknown trigger"

    @asyncio.coroutine
    def instantiate_vls(self):
        """
        This function instantiates VLs for every VL in this Network Service
        """
        self._log.debug("Instantiating %d VLs in NSD id %s", len(self._vlrs),
                        self.id)
        for vlr in self._vlrs:
            yield from self.nsm_plugin.instantiate_vl(self, vlr)

    @asyncio.coroutine
    def create(self, config_xact):
        """ Create this network service"""
        # Create virtual links  for all the external vnf
        # connection points in this NS
        yield from self.create_vls()

        # Create VNFs in this network service
        yield from self.create_vnfs(config_xact)

        # Create VNFFG for network service
        self.create_vnffgs()

        # Create Scaling Groups for each scaling group in NSD
        self.create_scaling_groups()

        # Create Parameter Pools
        self.create_param_pools()

    @asyncio.coroutine
    def apply_scale_group_config_script(self, script, group, scale_instance, trigger, vnfrs=None):
        """ Apply config based on script for scale group """

        @asyncio.coroutine
        def add_vnfrs_data(vnfrs_list):
            """ Add as a dict each of the VNFRs data """
            vnfrs_data = []
            for vnfr in vnfrs_list:
                self._log.debug("Add VNFR {} data".format(vnfr))
                vnfr_data = dict()
                vnfr_data['name'] = vnfr.name
                if trigger in [NsdYang.ScalingTrigger.PRE_SCALE_IN, NsdYang.ScalingTrigger.POST_SCALE_OUT]:
                    # Get VNF management and other IPs, etc
                    opdata = yield from self.fetch_vnfr(vnfr.xpath)
                    self._log.debug("VNFR {} op data: {}".format(vnfr.name, opdata))
                    try:
                        vnfr_data['rw_mgmt_ip'] = opdata.mgmt_interface.ip_address
                        vnfr_data['rw_mgmt_port'] = opdata.mgmt_interface.port
                    except Exception as e:
                        self._log.error("Unable to get management IP for vnfr {}:{}".
                                        format(vnfr.name, e))

                    try:
                        vnfr_data['connection_points'] = []
                        for cp in opdata.connection_point:
                            con_pt = dict()
                            con_pt['name'] = cp.name
                            con_pt['ip_address'] = cp.ip_address
                            vnfr_data['connection_points'].append(con_pt)
                    except Exception as e:
                        self._log.error("Exception getting connections points for VNFR {}: {}".
                                        format(vnfr.name, e))

                vnfrs_data.append(vnfr_data)
                self._log.debug("VNFRs data: {}".format(vnfrs_data))

            return vnfrs_data

        def add_nsr_data(nsr):
            nsr_data = dict()
            nsr_data['name'] = nsr.name
            return nsr_data

        if script is None or len(script) == 0:
            self._log.error("Script not provided for scale group config: {}".format(group.name))
            return False

        if script[0] == '/':
            path = script
        else:
            path = os.path.join(os.environ['RIFT_INSTALL'], "usr/bin", script)
        if not os.path.exists(path):
            self._log.error("Config faled for scale group {}: Script does not exist at {}".
                            format(group.name, path))
            return False

        # Build a YAML file with all parameters for the script to execute
        # The data consists of 5 sections
        # 1. Trigger
        # 2. Scale group config
        # 3. VNFRs in the scale group
        # 4. VNFRs outside scale group
        # 5. NSR data
        data = dict()
        data['trigger'] = group.trigger_map(trigger)
        data['config'] = group.group_msg.as_dict()

        if vnfrs:
            data["vnfrs_in_group"] = yield from add_vnfrs_data(vnfrs)
        else:
            data["vnfrs_in_group"] = yield from add_vnfrs_data(scale_instance.vnfrs)

        data["vnfrs_others"] = yield from add_vnfrs_data(self.vnfrs.values())
        data["nsr"] = add_nsr_data(self)

        tmp_file = None
        with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
            tmp_file.write(yaml.dump(data, default_flow_style=True)
                    .encode("UTF-8"))

        self._log.debug("Creating a temp file: {} with input data: {}".
                        format(tmp_file.name, data))

        cmd = "{} {}".format(path, tmp_file.name)
        self._log.debug("Running the CMD: {}".format(cmd))
        proc = yield from asyncio.create_subprocess_shell(cmd, loop=self._loop)
        rc = yield from proc.wait()
        if rc:
            self._log.error("The script {} for scale group {} config returned: {}".
                            format(script, group.name, rc))
            return False

        # Success
        return True


    @asyncio.coroutine
    def apply_scaling_group_config(self, trigger, group, scale_instance, vnfrs=None):
        """ Apply the config for the scaling group based on trigger """
        if group is None or scale_instance is None:
            return False

        @asyncio.coroutine
        def update_config_status(success=True, err_msg=None):
            self._log.debug("Update %s config status to %r : %s",
                            scale_instance, success, err_msg)
            if (scale_instance.config_status == "failed"):
                # Do not update the config status if it is already in failed state
                return

            if scale_instance.config_status == "configured":
                # Update only to failed state an already configured scale instance
                if not success:
                    scale_instance.config_status = "failed"
                    scale_instance.config_err_msg = err_msg
                    yield from self.update_state()
            else:
                # We are in configuring state
                # Only after post scale out mark instance as configured
                if trigger == NsdYang.ScalingTrigger.POST_SCALE_OUT:
                    if success:
                        scale_instance.config_status = "configured"
                    else:
                        scale_instance.config_status = "failed"
                        scale_instance.config_err_msg = err_msg
                    yield from self.update_state()

        config = group.trigger_config(trigger)
        if config is None:
            return True

        self._log.debug("Scaling group {} config: {}".format(group.name, config))
        if config.has_field("ns_config_primitive_name_ref"):
            config_name = config.ns_config_primitive_name_ref
            nsd_msg = self._nsm.get_nsd(self.nsd_id).msg
            config_primitive = None
            for ns_cfg_prim in nsd_msg.config_primitive:
                if ns_cfg_prim.name == config_name:
                    config_primitive = ns_cfg_prim
                    break

            if config_primitive is None:
                raise ValueError("Could not find ns_cfg_prim %s in nsr %s" % (config_name, self.name))

            self._log.debug("Scaling group {} config primitive: {}".format(group.name, config_primitive))
            if config_primitive.has_field("user_defined_script"):
                rc = yield from self.apply_scale_group_config_script(config_primitive.user_defined_script,
                                                                     group, scale_instance, trigger, vnfrs)
                err_msg = None
                if not rc:
                    err_msg = "Failed config for trigger {} using config script '{}'". \
                              format(self.scaling_trigger_str(trigger),
                                     config_primitive.user_defined_script)
                yield from update_config_status(success=rc, err_msg=err_msg)
                return rc
            else:
                err_msg = "Failed config for trigger {} as config script is not specified". \
                          format(self.scaling_trigger_str(trigger))
                yield from update_config_status(success=False, err_msg=err_msg)
                raise NotImplementedError("Only script based config support for scale group for now: {}".
                                          format(group.name))
        else:
            err_msg = "Failed config for trigger {} as config primitive is not specified".\
                      format(self.scaling_trigger_str(trigger))
            yield from update_config_status(success=False, err_msg=err_msg)
            self._log.error("Config primitive not specified for config action in scale group %s" %
                            (group.name))
        return False

    def create_scaling_groups(self):
        """ This function creates a NSScalingGroup for every scaling
        group defined in he NSD"""

        for scaling_group_msg in self.nsd_msg.scaling_group_descriptor:
            self._log.debug("Found scaling_group %s in nsr id %s",
                            scaling_group_msg.name, self.id)

            group_record = scale_group.ScalingGroup(
                    self._log,
                    scaling_group_msg
                    )

            self._scaling_groups[group_record.name] = group_record

    @asyncio.coroutine
    def create_scale_group_instance(self, group_name, index, config_xact, is_default=False):
        group = self._scaling_groups[group_name]
        scale_instance = group.create_instance(index, is_default)

        @asyncio.coroutine
        def create_vnfs():
            self._log.debug("Creating %u VNFs associated with NS id %s scaling group %s",
                            len(self.nsd_msg.constituent_vnfd), self.id, self)

            vnfrs = []
            for vnf_index, count in group.vnf_index_count_map.items():
                const_vnfd_msg = self._get_constituent_vnfd_msg(vnf_index)
                vnfd_msg = self._get_vnfd(const_vnfd_msg.vnfd_id_ref, config_xact)
                for _ in range(count):
                    vnfr = yield from self.create_vnf_record(vnfd_msg, const_vnfd_msg, group_name, index)
                    scale_instance.add_vnfr(vnfr)
                    vnfrs.append(vnfr)

            return vnfrs

        @asyncio.coroutine
        def instantiate_instance():
            self._log.debug("Creating %s VNFRS", scale_instance)
            vnfrs = yield from create_vnfs()
            yield from self.publish()

            self._log.debug("Instantiating %s VNFRS for %s", len(vnfrs), scale_instance)
            scale_instance.operational_status = "vnf_init_phase"
            yield from self.update_state()

            try:
                rc = yield from self.apply_scaling_group_config(NsdYang.ScalingTrigger.PRE_SCALE_OUT,
                                                                group, scale_instance, vnfrs)
                if not rc:
                    self._log.error("Pre scale out config for scale group {} ({}) failed".
                                    format(group.name, index))
                    scale_instance.operational_status = "failed"
                else:
                    yield from self.instantiate_vnfs(vnfrs)

            except Exception as e:
                self._log.exception("Failed to begin instantiatiation of vnfs for scale group {}: {}".
                                    format(group.name, e))
                self._log.exception(e)
                scale_instance.operational_status = "failed"

            yield from self.update_state()

        yield from instantiate_instance()

    @asyncio.coroutine
    def delete_scale_group_instance(self, group_name, index):
        group = self._scaling_groups[group_name]
        scale_instance = group.get_instance(index)
        if scale_instance.is_default:
            raise ScalingOperationError("Cannot terminate a default scaling group instance")

        scale_instance.operational_status = "terminate"
        yield from self.update_state()

        @asyncio.coroutine
        def terminate_instance():
            self._log.debug("Terminating %s VNFRS" % scale_instance)
            rc = yield from self.apply_scaling_group_config(NsdYang.ScalingTrigger.PRE_SCALE_IN,
                                                            group, scale_instance)
            if not rc:
                self._log.error("Pre scale in config for scale group {} ({}) failed".
                                format(group.name, index))

            # Going ahead with terminate, even if there is an error in pre-scale-in config
            # as this could be result of scale out failure and we need to cleanup this group
            yield from self.terminate_vnfrs(scale_instance.vnfrs)
            group.delete_instance(index)

            scale_instance.operational_status = "vnf_terminate_phase"
            yield from self.update_state()

        yield from terminate_instance()

    @asyncio.coroutine
    def _update_scale_group_instances_status(self):
        @asyncio.coroutine
        def post_scale_out_task(group, instance):
            # Apply post scale out config once all VNFRs are active
            rc = yield from self.apply_scaling_group_config(NsdYang.ScalingTrigger.POST_SCALE_OUT,
                                                            group, instance)
            instance.operational_status = "running"
            if rc:
                self._log.debug("Scale out for group {} and instance {} succeeded".
                                format(group.name, instance.instance_id))
            else:
                self._log.error("Post scale out config for scale group {} ({}) failed".
                                format(group.name, instance.instance_id))

            yield from self.update_state()

        group_instances = {group: group.instances for group in self._scaling_groups.values()}
        for group, instances in group_instances.items():
            self._log.debug("Updating %s instance status", group)
            for instance in instances:
                instance_vnf_state_list = [vnfr.state for vnfr in instance.vnfrs]
                self._log.debug("Got vnfr instance states: %s", instance_vnf_state_list)
                if instance.operational_status == "vnf_init_phase":
                    if all([state == VnfRecordState.ACTIVE for state in instance_vnf_state_list]):
                        instance.operational_status = "running"

                        # Create a task for post scale out to allow us to sleep before attempting
                        # to configure newly created VM's
                        self._loop.create_task(post_scale_out_task(group, instance))

                    elif any([state == VnfRecordState.FAILED for state in instance_vnf_state_list]):
                        self._log.debug("Scale out for group {} and instance {} failed".
                                        format(group.name, instance.instance_id))
                        instance.operational_status = "failed"

                elif instance.operational_status == "vnf_terminate_phase":
                    if all([state == VnfRecordState.TERMINATED for state in instance_vnf_state_list]):
                        instance.operational_status = "terminated"
                        rc = yield from self.apply_scaling_group_config(NsdYang.ScalingTrigger.POST_SCALE_IN,
                                                                         group, instance)
                        if rc:
                            self._log.debug("Scale in for group {} and instance {} succeeded".
                                            format(group.name, instance.instance_id))
                        else:
                            self._log.error("Post scale in config for scale group {} ({}) failed".
                                            format(group.name, instance.instance_id))

    def create_vnffgs(self):
        """ This function creates VNFFGs for every VNFFG in the NSD
        associated with this NSR"""

        for vnffgd in self.nsd_msg.vnffgd:
            self._log.debug("Found vnffgd %s in nsr id %s", vnffgd, self.id)
            vnffgr = VnffgRecord(self._dts,
                                 self._log,
                                 self._loop,
                                 self._nsm._vnffgmgr,
                                 self,
                                 self.name,
                                 vnffgd,
                                 self._sdn_account_name
                                 )
            self._vnffgrs[vnffgr.id] = vnffgr

    @asyncio.coroutine
    def create_vls(self):
        """ This function creates VLs for every VLD in the NSD
        associated with this NSR"""

        for vld in self.nsd_msg.vld:
            self._log.debug("Found vld %s in nsr id %s", vld, self.id)
            vlr = yield from VirtualLinkRecord.create_record(self._dts,
                                    self._log,
                                    self._loop,
                                    self.name,
                                    vld,
                                    self.cloud_account_name,
                                    self.id,
                                    restart_mode=self.restart_mode,
                                    )
            self._vlrs.append(vlr)

    @asyncio.coroutine
    def create_vnfs(self, config_xact):
        """
        This function creates VNFs for every VNF in the NSD
        associated with this NSR
        """
        self._log.debug("Creating %u VNFs associated with this NS id %s",
                        len(self.nsd_msg.constituent_vnfd), self.id)

        for const_vnfd in self.nsd_msg.constituent_vnfd:
            if not const_vnfd.start_by_default:
                self._log.debug("start_by_default set to False in constituent VNF (%s). Skipping start.",
                                const_vnfd.member_vnf_index)
                continue

            vnfd_msg = self._get_vnfd(const_vnfd.vnfd_id_ref, config_xact)
            yield from self.create_vnf_record(vnfd_msg, const_vnfd)


    def get_placement_groups(self, vnfd_msg, const_vnfd):
        placement_groups = []
        for group in self.nsd_msg.placement_groups:
            for member_vnfd in group.member_vnfd:
                if (member_vnfd.vnfd_id_ref == vnfd_msg.id) and \
                   (member_vnfd.member_vnf_index_ref == const_vnfd.member_vnf_index):
                    group_info = self.resolve_placement_group_cloud_construct(group)
                    if group_info is None:
                        self._log.error("Could not resolve cloud-construct for placement group: %s", group.name)
                        ### raise PlacementGroupError("Could not resolve cloud-construct for placement group: {}".format(group.name))
                    else:
                        self._log.info("Successfully resolved cloud construct for placement group: %s for VNF: %s (Member Index: %s)",
                                       str(group_info),
                                       vnfd_msg.name,
                                       const_vnfd.member_vnf_index)
                        placement_groups.append(group_info)
        return placement_groups

    @asyncio.coroutine
    def create_vnf_record(self, vnfd_msg, const_vnfd, group_name=None, group_instance_id=None):
        # Fetch the VNFD associated with this VNF
        placement_groups = self.get_placement_groups(vnfd_msg, const_vnfd)
        self._log.info("Launching VNF: %s (Member Index: %s) in NSD plancement Groups: %s",
                       vnfd_msg.name,
                       const_vnfd.member_vnf_index,
                       [ group.name for group in placement_groups])
        vnfr = yield from VirtualNetworkFunctionRecord.create_record(self._dts,
                                            self._log,
                                            self._loop,
                                            vnfd_msg,
                                            const_vnfd,
                                            self.nsd_id,
                                            self.name,
                                            self.cloud_account_name,
                                            self.id,
                                            group_name,
                                            group_instance_id,
                                            placement_groups,
                                            restart_mode=self.restart_mode,
                                            )
        if vnfr.id in self._vnfrs:
            err = "VNF with VNFR id %s already in vnf list" % (vnfr.id,)
            raise NetworkServiceRecordError(err)

        self._vnfrs[vnfr.id] = vnfr
        self._nsm.vnfrs[vnfr.id] = vnfr

        yield from vnfr.set_config_status(NsrYang.ConfigStates.INIT)

        self._log.debug("Added VNFR %s to NSM VNFR list with id %s",
                        vnfr.name,
                        vnfr.id)

        return vnfr

    def create_param_pools(self):
        for param_pool in self.nsd_msg.parameter_pool:
            self._log.debug("Found parameter pool %s in nsr id %s", param_pool, self.id)

            start_value = param_pool.range.start_value
            end_value = param_pool.range.end_value
            if end_value < start_value:
                raise NetworkServiceRecordError(
                        "Parameter pool %s has invalid range (start: {}, end: {})".format(
                            start_value, end_value
                            )
                        )

            self._param_pools[param_pool.name] = config_value_pool.ParameterValuePool(
                    self._log,
                    param_pool.name,
                    range(start_value, end_value)
                    )

    @asyncio.coroutine
    def fetch_vnfr(self, vnfr_path):
        """ Fetch VNFR record """
        vnfr = None
        self._log.debug("Fetching VNFR with key %s while instantiating %s",
                        vnfr_path, self.id)
        res_iter = yield from self._dts.query_read(vnfr_path, rwdts.XactFlag.MERGE)

        for ent in res_iter:
            res = yield from ent
            vnfr = res.result

        return vnfr

    @asyncio.coroutine
    def instantiate_vnfs(self, vnfrs):
        """
        This function instantiates VNFs for every VNF in this Network Service
        """
        self._log.debug("Instantiating %u VNFs in NS %s", len(vnfrs), self.id)
        for vnf in vnfrs:
            self._log.debug("Instantiating VNF: %s in NS %s", vnf, self.id)
            yield from self.nsm_plugin.instantiate_vnf(self, vnf)

    @asyncio.coroutine
    def instantiate_vnffgs(self):
        """
        This function instantiates VNFFGs for every VNFFG in this Network Service
        """
        self._log.debug("Instantiating %u VNFFGs in NS %s",
                        len(self.nsd_msg.vnffgd), self.id)
        for _, vnfr in self.vnfrs.items():
            while vnfr.state in [VnfRecordState.INSTANTIATION_PENDING, VnfRecordState.INIT]:
                self._log.debug("Received vnfr state for vnfr %s is %s; retrying",vnfr.name,vnfr.state)
                yield from asyncio.sleep(2, loop=self._loop)
            if vnfr.state == VnfRecordState.ACTIVE:
                self._log.debug("Received vnfr state for vnfr %s is %s ",vnfr.name,vnfr.state)
                continue
            else:
                self._log.debug("Received vnfr state for vnfr %s is %s; failing vnffg creation",vnfr.name,vnfr.state)
                self._vnffgr_state = VnffgRecordState.FAILED
                return

        self._log.info("Waiting for 90 seconds for VMs to come up")
        yield from asyncio.sleep(90, loop=self._loop)
        self._log.info("Starting VNFFG orchestration")
        for vnffg in self._vnffgrs.values():
            self._log.debug("Instantiating VNFFG: %s in NS %s", vnffg, self.id)
            yield from vnffg.instantiate()

    @asyncio.coroutine
    def instantiate_scaling_instances(self, config_xact):
        """ Instantiate any default scaling instances in this Network Service """
        for group in self._scaling_groups.values():
            for i in range(group.min_instance_count):
                self._log.debug("Instantiating %s default scaling instance %s", group, i)
                yield from self.create_scale_group_instance(
                        group.name, i, config_xact, is_default=True
                        )

            for group_msg in self._nsr_cfg_msg.scaling_group:
                if group_msg.scaling_group_name_ref != group.name:
                    continue

                for instance in group_msg.instance:
                    self._log.debug("Reloading %s scaling instance %s", group_msg, instance.id)
                    yield from self.create_scale_group_instance(
                            group.name, instance.id, config_xact, is_default=False
                            )

    def has_scaling_instances(self):
        """ Return boolean indicating if the network service has default scaling groups """
        for group in self._scaling_groups.values():
            if group.min_instance_count > 0:
                return True

        for group_msg in self._nsr_cfg_msg.scaling_group:
            if len(group_msg.instance) > 0:
                return True

        return False

    @asyncio.coroutine
    def publish(self):
        """ This function publishes this NSR """
        self._nsr_msg = self.create_msg()

        self._log.debug("Publishing the NSR with xpath %s and nsr %s",
                        self.nsr_xpath,
                        self._nsr_msg)

        if self._debug_running:
            self._log.debug("Publishing NSR in RUNNING state!")
            #raise()

        with self._dts.transaction() as xact:
            yield from self._nsm.nsr_handler.update(xact, self.nsr_xpath, self._nsr_msg)
            if self._op_status.state == NetworkServiceRecordState.RUNNING:
                self._debug_running = True

    @asyncio.coroutine
    def unpublish(self, xact):
        """ Unpublish this NSR object """
        self._log.debug("Unpublishing Network service id %s", self.id)
        yield from self._nsm.nsr_handler.delete(xact, self.nsr_xpath)

    @property
    def nsr_xpath(self):
        """ Returns the xpath associated with this NSR """
        return(
            "D,/nsr:ns-instance-opdata" +
            "/nsr:nsr[nsr:ns-instance-config-ref = '{}']"
            ).format(self.id)

    @staticmethod
    def xpath_from_nsr(nsr):
        """ Returns the xpath associated with this NSR  op data"""
        return (NetworkServiceRecord.XPATH +
                "[nsr:ns-instance-config-ref = '{}']").format(nsr.id)

    @property
    def nsd_xpath(self):
        """ Return NSD config xpath."""
        return(
            "C,/nsd:nsd-catalog/nsd:nsd[nsd:id = '{}']"
            ).format(self.nsd_id)

    @asyncio.coroutine
    def instantiate(self, config_xact):
        """"Instantiates a NetworkServiceRecord.

        This function instantiates a Network service
        which involves the following steps,

        * Fetch the NSD associated with NSR from DTS.
        * Merge the NSD withe NSR config to begin instantiating the NS.
        * Instantiate every VL in NSD by sending create VLR request to DTS.
        * Instantiate every VNF in NSD by sending create VNF reuqest to DTS.
        * Publish the NSR details to DTS

        Arguments:
            nsr:  The NSR configuration request containing nsr-id and nsd_ref
            config_xact: The configuration transaction which initiated the instatiation

        Raises:
            NetworkServiceRecordError if the NSR creation fails

        Returns:
            No return value
        """

        self._log.debug("Instantiating NS - %s xact - %s", self, config_xact)

        # Move the state to INIITALIZING
        self.set_state(NetworkServiceRecordState.INIT)

        event_descr = "Instantiation Request Received NSR Id:%s" % self.id
        self.record_event("instantiating", event_descr)

        # Find the NSD
        self._nsd = self._nsm.get_nsd_ref(self.nsd_id)

        # Merge any config and initial config primitive values
        self.config_store.merge_nsd_config(self.nsd_msg)
        self._log.debug("Merged NSD: {}".format(self.nsd_msg.as_dict()))

        event_descr = "Fetched NSD with descriptor id %s" % self.nsd_id
        self.record_event("nsd-fetched", event_descr)

        if self._nsd is None:
            msg = "Failed to fetch NSD with nsd-id [%s] for nsr-id %s"
            self._log.debug(msg, self.nsd_id, self.id)
            raise NetworkServiceRecordError(self)

        self._log.debug("Got nsd result %s", self._nsd)

        # Sbustitute any input parameters
        self.substitute_input_parameters(self._nsd._nsd, self._nsr_cfg_msg)

        # Create the record
        yield from self.create(config_xact)

        # Publish the NSR to DTS
        yield from self.publish()

        @asyncio.coroutine
        def do_instantiate():
            """
                Instantiate network service
            """
            self._log.debug("Instantiating VLs nsr id [%s] nsd id [%s]",
                            self.id, self.nsd_id)

            # instantiate the VLs
            event_descr = ("Instantiating %s external VLs for NSR id %s" %
                           (len(self.nsd_msg.vld), self.id))
            self.record_event("begin-external-vls-instantiation", event_descr)

            self.set_state(NetworkServiceRecordState.VL_INIT_PHASE)

            yield from self.instantiate_vls()

            # Publish the NSR to DTS
            yield from self.publish()

            event_descr = ("Finished instantiating %s external VLs for NSR id %s" %
                           (len(self.nsd_msg.vld), self.id))
            self.record_event("end-external-vls-instantiation", event_descr)

            self.set_state(NetworkServiceRecordState.VNF_INIT_PHASE)

            self._log.debug("Instantiating VNFs  ...... nsr[%s], nsd[%s]",
                            self.id, self.nsd_id)

            # instantiate the VNFs
            event_descr = ("Instantiating %s VNFS for NSR id %s" %
                           (len(self.nsd_msg.constituent_vnfd), self.id))

            self.record_event("begin-vnf-instantiation", event_descr)

            yield from self.instantiate_vnfs(self._vnfrs.values())

            self._log.debug(" Finished instantiating %d VNFs for NSR id %s",
                            len(self.nsd.msg.constituent_vnfd), self.id)

            event_descr = ("Finished instantiating %s VNFs for NSR id %s" %
                           (len(self.nsd_msg.constituent_vnfd), self.id))
            self.record_event("end-vnf-instantiation", event_descr)

            if len(self.vnffgrs) > 0:
                #self.set_state(NetworkServiceRecordState.VNFFG_INIT_PHASE)
                event_descr = ("Instantiating %s VNFFGS for NSR id %s" %
                               (len(self.nsd.msg.vnffgd), self.id))

                self.record_event("begin-vnffg-instantiation", event_descr)

                yield from self.instantiate_vnffgs()

                event_descr = ("Finished instantiating %s VNFFGDs for NSR id %s" %
                               (len(self.nsd.msg.vnffgd), self.id))
                self.record_event("end-vnffg-instantiation", event_descr)

            if self.has_scaling_instances():
                event_descr = ("Instantiating %s Scaling Groups for NSR id %s" %
                               (len(self._scaling_groups), self.id))

                self.record_event("begin-scaling-group-instantiation", event_descr)
                yield from self.instantiate_scaling_instances(config_xact)
                self.record_event("end-scaling-group-instantiation", event_descr)

            # Give the plugin a chance to deploy the network service now that all
            # virtual links and vnfs are instantiated
            yield from self.nsm_plugin.deploy(self._nsr_msg)

            self._log.debug("Publishing  NSR...... nsr[%s], nsd[%s]",
                            self.id, self.nsd_id)

            # Publish the NSR to DTS
            yield from self.publish()

            self._log.debug("Published  NSR...... nsr[%s], nsd[%s]",
                            self.id, self.nsd_id)

        def on_instantiate_done(fut):
            # If the do_instantiate fails, then publish NSR with failed result
            if fut.exception() is not None:
                self._log.error("NSR instantiation failed for NSR id %s: %s", self.id, str(fut.exception()))
                self._loop.create_task(self.instantiation_failed())

        instantiate_task = self._loop.create_task(do_instantiate())
        instantiate_task.add_done_callback(on_instantiate_done)

    @asyncio.coroutine
    def set_config_status(self, status):
        if self.config_status != status:
            self._log.debug("Updating NSR {} status for {} to {}".
                            format(self.name, self.config_status, status))
            self._config_status = status
            yield from self.publish()

    @asyncio.coroutine
    def is_active(self):
        """ This NS is active """
        self.set_state(NetworkServiceRecordState.RUNNING)
        if self._is_active:
            return

        # Publish the NSR to DTS
        self._log.debug("Network service %s is active ", self.id)
        self._is_active = True

        event_descr = ("NSR in running state for NSR id %s" % self.id)
        self.record_event("ns-running", event_descr)

        yield from self.publish()

    @asyncio.coroutine
    def instantiation_failed(self):
        """ The NS instantiation failed"""
        self._log.error("Network service id:%s, name:%s instantiation failed",
                        self.id, self.name)
        self.set_state(NetworkServiceRecordState.FAILED)

        event_descr = "Instantiation of NS %s failed" % self.id
        self.record_event("ns-failed", event_descr)

        # Publish the NSR to DTS
        yield from self.publish()

    @asyncio.coroutine
    def terminate_vnfrs(self, vnfrs):
        """ Terminate VNFRS in this network service """
        self._log.debug("Terminating VNFs in network service %s", self.id)
        for vnfr in vnfrs:
            yield from self.nsm_plugin.terminate_vnf(vnfr)

    @asyncio.coroutine
    def terminate(self):
        """ Terminate a NetworkServiceRecord."""
        def terminate_vnffgrs():
            """ Terminate VNFFGRS in this network service """
            self._log.debug("Terminating VNFFGRs in network service %s", self.id)
            for vnffgr in self.vnffgrs.values():
                yield from vnffgr.terminate()

        def terminate_vlrs():
            """ Terminate VLRs in this netork service """
            self._log.debug("Terminating VLs in network service %s", self.id)
            for vlr in self.vlrs:
                yield from self.nsm_plugin.terminate_vl(vlr)

        self._log.debug("Terminating network service id %s", self.id)

        # Move the state to TERMINATE
        self.set_state(NetworkServiceRecordState.TERMINATE)
        event_descr = "Terminate being processed for NS Id:%s" % self.id
        self.record_event("terminate", event_descr)

        # Move the state to VNF_TERMINATE_PHASE
        self._log.debug("Terminating VNFFGs in NS ID: %s", self.id)
        self.set_state(NetworkServiceRecordState.VNFFG_TERMINATE_PHASE)
        event_descr = "Terminating VNFFGS in NS Id:%s" % self.id
        self.record_event("terminating-vnffgss", event_descr)
        yield from terminate_vnffgrs()

        # Move the state to VNF_TERMINATE_PHASE
        self.set_state(NetworkServiceRecordState.VNF_TERMINATE_PHASE)
        event_descr = "Terminating VNFS in NS Id:%s" % self.id
        self.record_event("terminating-vnfs", event_descr)
        yield from self.terminate_vnfrs(self.vnfrs.values())

        # Move the state to VL_TERMINATE_PHASE
        self.set_state(NetworkServiceRecordState.VL_TERMINATE_PHASE)
        event_descr = "Terminating VLs in NS Id:%s" % self.id
        self.record_event("terminating-vls", event_descr)
        yield from terminate_vlrs()

        yield from self.nsm_plugin.terminate_ns(self)

        # Move the state to TERMINATED
        self.set_state(NetworkServiceRecordState.TERMINATED)
        event_descr = "Terminated NS Id:%s" % self.id
        self.record_event("terminated", event_descr)

    def enable(self):
        """"Enable a NetworkServiceRecord."""
        pass

    def disable(self):
        """"Disable a NetworkServiceRecord."""
        pass

    def map_config_status(self):
        self._log.debug("Config status for ns {} is {}".
                        format(self.name, self._config_status))
        if self._config_status == NsrYang.ConfigStates.CONFIGURING:
            return 'configuring'
        if self._config_status == NsrYang.ConfigStates.FAILED:
            return 'failed'
        return 'configured'

    def vl_phase_completed(self):
        """ Are VLs created in this NS?"""
        return self._vl_phase_completed

    def vnf_phase_completed(self):
        """ Are VLs created in this NS?"""
        return self._vnf_phase_completed

    def create_msg(self):
        """ The network serice record as a message """
        nsr_dict = {"ns_instance_config_ref": self.id}
        nsr = RwNsrYang.YangData_Nsr_NsInstanceOpdata_Nsr.from_dict(nsr_dict)
        nsr.cloud_account = self.cloud_account_name
        nsr.sdn_account = self._sdn_account_name
        nsr.name_ref = self.name
        nsr.nsd_ref = self.nsd.id
        nsr.nsd_name_ref = self.nsd.name
        nsr.operational_events = self._op_status.msg
        nsr.operational_status = self._op_status.yang_str()
        nsr.config_status = self.map_config_status()
        nsr.create_time = self._create_time

        for cfg_prim in self.nsd_msg.config_primitive:
            cfg_prim = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_ConfigPrimitive.from_dict(
                    cfg_prim.as_dict())
            nsr.config_primitive.append(cfg_prim)

        for init_cfg in self.nsd_msg.initial_config_primitive:
            prim = NsrYang.NsrInitialConfigPrimitive.from_dict(
                init_cfg.as_dict())
            nsr.initial_config_primitive.append(prim)

        if self.vl_phase_completed():
            for vlr in self.vlrs:
                nsr.vlr.append(vlr.create_nsr_vlr_msg(self.vnfrs.values()))

        if self.vnf_phase_completed():
            for vnfr_id in self.vnfrs:
                nsr.constituent_vnfr_ref.append(self.vnfrs[vnfr_id].const_vnfr_msg)
            for vnffgr in self.vnffgrs.values():
                nsr.vnffgr.append(vnffgr.fetch_vnffgr())
            for scaling_group in self._scaling_groups.values():
                nsr.scaling_group_record.append(scaling_group.create_record_msg())

        return nsr

    def all_vnfs_active(self):
        """ Are all VNFS in this NS active? """
        for _, vnfr in self.vnfrs.items():
            if vnfr.active is not True:
                return False
        return True

    @asyncio.coroutine
    def update_state(self):
        """ Re-evaluate this  NS's state """
        curr_state = self._op_status.state

        if curr_state == NetworkServiceRecordState.TERMINATED:
            self._log.debug("NS (%s) in terminated state, not updating state", self.id)
            return

        new_state = NetworkServiceRecordState.RUNNING
        self._log.info("Received update_state for nsr: %s, curr-state: %s",
                       self.id, curr_state)

        # Check all the VNFRs are present
        for _, vnfr in self.vnfrs.items():
            if vnfr.state in [VnfRecordState.ACTIVE, VnfRecordState.TERMINATED]:
                pass
            elif vnfr.state == VnfRecordState.FAILED:
                if vnfr._prev_state != vnfr.state:
                    event_descr = "Instantiation of VNF %s failed" % vnfr.id
                    self.record_event("vnf-failed", event_descr)
                    vnfr.set_state(VnfRecordState.FAILED)
                else:
                    self._log.info("VNF state did not change, curr=%s, prev=%s",
                                   vnfr.state, vnfr._prev_state)
                new_state = NetworkServiceRecordState.FAILED
                break
            else:
                self._log.info("VNF %s in NSR %s is still not active; current state is: %s",
                               vnfr.id, self.id, vnfr.state)
                new_state = curr_state

        # If new state is RUNNIG; check VNFFGRs are also active
        if new_state == NetworkServiceRecordState.RUNNING:
            for _, vnffgr in self.vnffgrs.items():
                self._log.info("Checking vnffgr state for nsr %s is: %s",
                               self.id, vnffgr.state)
                if vnffgr.state == VnffgRecordState.ACTIVE:
                    pass
                elif vnffgr.state == VnffgRecordState.FAILED:
                    event_descr = "Instantiation of VNFFGR %s failed" % vnffgr.id
                    self.record_event("vnffg-failed", event_descr)
                    new_state = NetworkServiceRecordState.FAILED
                    break
                else:
                    self._log.info("VNFFGR %s in NSR %s is still not active; current state is: %s",
                                    vnffgr.id, self.id, vnffgr.state)
                    new_state = curr_state

        # Update all the scaling group instance operational status to
        # reflect the state of all VNFR within that instance
        yield from self._update_scale_group_instances_status()

        for _, group in self._scaling_groups.items():
            if group.state == scale_group.ScaleGroupState.SCALING_OUT:
                new_state = NetworkServiceRecordState.SCALING_OUT
                break
            elif group.state == scale_group.ScaleGroupState.SCALING_IN:
                new_state = NetworkServiceRecordState.SCALING_IN
                break

        if new_state != curr_state:
            self._log.debug("Changing state of Network service %s from %s to %s",
                            self.id, curr_state, new_state)
            if new_state == NetworkServiceRecordState.RUNNING:
                yield from self.is_active()
            elif new_state == NetworkServiceRecordState.FAILED:
                # If the NS is already active and we entered scaling_in, scaling_out,
                # do not mark the NS as failing if scaling operation failed.
                if curr_state in [NetworkServiceRecordState.SCALING_OUT,
                                  NetworkServiceRecordState.SCALING_IN] and self._is_active:
                    new_state = NetworkServiceRecordState.RUNNING
                    self.set_state(new_state)
                else:
                    yield from self.instantiation_failed()
            else:
                self.set_state(new_state)

        yield from self.publish()

    @asyncio.coroutine
    def get_monitoring_param(self):
        """ Get monitoring params for this network service """
        vnfrs = list(self.vnfrs.values())
        monp_list = []
        for vnfr in vnfrs:
            self._mon_params[vnfr.id].vnfr_id_ref = vnfr.id
            self._mon_params[vnfr.id].monitoring_param = yield from vnfr.get_monitoring_param()
            monp_list.append(self._mon_params[vnfr.id])

        return monp_list


class InputParameterSubstitution(object):
    """
    This class is responsible for substituting input parameters into an NSD.
    """

    def __init__(self, log):
        """Create an instance of InputParameterSubstitution

        Arguments:
            log - a logger for this object to use

        """
        self.log = log

    def __call__(self, nsd, nsr_config):
        """Substitutes input parameters from the NSR config into the NSD

        This call modifies the provided NSD with the input parameters that are
        contained in the NSR config.

        Arguments:
            nsd        - a GI NSD object
            nsr_config - a GI NSR config object

        """
        if nsd is None or nsr_config is None:
            return

        # Create a lookup of the xpath elements that this descriptor allows
        # to be modified
        optional_input_parameters = set()
        for input_parameter in nsd.input_parameter_xpath:
            optional_input_parameters.add(input_parameter.xpath)

        # Apply the input parameters to the descriptor
        if nsr_config.input_parameter:
            for param in nsr_config.input_parameter:
                if param.xpath not in optional_input_parameters:
                    msg = "tried to set an invalid input parameter ({})"
                    self.log.error(msg.format(param.xpath))

                    continue

                self.log.debug(
                        "input-parameter:{} = {}".format(
                            param.xpath,
                            param.value,
                            )
                        )

                try:
                    xpath.setxattr(nsd, param.xpath, param.value)

                except Exception as e:
                    self.log.exception(e)


class NetworkServiceDescriptor(object):
    """
    Network service descriptor class
    """

    def __init__(self, dts, log, loop, nsd, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop

        self._nsd = nsd
        self._ref_count = 0

        self._nsm = nsm

    @property
    def id(self):
        """ Returns nsd id """
        return self._nsd.id

    @property
    def name(self):
        """ Returns name of nsd """
        return self._nsd.name

    @property
    def ref_count(self):
        """ Returns reference count"""
        return self._ref_count

    def in_use(self):
        """ Returns whether nsd is in use or not """
        return True if self.ref_count > 0 else False

    def ref(self):
        """ Take a reference on this object """
        self._ref_count += 1

    def unref(self):
        """ Release reference on this object """
        if self.ref_count < 1:
            msg = ("Unref on a NSD object - nsd id %s, ref_count = %s" %
                   (self.id, self.ref_count))
            self._log.critical(msg)
            raise NetworkServiceDescriptorError(msg)
        self._ref_count -= 1

    @property
    def msg(self):
        """ Return the message associated with this NetworkServiceDescriptor"""
        return self._nsd

    @staticmethod
    def path_for_id(nsd_id):
        """ Return path for the passed nsd_id"""
        return "C,/nsd:nsd-catalog/nsd:nsd[nsd:id = '{}'".format(nsd_id)

    def path(self):
        """ Return the message associated with this NetworkServiceDescriptor"""
        return NetworkServiceDescriptor.path_for_id(self.id)

    def update(self, nsd):
        """ Update the NSD descriptor """
        if self.in_use():
            self._log.error("Cannot update descriptor %s in use", self.id)
            raise NetworkServiceDescriptorError("Cannot update descriptor in use %s" % self.id)
        self._nsd = nsd


class NsdDtsHandler(object):
    """ The network service descriptor DTS handler """
    XPATH = "C,/nsd:nsd-catalog/nsd:nsd"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm

        self._regh = None

    @property
    def regh(self):
        """ Return registration handle """
        return self._regh

    @asyncio.coroutine
    def register(self):
        """ Register for Nsd create/update/delete/read requests from dts """

        def on_apply(dts, acg, xact, action, scratch):
            """Apply the  configuration"""
            is_recovery = xact.xact is None and action == rwdts.AppconfAction.INSTALL
            self._log.debug("Got nsd apply cfg (xact:%s) (action:%s)",
                            xact, action)
            # Create/Update an NSD record
            for cfg in self._regh.get_xact_elements(xact):
                # Only interested in those NSD cfgs whose ID was received in prepare callback
                if cfg.id in scratch.get('nsds', []) or is_recovery:
                    self._nsm.update_nsd(cfg)

            scratch.pop('nsds', None)

            return RwTypes.RwStatus.SUCCESS

        @asyncio.coroutine
        def delete_nsd_libs(nsd_id):
            """ Remove any files uploaded with NSD and stored under $RIFT_ARTIFACTS/libs/<id> """
            try:
                rift_artifacts_dir = os.environ['RIFT_ARTIFACTS']
                nsd_dir = os.path.join(rift_artifacts_dir, 'launchpad/libs', nsd_id)

                if os.path.exists (nsd_dir):
                    shutil.rmtree(nsd_dir, ignore_errors=True)
            except Exception as e:
                self._log.error("Exception in cleaning up NSD libs {}: {}".
                                format(nsd_id, e))
                self._log.excpetion(e)

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ Prepare callback from DTS for NSD config """

            self._log.info("Got nsd prepare - config received nsd id %s, msg %s",
                           msg.id, msg)

            fref = ProtobufC.FieldReference.alloc()
            fref.goto_whole_message(msg.to_pbcm())

            if fref.is_field_deleted():
                # Delete an NSD record
                self._log.debug("Deleting NSD with id %s", msg.id)
                if self._nsm.nsd_in_use(msg.id):
                    self._log.debug("Cannot delete NSD in use - %s", msg.id)
                    err = "Cannot delete an NSD in use - %s" % msg.id
                    raise NetworkServiceDescriptorRefCountExists(err)

                yield from delete_nsd_libs(msg.id)
                self._nsm.delete_nsd(msg.id)
            else:
                # Handle actual adds/updates in apply_callback,
                # just check if NSD in use in prepare_callback
                if self._nsm.nsd_in_use(msg.id):
                    self._log.debug("Cannot modify an NSD in use - %s", msg.id)
                    err = "Cannot modify an NSD in use - %s" % msg.id
                    raise NetworkServiceDescriptorRefCountExists(err)

                # Add this NSD to scratch to create/update in apply callback
                nsds = scratch.setdefault('nsds', [])
                nsds.append(msg.id)
                # acg._scratch['nsds'].append(msg.id)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        self._log.debug(
            "Registering for NSD config using xpath: %s",
            NsdDtsHandler.XPATH,
            )

        acg_hdl = rift.tasklets.AppConfGroup.Handler(on_apply=on_apply)
        with self._dts.appconf_group_create(handler=acg_hdl) as acg:
            # Need a list in scratch to store NSDs to create/update later
            # acg._scratch['nsds'] = list()
            self._regh = acg.register(
                xpath=NsdDtsHandler.XPATH,
                flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY | rwdts.Flag.CACHE,
                on_prepare=on_prepare)


class VnfdDtsHandler(object):
    """ DTS handler for VNFD config changes """
    XPATH = "C,/vnfd:vnfd-catalog/vnfd:vnfd"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm
        self._regh = None

    @property
    def regh(self):
        """ DTS registration handle """
        return self._regh

    @asyncio.coroutine
    def register(self):
        """ Register for VNFD configuration"""

        @asyncio.coroutine
        def on_apply(dts, acg, xact, action, scratch):
            """Apply the  configuration"""
            self._log.debug("Got NSM VNFD apply (xact: %s) (action: %s)(scr: %s)",
                            xact, action, scratch)

            # Create/Update a VNFD record
            for cfg in self._regh.get_xact_elements(xact):
                # Only interested in those VNFD cfgs whose ID was received in prepare callback
                if cfg.id in scratch.get('vnfds', []):
                    self._nsm.update_vnfd(cfg)

            for cfg in self._regh.elements:
                if cfg.id in scratch.get('deleted_vnfds', []):
                    yield from self._nsm.delete_vnfd(cfg.id)

            scratch.pop('vnfds', None)
            scratch.pop('deleted_vnfds', None)

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ on prepare callback """
            self._log.debug("Got on prepare for VNFD (path: %s) (action: %s) (msg: %s)",
                            ks_path.to_xpath(RwNsmYang.get_schema()), xact_info.query_action, msg)

            fref = ProtobufC.FieldReference.alloc()
            fref.goto_whole_message(msg.to_pbcm())

            # Handle deletes in prepare_callback, but adds/updates in apply_callback
            if fref.is_field_deleted():
                self._log.debug("Adding msg to deleted field")
                deleted_vnfds = scratch.setdefault('deleted_vnfds', [])
                deleted_vnfds.append(msg.id)
            else:
                # Add this VNFD to scratch to create/update in apply callback
                vnfds = scratch.setdefault('vnfds', [])
                vnfds.append(msg.id)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        self._log.debug(
            "Registering for VNFD config using xpath: %s",
            VnfdDtsHandler.XPATH,
            )
        acg_hdl = rift.tasklets.AppConfGroup.Handler(on_apply=on_apply)
        with self._dts.appconf_group_create(handler=acg_hdl) as acg:
            # Need a list in scratch to store VNFDs to create/update later
            # acg._scratch['vnfds'] = list()
            # acg._scratch['deleted_vnfds'] = list()
            self._regh = acg.register(
                xpath=VnfdDtsHandler.XPATH,
                flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY,
                on_prepare=on_prepare)


class NsrDtsHandler(object):
    """ The network service DTS handler """
    NSR_XPATH = "C,/nsr:ns-instance-config/nsr:nsr"
    SCALE_INSTANCE_XPATH = "C,/nsr:ns-instance-config/nsr:nsr/nsr:scaling-group/nsr:instance"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm

        self._nsr_regh = None
        self._scale_regh = None

    @property
    def nsm(self):
        """ Return the NS manager instance """
        return self._nsm

    @asyncio.coroutine
    def register(self):
        """ Register for Nsr create/update/delete/read requests from dts """

        def nsr_id_from_keyspec(ks):
            nsr_path_entry = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr.schema().keyspec_to_entry(ks)
            nsr_id = nsr_path_entry.key00.id
            return nsr_id

        def group_name_from_keyspec(ks):
            group_path_entry = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_ScalingGroup.schema().keyspec_to_entry(ks)
            group_name = group_path_entry.key00.scaling_group_name_ref
            return group_name

        def is_instance_in_reg_elements(nsr_id, group_name, instance_id):
            """ Return boolean indicating if scaling group instance was already commited previously.

            By looking at the existing elements in this registration handle (elements not part
            of this current xact), we can tell if the instance was configured previously without
            keeping any application state.
            """
            for instance_cfg, keyspec in self._nsr_regh.get_xact_elements(include_keyspec=True):
                elem_nsr_id = nsr_id_from_keyspec(keyspec)
                elem_group_name = group_name_from_keyspec(keyspec)

                if elem_nsr_id != nsr_id or group_name != elem_group_name:
                    continue

                if instance_cfg.id == instance_id:
                    return True

            return False

        def get_scale_group_instance_delta(nsr_id, group_name, xact):
            delta = {"added": [], "deleted": []}
            for instance_cfg, keyspec in self._scale_regh.get_xact_elements(xact, include_keyspec=True):
                elem_nsr_id = nsr_id_from_keyspec(keyspec)
                if elem_nsr_id != nsr_id:
                    continue

                elem_group_name = group_name_from_keyspec(keyspec)
                if elem_group_name != group_name:
                    continue

                delta["added"].append(instance_cfg.id)

            for instance_cfg, keyspec in self._scale_regh.get_xact_elements(include_keyspec=True):
                elem_nsr_id = nsr_id_from_keyspec(keyspec)
                if elem_nsr_id != nsr_id:
                    continue

                elem_group_name = group_name_from_keyspec(keyspec)
                if elem_group_name != group_name:
                    continue

                if instance_cfg.id in delta["added"]:
                    delta["added"].remove(instance_cfg.id)
                else:
                    delta["deleted"].append(instance_cfg.id)

            return delta

        def get_add_delete_update_cfgs(dts_member_reg, xact, key_name):
            # Unforunately, it is currently difficult to figure out what has exactly
            # changed in this xact without Pbdelta support (RIFT-4916)
            # As a workaround, we can fetch the pre and post xact elements and
            # perform a comparison to figure out adds/deletes/updates
            xact_cfgs = list(dts_member_reg.get_xact_elements(xact))
            curr_cfgs = list(dts_member_reg.elements)

            xact_key_map = {getattr(cfg, key_name): cfg for cfg in xact_cfgs}
            curr_key_map = {getattr(cfg, key_name): cfg for cfg in curr_cfgs}

            # Find Adds
            added_keys = set(xact_key_map) - set(curr_key_map)
            added_cfgs = [xact_key_map[key] for key in added_keys]

            # Find Deletes
            deleted_keys = set(curr_key_map) - set(xact_key_map)
            deleted_cfgs = [curr_key_map[key] for key in deleted_keys]

            # Find Updates
            updated_keys = set(curr_key_map) & set(xact_key_map)
            updated_cfgs = [xact_key_map[key] for key in updated_keys if xact_key_map[key] != curr_key_map[key]]

            return added_cfgs, deleted_cfgs, updated_cfgs

        def on_apply(dts, acg, xact, action, scratch):
            """Apply the  configuration"""
            self._log.debug("Got nsr apply (xact: %s) (action: %s)(scr: %s)",
                            xact, action, scratch)

            def handle_create_nsr(msg, restart_mode=False):
                # Handle create nsr requests """
                # Do some validations
                if not msg.has_field("nsd_ref"):
                    err = "NSD reference not provided"
                    self._log.error(err)
                    raise NetworkServiceRecordError(err)

                self._log.info("Creating NetworkServiceRecord %s  from nsd_id  %s",
                               msg.id, msg.nsd_ref)

                nsr = self.nsm.create_nsr(msg, restart_mode=restart_mode)
                return nsr

            def handle_delete_nsr(msg):
                @asyncio.coroutine
                def delete_instantiation(ns_id):
                    """ Delete instantiation """
                    with self._dts.transaction() as xact:
                        yield from self._nsm.terminate_ns(ns_id, xact)

                # Handle delete NSR requests
                self._log.info("Delete req for  NSR Id: %s received", msg.id)
                # Terminate the NSR instance
                nsr = self._nsm.get_ns_by_nsr_id(msg.id)

                nsr.set_state(NetworkServiceRecordState.TERMINATE_RCVD)
                event_descr = "Terminate rcvd for NS Id:%s" % msg.id
                nsr.record_event("terminate-rcvd", event_descr)

                self._loop.create_task(delete_instantiation(msg.id))

            @asyncio.coroutine
            def begin_instantiation(nsr):
                # Begin instantiation
                self._log.info("Beginning NS instantiation: %s", nsr.id)
                yield from self._nsm.instantiate_ns(nsr.id, xact)

            self._log.debug("Got nsr apply (xact: %s) (action: %s)(scr: %s)",
                            xact, action, scratch)

            if action == rwdts.AppconfAction.INSTALL and xact.id is None:
                for element in self._nsr_regh.elements:
                    nsr = handle_create_nsr(element, restart_mode=True)
                    self._loop.create_task(begin_instantiation(nsr))


            (added_msgs, deleted_msgs, updated_msgs) = get_add_delete_update_cfgs(self._nsr_regh, xact, "id")

            for msg in added_msgs:
                if msg.id not in self._nsm.nsrs:
                    self._log.info("Create NSR received in on_apply to instantiate NS:%s", msg.id)
                    nsr = handle_create_nsr(msg)
                    self._loop.create_task(begin_instantiation(nsr))

            for msg in deleted_msgs:
                self._log.info("Delete NSR received in on_apply to terminate NS:%s", msg.id)
                try:
                    handle_delete_nsr(msg)
                except Exception:
                    self._log.exception("Failed to terminate NS:%s", msg.id)

            for msg in updated_msgs:
                self._log.info("Update NSR received in on_apply to change scaling groups in NS:%s", msg.id)

                for group in msg.scaling_group:
                    instance_delta = get_scale_group_instance_delta(msg.id, group.scaling_group_name_ref, xact)
                    self._log.debug("Got NSR:%s scale group instance delta: %s", msg.id, instance_delta)

                    for instance_id in instance_delta["added"]:
                        self._nsm.scale_nsr_out(msg.id, group.scaling_group_name_ref, instance_id, xact)

                    for instance_id in instance_delta["deleted"]:
                        self._nsm.scale_nsr_in(msg.id, group.scaling_group_name_ref, instance_id)


            return RwTypes.RwStatus.SUCCESS

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ Prepare calllback from DTS for NSR """

            xpath = ks_path.to_xpath(RwNsrYang.get_schema())
            action = xact_info.query_action
            self._log.debug(
                    "Got Nsr prepare callback (xact: %s) (action: %s) (info: %s), %s:%s)",
                    xact, action, xact_info, xpath, msg
                    )

            @asyncio.coroutine
            def delete_instantiation(ns_id):
                """ Delete instantiation """
                yield from self._nsm.terminate_ns(ns_id, None)

            def handle_delete_nsr():
                """ Handle delete NSR requests """
                self._log.info("Delete req for  NSR Id: %s received", msg.id)
                # Terminate the NSR instance
                nsr = self._nsm.get_ns_by_nsr_id(msg.id)

                nsr.set_state(NetworkServiceRecordState.TERMINATE_RCVD)
                event_descr = "Terminate rcvd for NS Id:%s" % msg.id
                nsr.record_event("terminate-rcvd", event_descr)

                self._loop.create_task(delete_instantiation(msg.id))

            fref = ProtobufC.FieldReference.alloc()
            fref.goto_whole_message(msg.to_pbcm())

            if action in [rwdts.QueryAction.CREATE, rwdts.QueryAction.UPDATE, rwdts.QueryAction.DELETE]:
                # Ensure the Cloud account has been specified if this is an NSR create
                if action != rwdts.QueryAction.DELETE and msg.id not in self._nsm.nsrs:
                    if not msg.has_field("cloud_account"):
                        raise NsrInstantiationFailed("Cloud account not specified in NSR")

                elif msg.has_field("scaling_group"):
                    nsr = self._nsm.nsrs[msg.id]
                    if nsr.state != NetworkServiceRecordState.RUNNING:
                        raise ScalingOperationError("Unable to perform scaling action when NS is not in running state")

                    if len(msg.scaling_group) > 1:
                        raise ScalingOperationError("Only a single scaling group can be configured at a time")

                    for group_msg in msg.scaling_group:
                        num_new_group_instances = len(group_msg.instance)
                        if num_new_group_instances > 1:
                            raise ScalingOperationError("Only a single scaling instance can be modified at a time")

                        elif num_new_group_instances == 1:
                            scale_group = nsr.scaling_groups[group_msg.scaling_group_name_ref]
                            if action in [rwdts.QueryAction.CREATE, rwdts.QueryAction.UPDATE]:
                                if len(scale_group.instances) == scale_group.max_instance_count:
                                    raise ScalingOperationError("Max instances for %s reached" % scale_group)

            acg.handle.prepare_complete_ok(xact_info.handle)


        self._log.debug("Registering for NSR config using xpath: %s",
                        NsrDtsHandler.NSR_XPATH)

        acg_hdl = rift.tasklets.AppConfGroup.Handler(on_apply=on_apply)
        with self._dts.appconf_group_create(handler=acg_hdl) as acg:
            self._nsr_regh = acg.register(xpath=NsrDtsHandler.NSR_XPATH,
                                      flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY | rwdts.Flag.CACHE,
                                      on_prepare=on_prepare)

            self._scale_regh = acg.register(
                                      xpath=NsrDtsHandler.SCALE_INSTANCE_XPATH,
                                      flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY| rwdts.Flag.CACHE,
                                      )


class NsrOpDataDtsHandler(object):
    """ The network service op data DTS handler """
    XPATH = "D,/nsr:ns-instance-opdata/nsr:nsr"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm
        self._regh = None

    @property
    def regh(self):
        """ Return the registration handle"""
        return self._regh

    @property
    def nsm(self):
        """ Return the NS manager instance """
        return self._nsm

    @asyncio.coroutine
    def register(self):
        """ Register for Nsr op data publisher registration"""
        self._log.debug("Registering Nsr op data path %s as publisher",
                        NsrOpDataDtsHandler.XPATH)

        hdl = rift.tasklets.DTS.RegistrationHandler()
        handlers = rift.tasklets.Group.Handler()
        with self._dts.group_create(handler=handlers) as group:
            self._regh = group.register(xpath=NsrOpDataDtsHandler.XPATH,
                                        handler=hdl,
                                        flags=rwdts.Flag.PUBLISHER | rwdts.Flag.NO_PREP_READ | rwdts.Flag.DATASTORE)

    @asyncio.coroutine
    def create(self, path, msg):
        """
        Create an NS record in DTS with the path and message
        """
        self._log.debug("Creating NSR %s:%s", path, msg)
        self.regh.create_element(path, msg)
        self._log.debug("Created NSR, %s:%s", path, msg)

    @asyncio.coroutine
    def update(self, path, msg, flags=rwdts.XactFlag.REPLACE):
        """
        Update an NS record in DTS with the path and message
        """
        self._log.debug("Updating NSR, %s:%s regh = %s", path, msg, self.regh)
        self.regh.update_element(path, msg, flags)
        self._log.debug("Updated NSR, %s:%s", path, msg)

    @asyncio.coroutine
    def delete(self, path):
        """
        Update an NS record in DTS with the path and message
        """
        self._log.debug("Deleting NSR path:%s", path)
        self.regh.delete_element(path)
        self._log.debug("Deleted NSR path:%s", path)


class VnfrDtsHandler(object):
    """ The virtual network service DTS handler """
    XPATH = "D,/vnfr:vnfr-catalog/vnfr:vnfr"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm

        self._regh = None

    @property
    def regh(self):
        """ Return registration handle """
        return self._regh

    @property
    def nsm(self):
        """ Return the NS manager instance """
        return self._nsm

    @asyncio.coroutine
    def register(self):
        """ Register for vnfr create/update/delete/ advises from dts """

        def on_commit(xact_info):
            """ The transaction has been committed """
            self._log.debug("Got vnfr commit (xact_info: %s)", xact_info)
            return rwdts.MemberRspCode.ACTION_OK

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            """ prepare callback from dts """
            xpath = ks_path.to_xpath(RwNsrYang.get_schema())
            self._log.debug(
                "Got vnfr on_prepare cb (xact_info: %s, action: %s): %s:%s",
                xact_info, action, ks_path, msg
                )

            schema = VnfrYang.YangData_Vnfr_VnfrCatalog_Vnfr.schema()
            path_entry = schema.keyspec_to_entry(ks_path)
            if path_entry.key00.id not in self._nsm._vnfrs:
                self._log.error("%s request for non existent record path %s",
                                action, xpath)
                xact_info.respond_xpath(rwdts.XactRspCode.NA, xpath)

                self._log.debug("Deleting VNFR with id %s", path_entry.key00.id)
            if action == rwdts.QueryAction.CREATE or action == rwdts.QueryAction.UPDATE:
                yield from self._nsm.update_vnfr(msg)
            elif action == rwdts.QueryAction.DELETE:
                self._log.debug("Deleting VNFR with id %s", path_entry.key00.id)
                self._nsm.delete_vnfr(path_entry.key00.id)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

        self._log.debug("Registering for VNFR using xpath: %s",
                        VnfrDtsHandler.XPATH,)

        hdl = rift.tasklets.DTS.RegistrationHandler(on_commit=on_commit,
                                                    on_prepare=on_prepare,)
        with self._dts.group_create() as group:
            self._regh = group.register(xpath=VnfrDtsHandler.XPATH,
                                        handler=hdl,
                                        flags=(rwdts.Flag.SUBSCRIBER),)


class NsMonitorDtsHandler(object):
    """ The Network service Monitor DTS handler """
    XPATH = "D,/nsr:ns-instance-opdata/nsr:nsr/nsr:vnf-monitoring-param"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm

        self._regh = None

    @property
    def regh(self):
        """ Return registration handle """
        return self._regh

    @property
    def nsm(self):
        """ Return the NS manager instance """
        return self._nsm

    @staticmethod
    def vnf_mon_param_xpath(nsr_id, vnfr_id):
        """ VNF monitoring xpath """
        return ("D,/nsr:ns-instance-opdata" +
                "/nsr:nsr[nsr:ns-instance-config-ref = '{}']" +
                "/nsr:vnf-monitoring-param" +
                "[nsr:vnfr-id-ref = '{}']").format(nsr_id, vnfr_id)

    @asyncio.coroutine
    def register(self):
        """ Register for NS monitoring read from dts """

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            """ prepare callback from dts """
            if action == rwdts.QueryAction.READ:
                schema = RwNsrYang.YangData_Nsr_NsInstanceOpdata_Nsr.schema()
                path_entry = schema.keyspec_to_entry(ks_path)
                try:
                    monp_list = yield from self._nsm.get_monitoring_param(
                        path_entry.key00.ns_instance_config_ref)
                    for nsr_id, vnf_monp_list in monp_list:
                        for monp in vnf_monp_list:
                            vnf_xpath = NsMonitorDtsHandler.vnf_mon_param_xpath(
                                            nsr_id,
                                            monp.vnfr_id_ref
                                            )
                            xact_info.respond_xpath(rwdts.XactRspCode.MORE,
                                                    vnf_xpath,
                                                    monp)
                except Exception:
                    self._log.exception("##### Caught exception while collection mon params #####")

                xact_info.respond_xpath(rwdts.XactRspCode.ACK)
            else:
                xact_info.respond_xpath(rwdts.XactRspCode.NA)

        hdl = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare,)
        with self._dts.group_create() as group:
            self._regh = group.register(xpath=NsMonitorDtsHandler.XPATH,
                                        handler=hdl,
                                        flags=rwdts.Flag.PUBLISHER,
                                        )


class NsdRefCountDtsHandler(object):
    """ The NSD Ref Count DTS handler """
    XPATH = "D,/nsr:ns-instance-opdata/rw-nsr:nsd-ref-count"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm

        self._regh = None

    @property
    def regh(self):
        """ Return registration handle """
        return self._regh

    @property
    def nsm(self):
        """ Return the NS manager instance """
        return self._nsm

    @asyncio.coroutine
    def register(self):
        """ Register for NSD ref count read from dts """

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            """ prepare callback from dts """
            xpath = ks_path.to_xpath(RwNsrYang.get_schema())

            if action == rwdts.QueryAction.READ:
                schema = RwNsrYang.YangData_Nsr_NsInstanceOpdata_NsdRefCount.schema()
                path_entry = schema.keyspec_to_entry(ks_path)
                nsd_list = yield from self._nsm.get_nsd_refcount(path_entry.key00.nsd_id_ref)
                for xpath, msg in nsd_list:
                    xact_info.respond_xpath(rsp_code=rwdts.XactRspCode.MORE,
                                            xpath=xpath,
                                            msg=msg)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK)
            else:
                raise NetworkServiceRecordError("Not supported operation %s" % action)

        hdl = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare,)
        with self._dts.group_create() as group:
            self._regh = group.register(xpath=NsdRefCountDtsHandler.XPATH,
                                        handler=hdl,
                                        flags=rwdts.Flag.PUBLISHER,)


class NsManager(object):
    """ The Network Service Manager class"""
    def __init__(self, dts, log, loop,
                 nsr_handler, vnfr_handler, vlr_handler, cloud_plugin_selector, vnffgmgr, vnfd_pub_handler):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsr_handler = nsr_handler
        self._vnfr_pub_handler = vnfr_handler
        self._vlr_pub_handler = vlr_handler
        self._vnffgmgr = vnffgmgr
        self._vnfd_pub_handler = vnfd_pub_handler

        self._cloud_plugin_selector = cloud_plugin_selector

        self._nsrs = {}
        self._nsds = {}
        self._vnfds = {}
        self._vnfrs = {}

        self.cfgmgr_obj = conman.ROConfigManager(log, loop, dts, self)

        # TODO: All these handlers should move to tasklet level.
        # Passing self is often an indication of bad design
        self._nsd_dts_handler = NsdDtsHandler(dts, log, loop, self)
        self._vnfd_dts_handler = VnfdDtsHandler(dts, log, loop, self)
        self._dts_handlers = [self._nsd_dts_handler,
                              VnfrDtsHandler(dts, log, loop, self),
                              NsMonitorDtsHandler(dts, log, loop, self),
                              NsdRefCountDtsHandler(dts, log, loop, self),
                              NsrDtsHandler(dts, log, loop, self),
                              self._vnfd_dts_handler,
                              self.cfgmgr_obj,
                              ]


    @property
    def log(self):
        """ Log handle """
        return self._log

    @property
    def loop(self):
        """ Loop """
        return self._loop

    @property
    def dts(self):
        """ DTS handle """
        return self._dts

    @property
    def nsr_handler(self):
        """" NSR handler """
        return self._nsr_handler

    @property
    def so_obj(self):
        """" So Obj handler """
        return self._so_obj

    @property
    def nsrs(self):
        """ NSRs in this NSM"""
        return self._nsrs

    @property
    def nsds(self):
        """ NSDs in this NSM"""
        return self._nsds

    @property
    def vnfds(self):
        """ VNFDs in this NSM"""
        return self._vnfds

    @property
    def vnfrs(self):
        """ VNFRs in this NSM"""
        return self._vnfrs

    @property
    def nsr_pub_handler(self):
        """ NSR publication handler """
        return self._nsr_handler

    @property
    def vnfr_pub_handler(self):
        """ VNFR publication handler """
        return self._vnfr_pub_handler

    @property
    def vlr_pub_handler(self):
        """ VLR publication handler """
        return self._vlr_pub_handler

    @property
    def vnfd_pub_handler(self):
        return self._vnfd_pub_handler

    @asyncio.coroutine
    def register(self):
        """ Register all static DTS handlers """
        for dts_handle in self._dts_handlers:
            yield from dts_handle.register()

    def get_ns_by_nsr_id(self, nsr_id):
        """ get NSR by nsr id """
        if nsr_id not in self._nsrs:
            raise NetworkServiceRecordError("NSR id %s not found" % nsr_id)

        return self._nsrs[nsr_id]

    def scale_nsr_out(self, nsr_id, scale_group_name, instance_id, config_xact):
        self.log.debug("Scale out NetworkServiceRecord (nsr_id: %s) (scaling group: %s) (instance_id: %s)",
                       nsr_id,
                       scale_group_name,
                       instance_id
                       )
        nsr = self._nsrs[nsr_id]
        if nsr.state != NetworkServiceRecordState.RUNNING:
            raise ScalingOperationError("Cannot perform scaling operation if NSR is not is not in running state")

        self._loop.create_task(nsr.create_scale_group_instance(scale_group_name, instance_id, config_xact))

    def scale_nsr_in(self, nsr_id, scale_group_name, instance_id):
        self.log.debug("Scale in NetworkServiceRecord (nsr_id: %s) (scaling group: %s) (instance_id: %s)",
                       nsr_id,
                       scale_group_name,
                       instance_id,
                       )
        nsr = self._nsrs[nsr_id]
        if nsr.state != NetworkServiceRecordState.RUNNING:
            raise ScalingOperationError("Cannot perform scaling operation if NSR is not is not in running state")

        self._loop.create_task(nsr.delete_scale_group_instance(scale_group_name, instance_id))

    def create_nsr(self, nsr_msg, restart_mode=False):
        """ Create an NSR instance """
        if nsr_msg.id in self._nsrs:
            msg = "NSR id %s already exists" % nsr_msg.id
            self._log.error(msg)
            raise NetworkServiceRecordError(msg)

        self._log.info("Create NetworkServiceRecord nsr id %s from nsd_id %s",
                       nsr_msg.id,
                       nsr_msg.nsd_ref)

        nsm_plugin = self._cloud_plugin_selector.get_cloud_account_plugin_instance(
            nsr_msg.cloud_account
            )
        sdn_account_name = self._cloud_plugin_selector.get_cloud_account_sdn_name(nsr_msg.cloud_account)

        nsr = NetworkServiceRecord(self._dts,
                                   self._log,
                                   self._loop,
                                   self,
                                   nsm_plugin,
                                   nsr_msg,
                                   sdn_account_name,
                                   restart_mode=restart_mode
                                   )
        self._nsrs[nsr_msg.id] = nsr
        nsm_plugin.create_nsr(nsr_msg, self.get_nsd(nsr_msg.nsd_ref).msg)

        return nsr

    def delete_nsr(self, nsr_id):
        """
        Delete NSR with the passed nsr id
        """
        del self._nsrs[nsr_id]

    @asyncio.coroutine
    def instantiate_ns(self, nsr_id, config_xact):
        """ Instantiate an NS instance """
        self._log.debug("Instantiating Network service id %s", nsr_id)
        if nsr_id not in self._nsrs:
            err = "NSR id %s not found " % nsr_id
            self._log.error(err)
            raise NetworkServiceRecordError(err)

        nsr = self._nsrs[nsr_id]
        yield from nsr.nsm_plugin.instantiate_ns(nsr, config_xact)

    @asyncio.coroutine
    def update_vnfr(self, vnfr):
        """Create/Update an VNFR """

        vnfr_state = self._vnfrs[vnfr.id].state
        self._log.debug("Updating VNFR with state %s: vnfr %s", vnfr_state, vnfr)

        yield from self._vnfrs[vnfr.id].update_state(vnfr)
        nsr = self.find_nsr_for_vnfr(vnfr.id)
        yield from nsr.update_state()

    def find_nsr_for_vnfr(self, vnfr_id):
        """ Find the NSR which )has the passed vnfr id"""
        for nsr in list(self.nsrs.values()):
            for vnfr in list(nsr.vnfrs.values()):
                if vnfr.id == vnfr_id:
                    return nsr
        return None

    def delete_vnfr(self, vnfr_id):
        """ Delete VNFR  with the passed id"""
        del self._vnfrs[vnfr_id]

    def get_nsd_ref(self, nsd_id):
        """ Get network service descriptor for the passed nsd_id
            with a reference"""
        nsd = self.get_nsd(nsd_id)
        nsd.ref()
        return nsd

    @asyncio.coroutine
    def get_nsr_config(self, nsd_id):
        xpath = "C,/nsr:ns-instance-config"
        results = yield from self._dts.query_read(xpath, rwdts.XactFlag.MERGE)

        for result in results:
            entry = yield from result
            ns_instance_config = entry.result

            for nsr in ns_instance_config.nsr:
                if nsr.nsd_ref == nsd_id:
                    return nsr

        return None

    @asyncio.coroutine
    def nsd_unref_by_nsr_id(self, nsr_id):
        """ Unref the network service descriptor based on NSR id """
        self._log.debug("NSR Unref called for Nsr Id:%s", nsr_id)
        if nsr_id in self._nsrs:
            nsr = self._nsrs[nsr_id]
            nsd = self.get_nsd(nsr.nsd_id)
            self._log.debug("Releasing ref on NSD %s held by NSR %s - Curr %d",
                            nsd.id, nsr.id, nsd.ref_count)
            nsd.unref()
        else:
            self._log.error("Cannot find NSD for NSR id %s", nsr_id)
            raise NetworkServiceDescriptorUnrefError("No Nsd for nsr id" % nsr_id)

    @asyncio.coroutine
    def nsd_unref(self, nsd_id):
        """ Unref the network service descriptor associated with the id """
        nsd = self.get_nsd(nsd_id)
        nsd.unref()

    def get_nsd(self, nsd_id):
        """ Get network service descriptor for the passed nsd_id"""
        if nsd_id not in self._nsds:
            self._log.error("Cannot find NSD id:%s", nsd_id)
            raise NetworkServiceDescriptorError("Cannot find NSD id:%s", nsd_id)

        return self._nsds[nsd_id]

    def create_nsd(self, nsd_msg):
        """ Create a network service descriptor """
        self._log.debug("Create network service descriptor - %s", nsd_msg)
        if nsd_msg.id in self._nsds:
            self._log.error("Cannot create NSD %s -NSD ID already exists", nsd_msg)
            raise NetworkServiceDescriptorError("NSD already exists-%s", nsd_msg.id)

        nsd = NetworkServiceDescriptor(
                self._dts,
                self._log,
                self._loop,
                nsd_msg,
                self
                )
        self._nsds[nsd_msg.id] = nsd

        return nsd

    def update_nsd(self, nsd):
        """ update the Network service descriptor """
        self._log.debug("Update network service descriptor - %s", nsd)
        if nsd.id not in self._nsds:
            self._log.debug("No NSD found - creating NSD id = %s", nsd.id)
            self.create_nsd(nsd)
        else:
            self._log.debug("Updating NSD id = %s, nsd = %s", nsd.id, nsd)
            self._nsds[nsd.id].update(nsd)

    def delete_nsd(self, nsd_id):
        """ Delete the Network service descriptor with the passed id """
        self._log.debug("Deleting the network service descriptor - %s", nsd_id)
        if nsd_id not in self._nsds:
            self._log.debug("Delete NSD failed - cannot find nsd-id %s", nsd_id)
            raise NetworkServiceDescriptorNotFound("Cannot find %s", nsd_id)

        if nsd_id not in self._nsds:
            self._log.debug("Cannot delete NSD id %s reference exists %s",
                            nsd_id,
                            self._nsds[nsd_id].ref_count)
            raise NetworkServiceDescriptorRefCountExists(
                "Cannot delete :%s, ref_count:%s",
                nsd_id,
                self._nsds[nsd_id].ref_count)

        del self._nsds[nsd_id]

    def get_vnfd_config(self, xact):
        vnfd_dts_reg = self._vnfd_dts_handler.regh
        for cfg in vnfd_dts_reg.get_xact_elements(xact):
            if cfg.id not in self._vnfds:
                self.create_vnfd(cfg)

    def get_vnfd(self, vnfd_id, xact):
        """ Get virtual network function descriptor for the passed vnfd_id"""
        if vnfd_id not in self._vnfds:
            self._log.error("Cannot find VNFD id:%s", vnfd_id)
            self.get_vnfd_config(xact)

            if vnfd_id not in self._vnfds:
                self._log.error("Cannot find VNFD id:%s", vnfd_id)
                raise VnfDescriptorError("Cannot find VNFD id:%s", vnfd_id)

        return self._vnfds[vnfd_id]

    def create_vnfd(self, vnfd):
        """ Create a virtual network function descriptor """
        self._log.debug("Create virtual network function descriptor - %s", vnfd)
        if vnfd.id in self._vnfds:
            self._log.error("Cannot create VNFD %s -VNFD ID already exists", vnfd)
            raise VnfDescriptorError("VNFD already exists-%s", vnfd.id)

        self._vnfds[vnfd.id] = vnfd
        return self._vnfds[vnfd.id]

    def update_vnfd(self, vnfd):
        """ Update the virtual network function descriptor """
        self._log.debug("Update virtual network function descriptor- %s", vnfd)

        # Hack to remove duplicates from leaf-lists - to be fixed by RIFT-6511
        for ivld in vnfd.internal_vld:
            ivld.internal_connection_point_ref = list(set(ivld.internal_connection_point_ref))

        if vnfd.id not in self._vnfds:
            self._log.debug("No VNFD found - creating VNFD id = %s", vnfd.id)
            self.create_vnfd(vnfd)
        else:
            self._log.debug("Updating VNFD id = %s, vnfd = %s", vnfd.id, vnfd)
            self._vnfds[vnfd.id] = vnfd

    @asyncio.coroutine
    def delete_vnfd(self, vnfd_id):
        """ Delete the virtual network function descriptor with the passed id """
        self._log.debug("Deleting the virtual network function descriptor - %s", vnfd_id)
        if vnfd_id not in self._vnfds:
            self._log.debug("Delete VNFD failed - cannot find vnfd-id %s", vnfd_id)
            raise VnfDescriptorError("Cannot find %s", vnfd_id)

        del self._vnfds[vnfd_id]

    def nsd_in_use(self, nsd_id):
        """ Is the NSD with the passed id in use """
        self._log.debug("Is this NSD in use - msg:%s", nsd_id)
        if nsd_id in self._nsds:
            return self._nsds[nsd_id].in_use()
        return False

    @asyncio.coroutine
    def publish_nsr(self, xact, path, msg):
        """ Publish a NSR """
        self._log.debug("Publish NSR with path %s, msg %s",
                        path, msg)
        yield from self.nsr_handler.update(xact, path, msg)

    @asyncio.coroutine
    def unpublish_nsr(self, xact, path):
        """ Un Publish an NSR """
        self._log.debug("Publishing delete NSR with path %s", path)
        yield from self.nsr_handler.delete(path, xact)

    def vnfr_is_ready(self, vnfr_id):
        """ VNFR with the id is ready """
        self._log.debug("VNFR id %s ready", vnfr_id)
        if vnfr_id not in self._vnfds:
            err = "Did not find VNFR ID with id %s" % vnfr_id
            self._log.critical("err")
            raise VirtualNetworkFunctionRecordError(err)
        self._vnfrs[vnfr_id].is_ready()

    @asyncio.coroutine
    def get_monitoring_param(self, nsr_id):
        """ Get the monitoring params based on the passed ks_path """
        monp_list = []
        if nsr_id is None or nsr_id == "":
            nsrs = list(self._nsrs.values())
            for nsr in nsrs:
                if nsr.active:
                    monp = yield from nsr.get_monitoring_param()
                    monp_list.append((nsr.id, monp))
        elif nsr_id in self._nsrs:
            if self._nsrs[nsr_id].active:
                monp = yield from self._nsrs[nsr_id].get_monitoring_param()
                monp_list.append((nsr_id, monp))

        return monp_list

    @asyncio.coroutine
    def get_nsd_refcount(self, nsd_id):
        """ Get the nsd_list from this NSM"""

        def nsd_refcount_xpath(nsd_id):
            """ xpath for ref count entry """
            return (NsdRefCountDtsHandler.XPATH +
                    "[rw-nsr:nsd-id-ref = '{}']").format(nsd_id)

        nsd_list = []
        if nsd_id is None or nsd_id == "":
            for nsd in self._nsds.values():
                nsd_msg = RwNsrYang.YangData_Nsr_NsInstanceOpdata_NsdRefCount()
                nsd_msg.nsd_id_ref = nsd.id
                nsd_msg.instance_ref_count = nsd.ref_count
                nsd_list.append((nsd_refcount_xpath(nsd.id), nsd_msg))
        elif nsd_id in self._nsds:
            nsd_msg = RwNsrYang.YangData_Nsr_NsInstanceOpdata_NsdRefCount()
            nsd_msg.nsd_id_ref = self._nsds[nsd_id].id
            nsd_msg.instance_ref_count = self._nsds[nsd_id].ref_count
            nsd_list.append((nsd_refcount_xpath(nsd_id), nsd_msg))

        return nsd_list

    @asyncio.coroutine
    def terminate_ns(self, nsr_id, xact):
        """
        Terminate network service for the given NSR Id
        """

        # Terminate the instances/networks assocaited with this nw service
        self._log.debug("Terminating the network service %s", nsr_id)
        yield from self._nsrs[nsr_id].terminate()

        # Unref the NSD
        yield from self.nsd_unref_by_nsr_id(nsr_id)

        # Unpublish the NSR record
        self._log.debug("Unpublishing the network service %s", nsr_id)
        yield from self._nsrs[nsr_id].unpublish(xact)

        # Finaly delete the NS instance from this NS Manager
        self._log.debug("Deletng the network service %s", nsr_id)
        self.delete_nsr(nsr_id)


class NsmRecordsPublisherProxy(object):
    """ This class provides a publisher interface that allows plugin objects
        to publish NSR/VNFR/VLR"""

    def __init__(self, dts, log, loop, nsr_pub_hdlr, vnfr_pub_hdlr, vlr_pub_hdlr):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsr_pub_hdlr = nsr_pub_hdlr
        self._vlr_pub_hdlr = vlr_pub_hdlr
        self._vnfr_pub_hdlr = vnfr_pub_hdlr

    @asyncio.coroutine
    def publish_nsr(self, xact, nsr):
        """ Publish an NSR """
        path = NetworkServiceRecord.xpath_from_nsr(nsr)
        return (yield from self._nsr_pub_hdlr.update(xact, path, nsr))

    @asyncio.coroutine
    def unpublish_nsr(self, xact, nsr):
        """ Unpublish an NSR """
        path = NetworkServiceRecord.xpath_from_nsr(nsr)
        return (yield from self._nsr_pub_hdlr.delete(xact, path))

    @asyncio.coroutine
    def publish_vnfr(self, xact, vnfr):
        """ Publish an VNFR """
        path = VirtualNetworkFunctionRecord.vnfr_xpath(vnfr)
        return (yield from self._vnfr_pub_hdlr.update(xact, path, vnfr))

    @asyncio.coroutine
    def unpublish_vnfr(self, xact, vnfr):
        """ Unpublish a VNFR """
        path = VirtualNetworkFunctionRecord.vnfr_xpath(vnfr)
        return (yield from self._vnfr_pub_hdlr.delete(xact, path))

    @asyncio.coroutine
    def publish_vlr(self, xact, vlr):
        """ Publish a VLR """
        path = VirtualLinkRecord.vlr_xpath(vlr)
        return (yield from self._vlr_pub_hdlr.update(xact, path, vlr))

    @asyncio.coroutine
    def unpublish_vlr(self, xact, vlr):
        """ Unpublish a VLR """
        path = VirtualLinkRecord.vlr_xpath(vlr)
        return (yield from self._vlr_pub_hdlr.delete(xact, path))


class NsmTasklet(rift.tasklets.Tasklet):
    """
    The network service manager  tasklet
    """
    def __init__(self, *args, **kwargs):
        super(NsmTasklet, self).__init__(*args, **kwargs)
        self.rwlog.set_category("rw-mano-log")
        self.rwlog.set_subcategory("nsm")

        self._dts = None
        self._nsm = None

        self._cloud_plugin_selector = None
        self._vnffgmgr = None

        self._nsr_handler = None
        self._vnfr_pub_handler = None
        self._vlr_pub_handler = None
        self._vnfd_pub_handler = None
        self._scale_cfg_handler = None

        self._records_publisher_proxy = None

    def start(self):
        """ The task start callback """
        super(NsmTasklet, self).start()
        self.log.info("Starting NsmTasklet")

        self.log.debug("Registering with dts")
        self._dts = rift.tasklets.DTS(self.tasklet_info,
                                      RwNsmYang.get_schema(),
                                      self.loop,
                                      self.on_dts_state_change)

        self.log.debug("Created DTS Api GI Object: %s", self._dts)

    def stop(self):
        try:
            self._dts.deinit()
        except Exception:
            print("Caught Exception in NSM stop:", sys.exc_info()[0])
            raise

    def on_instance_started(self):
        """ Task instance started callback """
        self.log.debug("Got instance started callback")

    @asyncio.coroutine
    def init(self):
        """ Task init callback """
        self.log.debug("Got instance started callback")

        self.log.debug("creating config account handler")

        self._nsr_pub_handler = publisher.NsrOpDataDtsHandler(self._dts, self.log, self.loop)
        yield from self._nsr_pub_handler.register()

        self._vnfr_pub_handler = publisher.VnfrPublisherDtsHandler(self._dts, self.log, self.loop)
        yield from self._vnfr_pub_handler.register()

        self._vlr_pub_handler = publisher.VlrPublisherDtsHandler(self._dts, self.log, self.loop)
        yield from self._vlr_pub_handler.register()


        manifest = self.tasklet_info.get_pb_manifest()
        use_ssl = manifest.bootstrap_phase.rwsecurity.use_ssl
        ssl_cert = manifest.bootstrap_phase.rwsecurity.cert
        ssl_key = manifest.bootstrap_phase.rwsecurity.key

        self._vnfd_pub_handler = publisher.VnfdPublisher(use_ssl, ssl_cert, ssl_key, self.loop)

        self._records_publisher_proxy = NsmRecordsPublisherProxy(
                self._dts,
                self.log,
                self.loop,
                self._nsr_pub_handler,
                self._vnfr_pub_handler,
                self._vlr_pub_handler,
                )

        # Register the NSM to receive the nsm plugin
        # when cloud account is configured
        self._cloud_plugin_selector = cloud.CloudAccountNsmPluginSelector(
                self._dts,
                self.log,
                self.log_hdl,
                self.loop,
                self._records_publisher_proxy,
                )
        yield from self._cloud_plugin_selector.register()

        self._vnffgmgr = rwvnffgmgr.VnffgMgr(self._dts, self.log, self.log_hdl, self.loop)
        yield from self._vnffgmgr.register()

        self._nsm = NsManager(
                self._dts,
                self.log,
                self.loop,
                self._nsr_pub_handler,
                self._vnfr_pub_handler,
                self._vlr_pub_handler,
                self._cloud_plugin_selector,
                self._vnffgmgr,
                self._vnfd_pub_handler
                )

        yield from self._nsm.register()

    @asyncio.coroutine
    def run(self):
        """ Task run callback """
        pass

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application into the corresponding application state

        Arguments
            state - current dts state
        """
        switch = {
            rwdts.State.INIT: rwdts.State.REGN_COMPLETE,
            rwdts.State.CONFIG: rwdts.State.RUN,
        }

        handlers = {
            rwdts.State.INIT: self.init,
            rwdts.State.RUN: self.run,
        }

        # Transition application to next state
        handler = handlers.get(state, None)
        if handler is not None:
            yield from handler()

        # Transition dts to next state
        next_state = switch.get(state, None)
        if next_state is not None:
            self.log.debug("Changing state to %s", next_state)
            self._dts.handle.set_state(next_state)
