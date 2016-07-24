import time

from enum import Enum

from gi.repository import NsdYang, NsrYang


class ScalingGroupIndexExists(Exception):
    pass


class ScaleGroupTrigger(Enum):
    """ Trigger for scaling config """
    PRE_SCALE_IN = 1
    POST_SCALE_IN = 2
    PRE_SCALE_OUT = 3
    POST_SCALE_OUT = 4


class ScaleGroupState(Enum):
    """ Scaling group state  """
    RUNNING = 1
    SCALING_IN = 2
    SCALING_OUT = 3


class ScalingGroup(object):
    """ This represents a configured NSR scaling group """
    def __init__(self, log, group_msg):
        """ Create a ScalingGroup instance

        This class is responsible for representing a configured scaling group
        which is present within an NSR.

        :param log: A logger instance
        :param group_msg: A NSD scaling group pb message
        """
        self._log = log
        self._group_msg = group_msg

        self._instances = {}

    def __str__(self):
        return "ScalingGroup(%s)" % self.name

    @property
    def name(self):
        """ Name of the scaling group """
        return self._group_msg.name

    @property
    def state(self):
        """ State of the scaling group """
        state = ScaleGroupState.RUNNING
        for instance in self._instances.values():
            if instance.operational_status in ["init", "vnf_init_phase"]:
                self._log.debug("Scaling instance %s in scaling-out state: %s",
                                instance, instance.operational_status)
                state = ScaleGroupState.SCALING_OUT

            elif instance.operational_status in ["terminate", "vnf_terminate_phase"]:
                self._log.debug("Scaling instance %s in scaling-in state: %s",
                                instance, instance.operational_status)
                state = ScaleGroupState.SCALING_IN

        return state

    @property
    def vnf_index_count_map(self):
        """ The mapping of member_vnf_index_ref to count"""
        return {mbr.member_vnf_index_ref: mbr.count for mbr in self._group_msg.vnfd_member}

    @property
    def group_msg(self):
        """ Return the scale group PB message """
        return self._group_msg

    @property
    def min_instance_count(self):
        """ Minimum (and default) number of instance of the scaling group """
        return self._group_msg.min_instance_count

    @property
    def max_instance_count(self):
        """ Maximum number of instance of the scaling group """
        return self._group_msg.max_instance_count

    def create_record_msg(self):
        """ Returns a NSR Scaling group record """
        msg = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_ScalingGroupRecord(
                scaling_group_name_ref=self.name,
                )

        for instance in self.instances:
            msg.instance.append(instance.create_record_msg())

        return msg

    @property
    def instances(self):
        return self._instances.values()

    def get_instance(self, instance_id):
        """ Get a scaling group instance

        :param instance_id: The instance's instance_id
        """
        return self._instances[instance_id]

    def create_instance(self, instance_id, is_default=False):
        """ Create a scaling group instance

        :param instance_id: The new instance's instance_id
        """
        self._log.debug("Creating %s instance instance_id %s ", self, instance_id)

        if instance_id in self._instances:
            raise ScalingGroupIndexExists("%s instance_id %s already exists" % (self, instance_id))

        instance = ScalingGroupInstance(
                log=self._log,
                group_name=self.name,
                instance_id=instance_id,
                is_default=is_default,
                )

        self._instances[instance_id] = instance

        return instance

    def delete_instance(self, instance_id):
        self._log.debug("Deleting %s instance instance_id %s ", self, instance_id)
        del self._instances[instance_id]

    def trigger_map(self, trigger):
        trig_map = {
            NsdYang.ScalingTrigger.PRE_SCALE_IN   : 'pre_scale_in',
            NsdYang.ScalingTrigger.POST_SCALE_IN  : 'post_scale_in',
            NsdYang.ScalingTrigger.PRE_SCALE_OUT  : 'pre_scale_out',
            NsdYang.ScalingTrigger.POST_SCALE_OUT : 'post_scale_out',
        }

        try:
            return trig_map[trigger]
        except Exception as e:
            self._log.error("Unknown scaling group trigger passed: {}".format(trigger))
            self._log.exception(e)

    def trigger_config(self, trigger):
        """ Get the config action for the trigger """
        self._log.debug("Trigger config {}: {}".format(trigger, self._group_msg))
        trig = self.trigger_map(trigger)
        if trig is None:
            return

        for config in self._group_msg.scaling_config_action:
            if trig == config.trigger:
                return config


class ScalingGroupInstance(object):
    """  This class represents a configured NSR Scaling Group instance"""

    valid_status_list = (
      "init",
      "vnf_init_phase",
      "running",
      "terminate",
      "vnf_terminate_phase",
      "terminated",
      "failed",
      )

    valid_config_status_list = (
        "configuring",
        "configured",
        "failed",
    )

    def __init__(self, log, group_name, instance_id, is_default=False):
        self._log = log
        self._group_name = group_name
        self._instance_id = instance_id
        self._is_default = is_default

        self._vnfrs = {}

        self._create_time = int(time.time())
        self._op_status = "init"
        self._config_status = "configuring"
        self._config_err_msg = None

    def __str__(self):
        return "ScalingGroupInstance(%s #%s)" % (self._group_name, self.instance_id)

    @property
    def operational_status(self):
        return self._op_status

    @operational_status.setter
    def operational_status(self, op_status):
        if op_status not in ScalingGroupInstance.valid_status_list:
            raise ValueError("Invalid scaling group instance status: %s", op_status)

        self._op_status = op_status

    @property
    def config_status(self):
        return self._config_status

    @config_status.setter
    def config_status(self, status):
        if status not in ScalingGroupInstance.valid_config_status_list:
            raise ValueError("%s, invalid status: %s",
                             self, status)

        self._config_status = status

    @property
    def config_err_msg(self):
        return self._config_err_msg

    @config_err_msg.setter
    def config_err_msg(self, msg):
        if self.config_err_msg is not None:
            self._log.info("%s, overwriting previous config error msg '%s' with '%s'",
                           self, self.config_err_msg, msg)

        self._config_err_msg = msg

    @property
    def instance_id(self):
        return self._instance_id

    @property
    def is_default(self):
        return self._is_default

    @property
    def vnfrs(self):
        """ Return all VirtualNetworkFunctionRecord's that have been added"""
        return self._vnfrs.values()

    def create_record_msg(self):
        msg = NsrYang.YangData_Nsr_NsInstanceOpdata_Nsr_ScalingGroupRecord_Instance(
                instance_id=self._instance_id,
                create_time=self._create_time,
                op_status=self._op_status,
                config_status=self._config_status,
                error_msg=self._config_err_msg,
                is_default=self._is_default
                )

        for vnfr in self.vnfrs:
            msg.vnfrs.append(vnfr.id)

        return msg

    def add_vnfr(self, vnfr):
        """ Add a VirtualNetworkFunctionRecord"""
        self._log.debug("Added %s to %s", vnfr, self)
        self._vnfrs[vnfr.id] = vnfr

