
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio

from gi.repository import (
    RwDts as rwdts,
    RwsdnYang,
    RwTypes,
    ProtobufC,
)

from gi.repository.RwTypes import RwStatus
import rw_peas
import rift.tasklets

class SdnGetPluginError(Exception):
    """ Error while fetching SDN plugin """
    pass


class SdnGetInterfaceError(Exception):
    """ Error while fetching SDN interface"""
    pass


class SdnAccountError(Exception):
    """ Error while creating/deleting/updating SDN Account"""
    pass

class VnffgrDoesNotExist(Exception):
    """ Error while fetching SDN interface"""
    pass

class VnffgrAlreadyExist(Exception):
    """ Vnffgr already exists Error"""
    pass

class VnffgrCreationFailed(Exception):
    """ Error while creating VNFFGR"""
    pass


class VnffgrUpdateFailed(Exception):
    """ Error while updating VNFFGR"""
    pass

class VnffgMgr(object):
    """ Implements the interface to backend plugins to fetch topology """
    def __init__(self, dts, log, log_hdl, loop):
        self._account = {}
        self._dts = dts
        self._log = log
        self._log_hdl = log_hdl
        self._loop = loop
        self._sdn = {}
        self._sdn_handler = SDNAccountDtsHandler(self._dts,self._log,self)
        self._vnffgr_list = {}

    @asyncio.coroutine
    def register(self):
        yield from self._sdn_handler.register()

    def set_sdn_account(self,account):
        if (account.name in self._account):
            self._log.error("SDN Account is already set")
        else:
            sdn_account           = RwsdnYang.SDNAccount()
            sdn_account.from_dict(account.as_dict())
            sdn_account.name = account.name
            self._account[account.name] = sdn_account
            self._log.debug("Account set is %s , %s",type(self._account), self._account)

    def del_sdn_account(self, name):
        self._log.debug("Account deleted is %s , %s", type(self._account), name)
        del self._account[name]

    def update_sdn_account(self,account):
        self._log.debug("Account updated is %s , %s", type(self._account), account)
        if account.name in self._account:
            sdn_account = self._account[account.name]

            sdn_account.from_dict(
                account.as_dict(),
                ignore_missing_keys=True,
                )
            self._account[account.name] = sdn_account

    def get_sdn_account(self, name):
        """
        Creates an object for class RwsdnYang.SdnAccount()
        """
        if (name in self._account):
            return self._account[name]
        else:
            self._log.error("SDN account is not configured")


    def get_sdn_plugin(self,name):
        """
        Loads rw.sdn plugin via libpeas
        """
        if (name in self._sdn):
            return self._sdn[name]
        account = self.get_sdn_account(name)
        plugin_name = getattr(account, account.account_type).plugin_name
        self._log.debug("SDN plugin being created")
        plugin = rw_peas.PeasPlugin(plugin_name, 'RwSdn-1.0')
        engine, info, extension = plugin()

        self._sdn[name] = plugin.get_interface("Topology")
        try:
            rc = self._sdn[name].init(self._log_hdl)
            assert rc == RwStatus.SUCCESS
        except:
            self._log.error("ERROR:SDN plugin instantiation failed ")
        else:
            self._log.debug("SDN plugin successfully instantiated")
        return self._sdn[name]

    def fetch_vnffgr(self,vnffgr_id):
        if vnffgr_id not in self._vnffgr_list:
            self._log.error("VNFFGR with id %s not present in VNFFGMgr", vnffgr_id)
            msg = "VNFFGR with id {} not present in VNFFGMgr".format(vnffgr_id)
            raise VnffgrDoesNotExist(msg)
        self.update_vnffgrs(self._vnffgr_list[vnffgr_id].sdn_account)
        vnffgr = self._vnffgr_list[vnffgr_id].deep_copy()
        self._log.debug("VNFFGR for id %s is %s",vnffgr_id,vnffgr)
        return vnffgr

    def create_vnffgr(self, vnffgr,classifier_list,sff_list):
        """
        """
        self._log.debug("Received VNFFG chain Create msg %s",vnffgr)
        if vnffgr.id in self._vnffgr_list:
            self._log.error("VNFFGR with id %s already present in VNFFGMgr", vnffgr.id)
            vnffgr.operational_status = 'failed'
            msg = "VNFFGR with id {} already present in VNFFGMgr".format(vnffgr.id)
            raise VnffgrAlreadyExist(msg)

        self._vnffgr_list[vnffgr.id] = vnffgr
        vnffgr.operational_status = 'init'
        if len(self._account) == 0:
            self._log.error("SDN Account not configured")
            vnffgr.operational_status = 'failed'
            return
        if vnffgr.sdn_account:
            sdn_acct_name = vnffgr.sdn_account
        else:
            self._log.error("SDN Account is not associated to create VNFFGR")
            # TODO Fail the VNFFGR creation if SDN account is not associated
            #vnffgr.operational_status = 'failed'
            #msg = "SDN Account is not associated to create VNFFGR"
            #raise VnffgrCreationFailed(msg)
            sdn_account = [sdn_account.name for _,sdn_account in self._account.items()]
            sdn_acct_name = sdn_account[0]
            vnffgr.sdn_account = sdn_acct_name
        sdn_plugin = self.get_sdn_plugin(sdn_acct_name)

        for rsp in vnffgr.rsp:
            vnffg = RwsdnYang.VNFFGChain()
            vnffg.name = rsp.name
            vnffg.classifier_name = rsp.classifier_name

            vnfr_list = list()
            for index,cp_ref in enumerate(rsp.vnfr_connection_point_ref):
                cpath = vnffg.vnf_chain_path.add()
                cpath.order=cp_ref.hop_number
                cpath.service_function_type = cp_ref.service_function_type
                cpath.nsh_aware=True
                cpath.transport_type = 'vxlan-gpe'

                vnfr=cpath.vnfr_ids.add()
                vnfr.vnfr_id = cp_ref.vnfr_id_ref
                vnfr.vnfr_name = cp_ref.vnfr_name_ref
                vnfr.mgmt_address = cp_ref.connection_point_params.mgmt_address
                vnfr.mgmt_port = 5000
                vnfr_list.append(vnfr)
            
                vdu = vnfr.vdu_list.add()
                vdu.name = cp_ref.connection_point_params.name
                vdu.port_id = cp_ref.connection_point_params.port_id
                vdu.vm_id = cp_ref.connection_point_params.vm_id
                vdu.address = cp_ref.connection_point_params.address
                vdu.port =  cp_ref.connection_point_params.port

            for sff in sff_list.values():
                _sff = vnffg.sff.add()
                _sff.from_dict(sff.as_dict())
                if sff.function_type == 'SFF':
                    for vnfr in vnfr_list:
                        vnfr.sff_name = sff.name
                self._log.debug("Recevied SFF %s, Created SFF is %s",sff, _sff)

            self._log.debug("VNFFG chain msg is %s",vnffg)
            rc,rs = sdn_plugin.create_vnffg_chain(self._account[sdn_acct_name],vnffg)
            if rc != RwTypes.RwStatus.SUCCESS:
                vnffgr.operational_status = 'failed'
                msg = "Instantiation of VNFFGR with id {} failed".format(vnffgr.id)
                raise VnffgrCreationFailed(msg)

            self._log.info("VNFFG chain created successfully for rsp with id %s",rsp.id)


        meta = {}
        if(len(classifier_list) == 2):
            meta[vnffgr.classifier[0].id] = '0x' + ''.join(str("%0.2X"%int(i)) for i in vnffgr.classifier[1].ip_address.split('.'))
            meta[vnffgr.classifier[1].id] = '0x' + ''.join(str("%0.2X"%int(i)) for i in vnffgr.classifier[0].ip_address.split('.'))
            
        self._log.debug("VNFFG Meta VNFFG chain is {}".format(meta))
        
        for classifier in classifier_list:
            vnffgr_cl = [_classifier  for _classifier in vnffgr.classifier if classifier.id == _classifier.id]
            if len(vnffgr_cl) > 0:
                cl_rsp_name = vnffgr_cl[0].rsp_name
            else:
                self._log.error("No RSP wiht name %s found; Skipping classifier %s creation",classifier.rsp_id_ref,classifier.name)
                continue
            vnffgcl = RwsdnYang.VNFFGClassifier()
            vnffgcl.name = classifier.name
            vnffgcl.rsp_name = cl_rsp_name
            vnffgcl.port_id = vnffgr_cl[0].port_id
            vnffgcl.vm_id = vnffgr_cl[0].vm_id
            # Get the symmetric classifier endpoint ip and set it in nsh ctx1
            
            vnffgcl.vnffg_metadata.ctx1 =  meta.get(vnffgr_cl[0].id,'0') 
            vnffgcl.vnffg_metadata.ctx2 = '0'
            vnffgcl.vnffg_metadata.ctx3 = '0'
            vnffgcl.vnffg_metadata.ctx4 = '0'
            if vnffgr_cl[0].has_field('sff_name'):
                vnffgcl.sff_name = vnffgr_cl[0].sff_name
            for index,match_rule in enumerate(classifier.match_attributes):
                acl = vnffgcl.match_attributes.add()
                #acl.name = vnffgcl.name + str(index)
                acl.name = match_rule.id
                acl.ip_proto  = match_rule.ip_proto
                acl.source_ip_address = match_rule.source_ip_address + '/32'
                acl.source_port = match_rule.source_port
                acl.destination_ip_address = match_rule.destination_ip_address + '/32'
                acl.destination_port = match_rule.destination_port

            self._log.debug(" Creating VNFFG Classifier Classifier %s for RSP: %s",vnffgcl.name,vnffgcl.rsp_name)
            rc,rs = sdn_plugin.create_vnffg_classifier(self._account[sdn_acct_name],vnffgcl)
            if rc != RwTypes.RwStatus.SUCCESS:
                self._log.error("VNFFG Classifier cretaion failed for Classifier %s for RSP ID: %s",classifier.name,classifier.rsp_id_ref)
                #vnffgr.operational_status = 'failed'
                #msg = "Instantiation of VNFFGR with id {} failed".format(vnffgr.id)
                #raise VnffgrCreationFailed(msg)

        vnffgr.operational_status = 'running'
        self.update_vnffgrs(vnffgr.sdn_account)
        return vnffgr

    def update_vnffgrs(self,sdn_acct_name):
        """
        Update VNFFGR by reading data from SDN Plugin
        """
        sdn_plugin = self.get_sdn_plugin(sdn_acct_name)
        rc,rs = sdn_plugin.get_vnffg_rendered_paths(self._account[sdn_acct_name])
        if rc != RwTypes.RwStatus.SUCCESS:
            msg = "Reading of VNFFGR from SDN Plugin failed"
            raise VnffgrUpdateFailed(msg)

        vnffgr_list = [_vnffgr for _vnffgr in self._vnffgr_list.values()  if _vnffgr.sdn_account == sdn_acct_name and _vnffgr.operational_status == 'running']

        for _vnffgr in vnffgr_list:
            for _vnffgr_rsp in _vnffgr.rsp:
                vnffg_rsp_list = [vnffg_rsp for vnffg_rsp in rs.vnffg_rendered_path if vnffg_rsp.name == _vnffgr_rsp.name]
                if vnffg_rsp_list is not None and len(vnffg_rsp_list) > 0:
                    vnffg_rsp = vnffg_rsp_list[0]
                    if len(vnffg_rsp.rendered_path_hop) != len(_vnffgr_rsp.vnfr_connection_point_ref):
                        _vnffgr.operational_status = 'failed'
                        self._log.error("Received hop count %d doesnt match the VNFFGD hop count %d", len(vnffg_rsp.rendered_path_hop),
                                         len(_vnffgr_rsp.vnfr_connection_point_ref))
                        msg = "Fetching of VNFFGR with id {} failed".format(_vnffgr.id)
                        raise VnffgrUpdateFailed(msg)
                    _vnffgr_rsp.path_id =  vnffg_rsp.path_id
                    for index, rendered_hop in enumerate(vnffg_rsp.rendered_path_hop):
                        for  vnfr_cp_ref in _vnffgr_rsp.vnfr_connection_point_ref:
                            if rendered_hop.vnfr_name == vnfr_cp_ref.vnfr_name_ref:
                               vnfr_cp_ref.hop_number = rendered_hop.hop_number
                               vnfr_cp_ref.service_index = rendered_hop.service_index
                               vnfr_cp_ref.service_function_forwarder.name = rendered_hop.service_function_forwarder.name
                               vnfr_cp_ref.service_function_forwarder.ip_address = rendered_hop.service_function_forwarder.ip_address
                               vnfr_cp_ref.service_function_forwarder.port = rendered_hop.service_function_forwarder.port
                else:
                    _vnffgr.operational_status = 'failed'
                    self._log.error("VNFFGR RSP with name %s in VNFFG %s not found",_vnffgr_rsp.name, _vnffgr.id)
                    msg = "Fetching of VNFFGR with name {} failed".format(_vnffgr_rsp.name)
                    raise VnffgrUpdateFailed(msg)


    def terminate_vnffgr(self,vnffgr_id,sdn_account_name = None):
        """
        Deletet the VNFFG chain
        """
        if vnffgr_id not in self._vnffgr_list:
            self._log.error("VNFFGR with id %s not present in VNFFGMgr during termination", vnffgr_id)
            msg = "VNFFGR with id {} not present in VNFFGMgr during termination".format(vnffgr_id)
            return
            #raise VnffgrDoesNotExist(msg)
        self._log.info("Received VNFFG chain terminate for id %s",vnffgr_id)
        if sdn_account_name is None:
            sdn_account = [sdn_account.name for _,sdn_account in self._account.items()]
            sdn_account_name = sdn_account[0]
        sdn_plugin = self.get_sdn_plugin(sdn_account_name)
        sdn_plugin.terminate_vnffg_chain(self._account[sdn_account_name],vnffgr_id)
        sdn_plugin.terminate_vnffg_classifier(self._account[sdn_account_name],vnffgr_id)
        del self._vnffgr_list[vnffgr_id]

class SDNAccountDtsHandler(object):
    XPATH = "C,/rw-sdn:sdn-account"

    def __init__(self, dts, log, parent):
        self._dts = dts
        self._log = log
        self._parent = parent

        self._sdn_account = {}

    def _set_sdn_account(self, account):
        self._log.info("Setting sdn account: {}".format(account))
        if account.name in self._sdn_account:
            self._log.error("SDN Account with name %s already exists. Ignoring config", account.name);
        self._sdn_account[account.name]  = account
        self._parent.set_sdn_account(account)

    def _del_sdn_account(self, account_name):
        self._log.info("Deleting sdn account: {}".format(account_name))
        del self._sdn_account[account_name]

        self._parent.del_sdn_account(account_name)

    def _update_sdn_account(self, account):
        self._log.info("Updating sdn account: {}".format(account))
        # No need to update locally saved sdn_account's updated fields, as they
        # are not used anywhere. Call the parent's update callback.
        self._parent.update_sdn_account(account)

    @asyncio.coroutine
    def register(self):
        def apply_config(dts, acg, xact, action, _):
            self._log.debug("Got sdn account apply config (xact: %s) (action: %s)", xact, action)
            if action == rwdts.AppconfAction.INSTALL and xact.id is None:
                self._log.debug("No xact handle.  Skipping apply config")
                return RwTypes.RwStatus.SUCCESS

            return RwTypes.RwStatus.SUCCESS

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ Prepare callback from DTS for SDN Account config """

            self._log.info("SDN Cloud account config received: %s", msg)

            fref = ProtobufC.FieldReference.alloc()
            fref.goto_whole_message(msg.to_pbcm())

            if fref.is_field_deleted():
                # Delete the sdn account record
                self._del_sdn_account(msg.name)
            else:
                # If the account already exists, then this is an update.
                if msg.name in self._sdn_account:
                    self._log.debug("SDN account already exists. Invoking on_prepare update request")
                    if msg.has_field("account_type"):
                        errmsg = "Cannot update SDN account's account-type."
                        self._log.error(errmsg)
                        xact_info.send_error_xpath(RwTypes.RwStatus.FAILURE,
                                                   SDNAccountDtsHandler.XPATH,
                                                   errmsg)
                        raise SdnAccountError(errmsg)

                    # Update the sdn account record
                    self._update_sdn_account(msg)
                else:
                    self._log.debug("SDN account does not already exist. Invoking on_prepare add request")
                    if not msg.has_field('account_type'):
                        errmsg = "New SDN account must contain account-type field."
                        self._log.error(errmsg)
                        xact_info.send_error_xpath(RwTypes.RwStatus.FAILURE,
                                                   SDNAccountDtsHandler.XPATH,
                                                   errmsg)
                        raise SdnAccountError(errmsg)

                    # Set the sdn account record
                    self._set_sdn_account(msg)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)


        self._log.debug("Registering for Sdn Account config using xpath: %s",
                        SDNAccountDtsHandler.XPATH,
                        )

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )

        with self._dts.appconf_group_create(acg_handler) as acg:
            acg.register(
                    xpath=SDNAccountDtsHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY,
                    on_prepare=on_prepare
                    )



