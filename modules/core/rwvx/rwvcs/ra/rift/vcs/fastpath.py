
# STANDARD_RIFT_IO_COPYRIGHT

from . import core
from .ext import ClassProperty
from . import port




class Fastpath(core.Tasklet):
    """
    This class represents a fastpath tasklet.
    """

    def __init__(self,
            name=None,
            uid=None,
            port_map=None,
            hashbin_credit=None,
            memory_model=None,
            lcore_workers=None,
            config_ready=True,
            recovery_action=core.RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=core.DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a Fastpath object.

        Arguments:
            name           - the name of the tasklet
            uid            - a unique identifier
            port_map       - the port map used by the tasklet
            hashbin_credit - a value that indicates the amount of the available
                             hashbins this tasklet can support
            memory_model   - the memory model that fastpath should use
            lcore_workers  - the number of lcore workers to use
            config_ready   - config readiness check enable
            recovery_action - recovery action mode
            start          - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active    - active mode setting
        """
        name = 'RW.Fpath' if name is None else name
        super(Fastpath, self).__init__(name=name, uid=uid, config_ready=config_ready,
                                       recovery_action=recovery_action, start=start,
                                       data_storetype=data_storetype,
                                       mode_active=mode_active,
                                       )

        self.cmdargs = CommandLineArguments()
        self.hashbin_credit = hashbin_credit
        self.memory_model = memory_model
        self.lcore_workers = lcore_workers
        self.port_map = [] if port_map is None else port_map

    plugin_name = ClassProperty("rwfpath-c")
    plugin_directory = ClassProperty("./usr/lib/rift/plugins/rwfpath-c")


class CommandLineArguments(object):
    """
    These arguments are derived from fastpath and DPDK. For reference,

        ${RIFT_ROOT}/modules/core/fpath/app/common/src/rw_fpath_init.c
        ${RIFT_ROOT}/modules/core/fpath/DPDK/1.6.0/lib/librte_eal/linuxapp/eal/eal.c

    Not all of the arguments are currently supported.

    """

    def __init__(self):
        self.file_prefix = False
        self.no_huge = False
        self.mem_alloc = None

        self.debug_cli = False
        self.disable_link_interrupts = False
        self.cli_terminal = False
        self.lcore_workers = None
        self.disable_kni = False
        self.mem_model = None

    def arguments(self):
        # Construct the command arguments for DPDK
        dpdk_args = ""

        if self.file_prefix:
            dpdk_args += " --file-prefix fpath{id}"

        if self.no_huge:
            dpdk_args += " --no-huge"

        if self.mem_alloc is not None:
            dpdk_args += " -m {}".format(self.mem_alloc)

        # Construct the command arguments for fastpath
        fpath_args = ""

        if self.debug_cli:
            fpath_args += " -D"

        if self.disable_link_interrupts:
            fpath_args += " -N"

        if self.cli_terminal:
            fpath_args += " -t /tmp/mypty{id}"

        if self.lcore_workers is not None:
            fpath_args += " -l {}".format(self.lcore_workers)

        if self.disable_kni:
            fpath_args += " -n"

        if self.mem_model is not None:
            fpath_args += " -M {}".format(self.mem_model)

        # Combine the command arguments
        cmd = dpdk_args
        if fpath_args:
            cmd += ' --' + fpath_args

        return "'{}'".format(cmd)


def assign_port_map(sysinfo, port_map):
    """ Sets the port map on the fastpath tasklets

    Each virtual machine in the system can have one fastpath tasklet. This
    tasklet needs to have a list of ports so that it knows how to communicate
    with other components. Each VM can have only one fastpath tasklet, so it is
    convenient to associate the list of ports with the IP of the VM containing a
    fastpath tasklet.

    Arguments:
        sysinfo  - a SystemInfo object
        port_map - a dict that maps IP addresses to a list of ports

    Raises:
        A ValueError is raised if a mapped VM does not contain a fastpath
        tasklet.

    """
    for vm in sysinfo.list_by_class(core.VirtualMachine):
        if vm.ip in port_map:
            for component in core.component_iterator(vm):
                if isinstance(component, Fastpath):
                    component.port_map = port_map[vm.ip]
                    break
            else:
                raise ValueError('Unable to find fastpath instance')


def set_fastpath_cmdargs(sysinfo, cmdargs):
    """ Set the cmdargs for each Fastpath contained within a sysinfo

    Arguments:
        sysinfo - a SystemInfo object
        cmdargs - a CommandLineArguments object to assign to the cmdargs
                  attribute for each Fastpath within a sysinfo class.
    """
    for fastpath in sysinfo.list_by_class(Fastpath):
        fastpath.cmdargs = cmdargs


def assign_port_names(sysinfo, mode, name_mapping):
    """Sets the real names of the ports in the fastpath tasklets

    The function iterates over the list of fastpath tasklets and then over the
    list ports associated with the tasklets. If the ports on the tasklet have a
    logical name in the port_names dict, the real name of the port is assigned.

    Arguments:
        sysinfo      - a SystemInfo object
        mode         - a string describing the mode to use for the port names
        name_mapping - a dict mapping a logical port name to a real port name

    Raises:
        There must be a mapping from every logical name in the system to a real
        name in the name_mapping. If there is not, a KeyError is raised.

    """
    def make_port_name(logical_name):
        return port.PortName(mode, name_mapping[logical_name])

    # Create a dictionary of port names
    port_names = {k: make_port_name(k) for k in name_mapping}

    # Assign the port names to the corresponding ports
    for tasklet in sysinfo.list_by_class(Fastpath):
        for p in tasklet.port_map:
            p.port_name = port_names[p.logical_name]
