import asyncio

import rift.mano.config_agent
import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwNsrYang', '1.0')
from gi.repository import (
    RwDts as rwdts,
    NsrYang,
)

class RiftCMRPCHandler(object):
    """ The Network service Monitor DTS handler """
    EXEC_NS_CONF_XPATH = "I,/nsr:exec-ns-config-primitive"
    EXEC_NS_CONF_O_XPATH = "O,/nsr:exec-ns-config-primitive"

    GET_NS_CONF_XPATH = "I,/nsr:get-ns-config-primitive-values"
    GET_NS_CONF_O_XPATH = "O,/nsr:get-ns-config-primitive-values"

    def __init__(self, dts, log, loop, nsm):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._nsm = nsm

        self._ns_regh = None
        self._vnf_regh = None
        self._get_ns_conf_regh = None

        self.job_manager = rift.mano.config_agent.ConfigAgentJobManager(dts, log, loop, nsm)

    @property
    def reghs(self):
        """ Return registration handles """
        return (self._ns_regh, self._vnf_regh, self._get_ns_conf_regh)

    @property
    def nsm(self):
        """ Return the NS manager instance """
        return self._nsm

    def prepare_meta(self, rpc_ip):

        try:
            nsr_id = rpc_ip.nsr_id_ref
            nsr = self._nsm.nsrs[nsr_id]
            vnfrs = {}
            for vnfr in nsr.vnfrs:
                vnfr_id = vnfr.id
                # vnfr is a dict containing all attributes
                vnfrs[vnfr_id] = vnfr

            return nsr, vnfrs
        except KeyError as e:
            raise ValueError("Record not found", str(e))

    @asyncio.coroutine
    def _get_ns_cfg_primitive(self, nsr_id, ns_cfg_name):
        nsd_msg = yield from self._nsm.get_nsd(nsr_id)

        def get_nsd_cfg_prim(name):
            for ns_cfg_prim in nsd_msg.config_primitive:
                if ns_cfg_prim.name == name:
                    return ns_cfg_prim
            return None

        ns_cfg_prim_msg = get_nsd_cfg_prim(ns_cfg_name)
        if ns_cfg_prim_msg is not None:
            ret_cfg_prim_msg = ns_cfg_prim_msg.deep_copy()
            return ret_cfg_prim_msg
        return None

    @asyncio.coroutine
    def _get_vnf_primitive(self, vnfr_id, nsr_id, primitive_name):
        vnf = self._nsm.get_vnfr_msg(vnfr_id, nsr_id)
        self._log.debug("vnfr_msg:%s", vnf)
        if vnf:
            self._log.debug("nsr/vnf {}/{}, vnf_configuration: %s",
                            vnf.vnf_configuration)
            for primitive in vnf.vnf_configuration.config_primitive:
                if primitive.name == primitive_name:
                    return primitive

        raise ValueError("Could not find nsr/vnf {}/{} primitive {}"
                         .format(nsr_id, vnfr_id, primitive_name))

    @asyncio.coroutine
    def register(self):
        """ Register for NS monitoring read from dts """
        yield from self.job_manager.register()

        @asyncio.coroutine
        def on_ns_config_prepare(xact_info, action, ks_path, msg):
            """ prepare callback from dts exec-ns-config-primitive"""
            assert action == rwdts.QueryAction.RPC
            rpc_ip = msg
            rpc_op = NsrYang.YangOutput_Nsr_ExecNsConfigPrimitive()

            try:
                ns_cfg_prim_name = rpc_ip.name
                nsr_id = rpc_ip.nsr_id_ref
                nsr = self._nsm.nsrs[nsr_id]

                nsd_cfg_prim_msg = yield from self._get_ns_cfg_primitive(nsr_id, ns_cfg_prim_name)

                def find_nsd_vnf_prim_param_pool(vnf_index, vnf_prim_name, param_name):
                    for vnf_prim_group in nsd_cfg_prim_msg.vnf_primitive_group:
                        if vnf_prim_group.member_vnf_index_ref != vnf_index:
                            continue

                        for vnf_prim in vnf_prim_group.primitive:
                            if vnf_prim.name != vnf_prim_name:
                                continue

                            try:
                                nsr_param_pool = nsr.param_pools[pool_param.parameter_pool]
                            except KeyError:
                                raise ValueError("Parameter pool %s does not exist in nsr" % vnf_prim.parameter_pool)

                            self._log.debug("Found parameter pool %s for vnf index(%s), vnf_prim_name(%s), param_name(%s)",
                                            nsr_param_pool, vnf_index, vnf_prim_name, param_name)
                            return nsr_param_pool

                    self._log.debug("Could not find parameter pool for vnf index(%s), vnf_prim_name(%s), param_name(%s)",
                                vnf_index, vnf_prim_name, param_name)
                    return None

                rpc_op.nsr_id_ref = nsr_id
                rpc_op.name = ns_cfg_prim_name

                nsr, vnfrs = self.prepare_meta(rpc_ip)
                rpc_op.job_id = nsr.job_id

                # Give preference to user defined script.
                if nsd_cfg_prim_msg and nsd_cfg_prim_msg.has_field("user_defined_script"):
                    rpc_ip.user_defined_script = nsd_cfg_prim_msg.user_defined_script

                    tasks = []
                    for config_plugin in self.nsm.config_agent_plugins:
                        task = yield from config_plugin.apply_ns_config(
                            nsr,
                            vnfrs,
                            rpc_ip)
                        tasks.append(task)

                    self.job_manager.add_job(rpc_op, tasks)
                else:
                    for vnf in rpc_ip.vnf_list:
                        vnf_op = rpc_op.vnf_out_list.add()
                        vnf_member_idx = vnf.member_vnf_index_ref
                        vnfr_id = vnf.vnfr_id_ref
                        vnf_op.vnfr_id_ref = vnfr_id
                        vnf_op.member_vnf_index_ref = vnf_member_idx
                        for primitive in vnf.vnf_primitive:
                            op_primitive = vnf_op.vnf_out_primitive.add()
                            op_primitive.name = primitive.name
                            op_primitive.execution_id = ''
                            op_primitive.execution_status = 'completed'
                            self._log.debug("%s:%s Got primitive %s:%s",
                                            nsr_id, vnf.member_vnf_index_ref, primitive.name, primitive.parameter)

                            nsd_vnf_primitive = yield from self._get_vnf_primitive(
                                vnfr_id,
                                nsr_id,
                                primitive.name
                            )
                            for param in nsd_vnf_primitive.parameter:
                                if not param.has_field("parameter_pool"):
                                    continue

                                try:
                                    nsr_param_pool = nsr.param_pools[param.parameter_pool]
                                except KeyError:
                                    raise ValueError("Parameter pool %s does not exist in nsr" % param.parameter_pool)
                                nsr_param_pool.add_used_value(param.value)

                            for config_plugin in self.nsm.config_agent_plugins:
                                yield from config_plugin.vnf_config_primitive(nsr_id,
                                                                              vnfr_id,
                                                                              primitive,
                                                                              op_primitive)

                    self.job_manager.add_job(rpc_op)

                # Get NSD
                # Find Config Primitive
                # For each vnf-primitive with parameter pool
                # Find parameter pool
                # Add used value to the pool
                self._log.debug("RPC output: {}".format(rpc_op))
                xact_info.respond_xpath(rwdts.XactRspCode.ACK,
                                        RiftCMRPCHandler.EXEC_NS_CONF_O_XPATH,
                                        rpc_op)
            except Exception as e:
                self._log.error("Exception processing the "
                                "exec-ns-config-primitive: {}".format(e))
                self._log.exception(e)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK,
                                        RiftCMRPCHandler.EXEC_NS_CONF_O_XPATH)

        @asyncio.coroutine
        def on_get_ns_config_values_prepare(xact_info, action, ks_path, msg):
            assert action == rwdts.QueryAction.RPC
            nsr_id = msg.nsr_id_ref
            cfg_prim_name = msg.name
            try:
                nsr = self._nsm.nsrs[nsr_id]

                rpc_op = NsrYang.YangOutput_Nsr_GetNsConfigPrimitiveValues()

                ns_cfg_prim_msg = self._get_ns_cfg_primitive(nsr_id, cfg_prim_name)

                # Get pool values for NS-level parameters
                for ns_param in ns_cfg_prim_msg.parameter:
                    if not ns_param.has_field("parameter_pool"):
                        continue

                    try:
                        nsr_param_pool = nsr.param_pools[ns_param.parameter_pool]
                    except KeyError:
                        raise ValueError("Parameter pool %s does not exist in nsr" % ns_param.parameter_pool)

                    new_ns_param = rpc_op.ns_parameter.add()
                    new_ns_param.name = ns_param.name
                    new_ns_param.value = str(nsr_param_pool.get_next_unused_value())

                # Get pool values for NS-level parameters
                for vnf_prim_group in ns_cfg_prim_msg.vnf_primitive_group:
                    rsp_prim_group = rpc_op.vnf_primitive_group.add()
                    rsp_prim_group.member_vnf_index_ref = vnf_prim_group.member_vnf_index_ref
                    if vnf_prim_group.has_field("vnfd_id_ref"):
                        rsp_prim_group.vnfd_id_ref = vnf_prim_group.vnfd_id_ref

                    for index, vnf_prim in enumerate(vnf_prim_group.primitive):
                        rsp_prim = rsp_prim_group.primitive.add()
                        rsp_prim.name = vnf_prim.name
                        rsp_prim.index = index
                        vnf_primitive = yield from self._get_vnf_primitive(
                                vnf_prim_group.vnfd_id_ref,
                                nsr_id,
                                vnf_prim.name
                        )
                        for param in vnf_primitive.parameter:
                            if not param.has_field("parameter_pool"):
                                continue

                # Get pool values for NS-level parameters
                for ns_param in ns_cfg_prim_msg.parameter:
                    if not ns_param.has_field("parameter_pool"):
                        continue

                    try:
                        nsr_param_pool = nsr.param_pools[ns_param.parameter_pool]
                    except KeyError:
                        raise ValueError("Parameter pool %s does not exist in nsr" % ns_param.parameter_pool)

                    new_ns_param = rpc_op.ns_parameter.add()
                    new_ns_param.name = ns_param.name
                    new_ns_param.value = str(nsr_param_pool.get_next_unused_value())

                # Get pool values for NS-level parameters
                for vnf_prim_group in ns_cfg_prim_msg.vnf_primitive_group:
                    rsp_prim_group = rpc_op.vnf_primitive_group.add()
                    rsp_prim_group.member_vnf_index_ref = vnf_prim_group.member_vnf_index_ref
                    if vnf_prim_group.has_field("vnfd_id_ref"):
                        rsp_prim_group.vnfd_id_ref = vnf_prim_group.vnfd_id_ref

                    for index, vnf_prim in enumerate(vnf_prim_group.primitive):
                        rsp_prim = rsp_prim_group.primitive.add()
                        rsp_prim.name = vnf_prim.name
                        rsp_prim.index = index
                        vnf_primitive = yield from self._get_vnf_primitive(
                                nsr_id,
                                vnf_prim_group.member_vnf_index_ref,
                                vnf_prim.name
                                )
                        for param in vnf_primitive.parameter:
                            if not param.has_field("parameter_pool"):
                                continue

                            try:
                                nsr_param_pool = nsr.param_pools[param.parameter_pool]
                            except KeyError:
                                raise ValueError("Parameter pool %s does not exist in nsr" % vnf_prim.parameter_pool)

                            vnf_param = rsp_prim.parameter.add()
                            vnf_param.name = param.name
                            vnf_param.value = str(nsr_param_pool.get_next_unused_value())

                self._log.debug("RPC output: {}".format(rpc_op))
                xact_info.respond_xpath(rwdts.XactRspCode.ACK,
                                        RiftCMRPCHandler.GET_NS_CONF_O_XPATH, rpc_op)
            except Exception as e:
                self._log.error("Exception processing the "
                                "get-ns-config-primitive-values: {}".format(e))
                self._log.exception(e)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK,
                                        RiftCMRPCHandler.GET_NS_CONF_O_XPATH)

        hdl_ns = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_ns_config_prepare,)
        hdl_ns_get = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_get_ns_config_values_prepare,)

        with self._dts.group_create() as group:
            self._ns_regh = group.register(xpath=RiftCMRPCHandler.EXEC_NS_CONF_XPATH,
                                           handler=hdl_ns,
                                           flags=rwdts.Flag.PUBLISHER,
                                           )
            self._get_ns_conf_regh = group.register(xpath=RiftCMRPCHandler.GET_NS_CONF_XPATH,
                                                    handler=hdl_ns_get,
                                                    flags=rwdts.Flag.PUBLISHER,
                                                    )


