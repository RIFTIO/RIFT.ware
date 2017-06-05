"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file compiler.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 03/25/2015
#

The constructor classes in this file define methods for creating an objects
that can be used in create the RaManifest from the core classes.

"""

import abc

import rift.vcs.manifest


class Constructor(object):
    """
    This class defines the interface that all contructors must implement.
    """

    __metaclass__ = abc.ABCMeta

    def __init__(self, compiler, cls):
        """Creates a constructor object

        Arguments:
            compiler - a ManifestCompiler object
            cls      - the class of the manifest object this constructor will
                       create

        """
        self.compiler = compiler
        self.cls = cls

    def __call__(self, prototype):
        """Returns a new manifest object

        This function is a convenience function that calls the 'create' function.

        Arguments:
            prototype - an object that contains the information required to
                        create the manifest class associated with this
                        constructor.

        Returns:
            a new manifest object

        """
        return self.create(prototype)

    @abc.abstractmethod
    def create(self, prototype):
        """Returns a new manifest object

        Arguments:
            prototype - an object that contains the information required to
                        create the manifest class associated with this
                        constructor.

        Returns:
            a new manifest object

        """
        raise NotImplementedError()


class GenericComponentCtor(Constructor):
    """
    This is a constructor that is able to create simple manifest objects.
    """

    def create(self, prototype):
        """Return a manifest object

        Arguments:
            prototype - an object that contains the information required to
                        create the manifest class associated with this
                        constructor.

        Returns:
            a new manifest object

        """
        obj = self.cls(
                name=prototype.name,
                instance_id=prototype.uid,
                config_ready=prototype.config_ready,
                recovery_action=prototype.recovery_action,
                start=prototype.start,
                data_storetype=prototype.data_storetype,
                )

        for component in prototype.subcomponents:
            obj.add_component(self.compiler.create(component))

        return obj


class VirtualMachineCtor(Constructor):
    """
    This constructor is used to create virtual machine objects for the manifest.
    """

    def create(self, prototype):
        """Returns an RaVm object

        Arguments:
            prototype - an object that contains the information required to
                        create a virtual machine object.

        Returns:
            a new virtual machine object

        """
        obj = self.cls()
        if prototype.name is not None:
            obj.name = prototype.name
        obj.instance_id = prototype.uid
        obj.ipaddress = prototype.ip
        obj.valgrind = prototype.valgrind
        obj.config_ready = prototype.config_ready
        obj.recovery_action = prototype.recovery_action
        obj.start = prototype.start
        obj.data_storetype = prototype.data_storetype

        for component in prototype.subcomponents:
            obj.add_component(self.compiler.create(component))

        return obj


class TaskletCtor(Constructor):
    """
    This constructor is used to create tasklet objects for the manifest.
    """

    def create(self, prototype):
        """Returns an tasklet object

        Arguments:
            prototype - an object that contains the information required to
                        create a tasklet object.

        Returns:
            a new tasklet object

        """
        return self.cls(
                name=prototype.name,
                plugin_directory=prototype.plugin_directory,
                plugin_name=prototype.plugin_name,
                instance_id=prototype.uid,
                config_ready=prototype.config_ready,
                recovery_action=prototype.recovery_action,
                start=prototype.start,
                data_storetype=prototype.data_storetype,
                mode_active=prototype.mode_active,
                )


class NativeProcessCtor(Constructor):
    """
    This constructor is used to create native process objects for the manifest.
    """

    def create(self, prototype):
        """Returns a native process object

        Arguments:
            prototype - an object that contains the information required to
                        create a native process object.

        Returns:
            an RaNativeProcess object

        """
        obj = self.cls(
                name=prototype.name,
                exe=prototype.exe,
                args=prototype.args,
                run_as=prototype.run_as,
                config_ready=prototype.config_ready,
                recovery_action=prototype.recovery_action,
                start=prototype.start,
                data_storetype=prototype.data_storetype,
                mode_active=prototype.mode_active,
                )

        obj.instance_id = prototype.uid
        obj.valgrind = prototype.valgrind
        obj.interactive = prototype.interactive
        return obj


class LegacyProcCtor(Constructor):
    """
    This constructor creates proc objects and names them according to the legacy
    convention. Note, this is a stateful object and the proc count will not be
    reset between compilations.
    """

    def __init__(self, compiler):
        """Creates a LegacyProcCtor object

        Arguments:
            compiler - a compiler object

        """
        super(LegacyProcCtor, self).__init__(compiler, rift.vcs.manifest.RaProc)
        self._count = 0

    def _build_proc_name(self, subcomponents):
        """Build a process name in the following format:

            RW.Proc_nn.{first subcomponent's name without 'RW.'prefix}
            (e.g. RW.Proc_6.fastpath, RW.Proc_1.uAgent)

        Arguments:
            subcomponents: process's subcomponents

        """
        first_subcomponent_name = ""
        if subcomponents[0].name:
            if subcomponents[0].name.startswith("RW."):
                first_subcomponent_name = subcomponents[0].name[3:]
            else:
                first_subcomponent_name = subcomponents[0].name
        return "RW.Proc_{}.{}".format(self._count, first_subcomponent_name)

    def create(self, prototype):
        """Returns an RaProc object

        Arguments:
            prototype - an object that contains the information required to
                        create an RaProc object.

        Returns:
            a new RaProc object

        """
        obj = self.cls()
        obj.run_as = prototype.run_as
        obj.instance_id = prototype.uid
        obj.valgrind = prototype.valgrind
        obj.config_ready = prototype.config_ready
        obj.recovery_action = prototype.recovery_action
        obj.start = prototype.start
        obj.data_storetype = prototype.data_storetype
        for component in prototype.subcomponents:
            obj.add_component(self.compiler.create(component))

        # Increment the count of procs created and append the count to the name
        # of the object
        self._count += 1
        obj.name = self._build_proc_name(prototype.subcomponents)

        return obj


class LegacyVmCtor(Constructor):
    """
    This constructor creates virtual machine objects. Any sub-component, proc or
    tasklet, is also added as a startup object.
    """

    def __init__(self, compiler):
        """Creates a LegacyVmCtor object

        Arguments:
            compiler - a compiler object

        """
        super(LegacyVmCtor, self).__init__(compiler, rift.vcs.manifest.RaVm)

    def create(self, prototype):
        """Returns an RaVm object

        Arguments:
            prototype - an object that contains the information required to
                        create an RaVm object.

        Returns:
            a new RaVm object

        """
        obj = self.cls()
        obj.name = prototype.name
        obj.instance_id = prototype.uid
        obj.ipaddress = prototype.ip
        obj.leader = prototype.leader
        obj.valgrind = prototype.valgrind
        obj.config_ready = prototype.config_ready
        obj.recovery_action = prototype.recovery_action
        obj.start = prototype.start
        obj.data_storetype = prototype.data_storetype

        for component in prototype.subcomponents:
            child = self.compiler.create(component)
            obj.add_component(child)
            obj.add_startup(child)

        return obj


class LegacyTaskletCtor(Constructor):
    """
    This constructor creates a tasklet and assigns it a name and possibly a port
    number.
    """

    def create(self, prototype):
        """Returns a tasklet object

        Arguments:
            prototype - an object that contains the information required to
                        create a tasklet object.

        Returns:
            a new tasklet object

        """
        try:
            # The uagent and message broker both need to have their ports set
            # through the constructor
            tasklet = self.cls(prototype.name, port=prototype.port,
                               config_ready=prototype.config_ready,
                               recovery_action=prototype.recovery_action,
                               start=prototype.start,
                               data_storetype=prototype.data_storetype,
                               mode_active=prototype.mode_active,
                              )
        except AttributeError:
            tasklet = self.cls(prototype.name, config_ready=prototype.config_ready,
                               recovery_action=prototype.recovery_action,
                               start=prototype.start,
                               data_storetype=prototype.data_storetype,
                               mode_active=prototype.mode_active,
                              )

        tasklet.instance_id = prototype.uid
        return tasklet


class LegacyUAgentCtor(Constructor):
    """
    This constructor creates a uAgent tasklet.
    """

    def __init__(self, compiler, sysinfo):
        """Creates a LegacyUAgentCtor object

        Arguments:
            compiler - a compiler object
            sysinfo  - a SystemInfo object

        """
        super(LegacyUAgentCtor, self).__init__(
                compiler,
                rift.vcs.manifest.RaUagent)

    def create(self, prototype):
        """Returns a uAgent tasklet

        Arguments:
            prototype - an object that contains the information required to
                        create a uAgent tasklet.

        Returns:
            a new tasklet object

        """

        tasklet = self.cls(
                prototype.name,
                port=prototype.port,
                config_ready=prototype.config_ready,
                recovery_action=prototype.recovery_action,
                start=prototype.start,
                data_storetype=prototype.data_storetype,
                mode_active=prototype.mode_active,
                )
        tasklet.instance_id = prototype.uid

        return tasklet


class FastpathTaskletCtor(Constructor):
    """
    This constructor creates a fastpath tasklet, which contains several pieces
    of information that other tasklets do not.
    """

    def __init__(self, compiler):
        """Creates a FastpathTaskletCtor object

        Arguments:
            compiler - a compiler object

        """
        super(FastpathTaskletCtor, self).__init__(
                compiler,
                rift.vcs.manifest.RaFastpath
                )

    def create(self, prototype):
        """Returns an RaFastpath object

        Arguments:
            prototype - an object that contains the information required to
                        create an RaFastpath object.

        Returns:
            a new RaFastpath object

        """
        if prototype.port_map is not None:
            port_map = ','.join(str(port) for port in prototype.port_map)
        else:
            port_map = None

        tasklet = rift.vcs.manifest.RaFastpath("RW.Fpath",
                cmdargs=prototype.cmdargs,
                port_map_expr=port_map,
                memory_model=prototype.memory_model,
                lcore_workers=prototype.lcore_workers,
                hashbin_credit=prototype.hashbin_credit,
                config_ready=prototype.config_ready,
                recovery_action=prototype.recovery_action,
                start=prototype.start,
                data_storetype=prototype.data_storetype,
                mode_active=prototype.mode_active,
                )
        tasklet.instance_id = prototype.uid
        return tasklet


class CliTaskletCtor(Constructor):
    """
    This constructor creates a CLI tasklet, which contains several pieces
    of information that other tasklets do not.
    """

    def __init__(self, compiler):
        """Creates a CLITaskletCtor object

        Arguments:
            compiler - a compiler object

        """
        super(CliTaskletCtor, self).__init__(
                compiler,
                rift.vcs.manifest.RaCli
                )

    def create(self, prototype):
        """Returns a RaCli object

        Arguments:
            prototype - an object that contains the information required to
                        create an RaCli object.

        Returns:
            a new RaCli object

        """
        if prototype.manifest_file is not None:
            manifest_file = prototype.manifest_file
        else:
            manifest_file = None

        tasklet = rift.vcs.manifest.RaCli(
                "RW.CLI",
                manifest_file=manifest_file,
                config_ready=prototype.config_ready,
                recovery_action=prototype.recovery_action,
                start=prototype.start,
                data_storetype=prototype.data_storetype,
                mode_active=prototype.mode_active,
                )
        return tasklet
