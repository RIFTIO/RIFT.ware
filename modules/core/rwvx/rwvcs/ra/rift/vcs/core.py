"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file core.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 09/12/2014
# @brief Core data representations
# @details This file contains classes that are used to represent the core
# concepts of the riftware system. The classes are intended to be very simple
# and have no dependencies upon other modules in the riftware system. They are
# also not intended to do any processing -- they only provide data that
# describe the objects they represent. This means that the classes should not
# do things like querying for IP addresses or VM state; they should be static
# data that can be interpreted by other parts of the riftware system.
#
"""

from .ext import ClassProperty

import gi
gi.require_version('RwManifestYang', '1.0')
from gi.repository.RwManifestYang import RwmgmtAgentMode

from enum import Enum
class RecoveryType(Enum):
   NONE = 0
   RESTART = 1
   FAILCRITICAL = 2
   IGNORE = 3
   CUSTOM = 4

class DataStore(Enum):
   NONE = 0
   NOSTORE = 1
   REDIS = 2
   BDB = 3

class Collection(object):
    """
    A Collection is the generic container used to describe VCS hierarchies.
    Each Collection should contain one virtual machine to act as the lead for
    the collections. Beyond that requirement, the remaining subcomponents in
    the collection maybe any of the core types (including Collections).

    The Collection class should not be subclasses. RIFT.ware is expected to
    support generic collections.
    """

    def __init__(self,
            uid=None,
            type=None,
            name='rw.collection',
            lead=None,
            subcomponents=None,
            config_ready=True,
            recovery_action=RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=DataStore.NOSTORE.value,
            ):
        """Create a Collection object

        Note that the lead VM is also considered a subcomponent of the
        collection. So when iterating over the collection, the first element
        will always be the lead VM. However, it is safest to retrieve the lead
        from the 'lead' attribute of this object, rather than rely upon it
        being in the first element of the subcomponents.

        Arguments:
            uid           - a unique identifier
            type          - a string defining the type of the collection
            name          - the name of the collection
            lead          - the lead VM of the collection
            subcomponents - a list of subcomponents to add to the collection
            config_ready  - config readiness check enable
            recovery_action - recovery action mode
            start         - Flag denotes whether to initially start this collection
            data_storetype - Type of data-store used for HA
        """
        self.uid = uid
        self.type = type
        self.name = name
        self.lead = lead
        self._subcomponents = [] if subcomponents is None else subcomponents
        self.config_ready = config_ready
        self.recovery_action = recovery_action
        self.start = start
        self.data_storetype = data_storetype

    @property
    def subcomponents(self):
        """The list of subcomponents contained in the Collection"""

        if self.lead is None:
            return self._subcomponents

        return tuple([self.lead] + self._subcomponents)

    def append(self, component):
        """Append a component

        Arguments:
            component - the component to append to the collection

        """
        self._subcomponents.append(component)

    def extend(self, components):
        """Extend the subcomponents

        Arguments:
            components - a list or tuple of components to append to the
                         collection

        """
        self._subcomponents.extend(components)

    def __len__(self):
        """Returns the number of subcomponents in the collection"""
        return len(self.subcomponents)

    def __getitem__(self, index):
        """Return the component in the specified index

        Arguments:
            index - the index of the component to return

        Returns:
            A component

        """
        return self.subcomponents[index]

    def __repr__(self):
        return repr({
            "uid": self.uid,
            "type": self.type,
            "name": self.name,
            "lead": self.lead,
            "subcomponents": self.subcomponents,
            "config_ready": self.config_ready,
            "recovery_action": self.recovery_action,
            "start": self.start,
            "data_storetype":self.data_storetype,
            })


class Colony(Collection):
    """
    Represents a collection of clusters (deprecated). You should not use this
    class in future work. Use the Collection class instead.
    """

    def __init__(self, clusters=None, uid=None, name='rw.colony', config_ready=True,
                 recovery_action=RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=DataStore.NOSTORE.value,
                ):
        """Creates a Colony object.

        Arguments:
            clusters - a list of clusters
            uid      - a unique ID
            name     - the name of the cluster
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this collection
            data_storetype - Type of data-store used for HA
        """
        super(Colony, self).__init__(
                uid=uid,
                type='colony',
                name=name,
                subcomponents=clusters,
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                )

    @property
    def clusters(self):
        """The list of the clusters in the colony"""
        return self.subcomponents

    def add_cluster(self, cluster):
        """Add a cluster to the colony

        Arguments:
            cluster - the cluster to add to the colony

        """
        self.subcomponents.append(cluster)

    def __repr__(self):
        """Returns a representation of the object."""
        return repr({
            "type": type(self).__name__,
            "clusters": self.clusters,
            "name": self.name,
            "uid": self.uid,
            "config_ready": self.config_ready,
            "recovery_action": self.recovery_action,
            "start": self.start,
            "data_storetype": self.data_storetype,
            })


class Cluster(Collection):
    """
    Represents a collection of virtual machines (deprecated). You should not
    use this class in future work. Use the Collection class instead.
    """

    def __init__(self, virtual_machines=None, uid=None, name='rw.cluster', config_ready=True,
                 recovery_action=RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=DataStore.NOSTORE.value,
                 ):
        """Creates a Cluster object.

        Argument:
            virtual_machines - a list of virtual machines
            uid              - a unique ID
            name             - the name of the cluster
            config_ready     - config readiness check enable
            recovery_action  - recovery action mode
            start            - Flag denotes whether to initially start this collection
        """
        super(Cluster, self).__init__(
                uid=uid,
                type='cluster',
                name=name,
                subcomponents=virtual_machines,
                config_ready=config_ready,
                recovery_action=recovery_action,
                start=start,
                data_storetype=data_storetype,
                )

    @property
    def virtual_machines(self):
        """The list of the virtual machines in the cluster"""
        return self.subcomponents

    def add_virtual_machine(self, vm):
        """Add a virtual machine to the cluster

        Arguments:
            vm - the virtual machine to add to the cluster

        """
        self.subcomponents.append(vm)

    def __repr__(self):
        """Returns a representation of the object."""
        return repr({
            "type": type(self).__name__,
            "virtual_machines": self.virtual_machines,
            "name": self.name,
            "uid": self.uid,
            "config_ready": self.config_ready,
            "recovery_action": self.recovery_action,
            "start": self.start,
            "data_storetype": self.data_storetype,
            })


class VirtualMachine(object):
    """Represents a virtual machine."""

    def __init__(self,
            uid=None,
            ip=None,
            name='rw.vm',
            tasklets=None,
            procs=None,
            restart_procs=None,
            standby_procs=None,
            leader=False,
            valgrind=None,
            valgrind_procs=None,
            config_ready=True,
            recovery_action=RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=DataStore.NOSTORE.value,
            ):
        """Creates a VirtualMachine object.

        Argument:
            uid      - a unique ID
            ip       - the IP address of this VM
            name     - the name of this VM
            ports    - the port map of this VM
            tasklets - a list of tasklets to add to the VM
            procs    - a list of procs to add to the VM
            leader   - True if this VM is a leader for the collection that owns
                       it (the parent)
            valgrind - Enable valgrind on this vm
            valgrind_procs - a list of procs to add to the VM with valgrind enabled
            config_ready   - config readiness check enable
            recovery_action - recovery action mode
            start          - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
        """
        self.subcomponents = []
        self.leader = leader
        self.name = name
        self.uid = uid
        self.ip = ip
        self.valgrind = valgrind
        self.config_ready = config_ready
        self.recovery_action = recovery_action
        self.start =  start
        self.data_storetype = data_storetype

        if procs is not None:
            for proc in procs:
                self.add_proc(proc)

        if restart_procs is not None:
            for proc in restart_procs:
                self.add_proc(proc,
                              recovery_action=RecoveryType.RESTART.value,
                              data_storetype=proc.data_storetype,
                             )

        if standby_procs is not None:
            for proc in standby_procs:
                self.add_proc(proc,
                              recovery_action=RecoveryType.RESTART.value,
                              data_storetype=proc.data_storetype,
                              mode_active=False,
                             )

        if valgrind_procs is not None:
            for proc in valgrind_procs:
                self.add_proc(proc, valgrind=True)

        if tasklets is not None:
            for tasklet in tasklets:
                self.add_tasklet(tasklet, mode_active=tasklet.mode_active)

    @property
    def tasklets(self):
        """The list of the tasklets in the virtual machine"""
        return [c for c in self.subcomponents if isinstance(c, Tasklet)]

    @property
    def procs(self):
        """The list of the procs in the virtual machine"""
        return [c for c in self.subcomponents if isinstance(c, Proc)]

    @property
    def native_processes(self):
        """The list of the native processes in the virtual machine"""
        return [c for c in self.subcomponents if isinstance(c, NativeProcess)]

    def add_proc(self, proc, valgrind=False,
                 recovery_action=RecoveryType.FAILCRITICAL.value,
                 data_storetype=DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Add a proc to the list of procs

        If the provided proc does not derive from the Proc class, it is assumed
        to be as tasklet that should be wrapped in a Proc.

        Arguments:
            proc - the proc to add

        """
        proc.config_ready = proc.config_ready and self.start

        if isinstance(proc, Tasklet):
            proc.mode_active=mode_active
            self.subcomponents.append(Proc(tasklets=[proc], valgrind=valgrind,
                                      recovery_action=recovery_action,
                                      data_storetype=data_storetype,
                                     ))
        elif isinstance(proc, (Proc, NativeProcess)):
            if valgrind:
                proc.valgrind = valgrind
            proc.recovery_action = recovery_action
            proc.data_storetype = data_storetype
            if isinstance(proc, NativeProcess):
                proc.mode_active = mode_active
            self.subcomponents.append(proc)
        else:
            raise ValueError('Unrecognized proc object passed to VirtualMachine')

    def add_tasklet(self, tasklet,
                    recovery_action=RecoveryType.FAILCRITICAL.value,
                    data_storetype=DataStore.NOSTORE.value,
                    mode_active=True,
                   ):
        """Add a tasklet directly to the virtual machine.

        The tasklet is added directly to the virtual machine, i.e. it is not
        contained within a proc.

        Arguments:
            tasklet - the tasklet to add to the virtual machine.

        """
        tasklet.config_ready = tasklet.config_ready and self.start
        tasklet.recovery_action = recovery_action
        tasklet.data_storetype = data_storetype
        tasklet.mode_active = mode_active
        self.subcomponents.append(tasklet)

    def __repr__(self):
        """Returns a representation of the object."""
        return repr({
            "type": type(self).__name__,
            "tasklets": self.tasklets,
            "native_processes": self.native_processes,
            "procs": self.procs,
            "name": self.name,
            "uid": self.uid,
            "ip": self.ip,
            "config_ready": self.config_ready,
            "recovery_action": self.recovery_action,
            "start": self.start,
            "data_storetype": self.data_storetype,
            })


class Tasklet(object):
    """Represents a tasklet."""

    def __init__(self, name='rw.tasklet', uid=None, config_ready=True,
                 recovery_action=RecoveryType.FAILCRITICAL.value, start=True,
                 data_storetype=DataStore.NOSTORE.value,
                 mode_active=True,
                ):
        """Creates a Tasklet object.

        Argument:
            name             - the name of this tasklet
            uid              - a unique ID
            config_ready     - config readiness check enable
            recovery_action  - recovery action mode
            start            - Flag denotes whether to initially start this component
            data_storetype   - Type of data-store used for HA
            mode_active      - active mode setting
        """
        self.name = name
        self.uid = uid
        self.config_ready = config_ready
        self.recovery_action = recovery_action
        self.start = start
        self.data_storetype = data_storetype
        self.mode_active = mode_active

    def __repr__(self):
        """Returns a representation of the object."""
        return repr({
            "type": type(self).__name__,
            "plugin_directory": self.plugin_directory,
            "plugin_name": self.plugin_name,
            "name": self.name,
            "uid": self.uid,
            "config_ready": self.config_ready,
            "recovery_action": self.recovery_action,
            "start": self.start,
            "data_storetype":self.data_storetype,
            "mode_active":self.mode_active,
            })

    # Each tasklet represents a plugin and has a name associated with the
    # plugin
    plugin_name = ClassProperty()

    # Each tasklet represents a plugin and has a directory associated with the
    # plugin
    plugin_directory = ClassProperty()


class NativeProcess(object):
    """Represents a native process"""

    def __init__(self,
            uid=None,
            name='rw.native-process',
            exe=None,
            args=None,
            run_as=None,
            valgrind=None,
            interactive=False,
            config_ready=True,
            recovery_action=RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=DataStore.NOSTORE.value,
            mode_active=True,
            ):
        """Creates a NativeProcess object.

        Argument:
            uid    - a unique ID
            name   - the name of this tasklet
            exe    - the absolute path to the executable
            args   - a string containing the arguments to pass to the executable
            run_as - the user that the process is run as
            valgrind - enable valgrind
            interactive - Native process runs in an interactive mode (like CLI)
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
            mode_active - active mode setting
        """
        self.uid = uid
        self.name = name
        self.run_as = run_as
        self._exe = exe
        self._args = args
        self.valgrind = valgrind
        self.interactive = interactive
        self.config_ready = config_ready
        self.recovery_action = recovery_action
        self.start = start
        self.data_storetype = data_storetype
        self.mode_active = mode_active

    def __repr__(self):
        """Returns a representation of the object."""
        return repr({
            "type": type(self).__name__,
            "exe": self._exe,
            "args": self._args,
            "name": self.name,
            "run_as": self.run_as,
            "uid": self.uid,
            "interactive": self.interactive,
            "config_ready": self.config_ready,
            "recovery_action": self.recovery_action,
            "start": self.start,
            "data_storetype": self.data_storetype,
            "mode_active": self.mode_active,
            })

    @property
    def exe(self):
        return self._exe

    @property
    def args(self):
        return self._args

    def set_exe(self, exe):
        """ Set this Processes exe"""
        self._exe = exe

    def set_args(self, args):
        """ Set this Processes argument string"""
        self._args = args


class Proc(object):
    """Represents a proc."""

    def __init__(self,
            uid=None,
            name="rw.proc",
            run_as=None,
            tasklets=None,
            valgrind=None,
            config_ready=True,
            recovery_action=RecoveryType.FAILCRITICAL.value,
            start=True,
            data_storetype=DataStore.NOSTORE.value,
            ):
        """Creates a Tasklet object.

        Argument:
            uid      - a unique ID
            name     - the name of this prov
            run_as   - the user that the process is run as
            tasklets - a list of tasklets
            config_ready - config readiness check enable
            recovery_action - recovery action mode
            start        - Flag denotes whether to initially start this component
            data_storetype - Type of data-store used for HA
        """
        self.uid = uid
        self.name = name
        self.run_as = run_as
        self.subcomponents = [] if tasklets is None else tasklets
        self.valgrind = valgrind
        self.config_ready = config_ready
        self.recovery_action = recovery_action
        self.start = start
        self.data_storetype = data_storetype

    @property
    def tasklets(self):
        """The list of the tasklets in the proc"""
        return self.subcomponents

    def add_tasklet(self, tasklet):
        """Add a tasklet to the proc

        Arguments:
            tasklet - the tasklet to add to the proc

        """
        tasklet.config_ready = tasklet.config_ready and self.start
        self.subcomponents.append(tasklet)

    def __repr__(self):
        """Returns a representation of the object."""
        return repr({
            "type": type(self).__name__,
            "tasklets": self.tasklets,
            "run_as": self.run_as,
            "name": self.name,
            "valgrind": self.valgrind,
            "uid": self.uid,
            "config_ready": self.config_ready,
            "recovery_action": self.recovery_action,
            "start": self.start,
            "data_storetype": self.data_storetype,
            })



class SystemInfo(object):
    """A summary of the information required to create a system manifest."""

    def __init__(self,
            colonies=None,
            mode=None,
            collapsed=True,
            zookeeper=None,
            multi_broker=False,
            multi_dtsrouter=False,
            dtsperfmgr=False,
            valgrind=None,
            vm_bootstrap=None,
            proc_bootstrap=None,
            northbound_listing="rwbase_schema_listing.txt",
            netconf_trace=None,
            mgmt_ip_list=[],
            agent_mode=RwmgmtAgentMode.AUTOMODE,
            persist_dir_name="persist.riftware",
            ):
        """Creates a SystemInfo object.

        Arguments:
            colonies           - a list of the colonies in the system
            mode               - the mode of the system
            collapsed          - a flag indicating whether the system should be run
                                 in collapsed (True) more or expanded (False).
            zookepper          - the zookeeper instance to use
            multi_broker       - a flag the determines whether the system will run in
                                 single-broker or multi-broker mode.
            multi_dtsrouter    - a flag the determines whether the system will run in
                                 single-dtsrouter or multi-dtsrouter mode.
            dtsperfmgr         - a flag that determines whether the system will run the
                                 dtsperfmgr or not.
            valgrind           - a list of components that are wrapped by valgrind
            vm_bootstrap       - a list of components that are part of RWVM bootstrapping
            proc_bootstrap     - a list of components that are part of RWProc bootstrapping
            northbound_listing - the northbound schema list.
            agent-mode         - Management Agent mode; RwmgmtAgentMode.CONFD or RwmgmtAgentMode.RWXML
            netconf_trace      - Knob to enable/disable netconf trace generation.
                                  Valid values - ENABLE / DISABLE / AUTO
            mgmt_ip_list       - list of management ip addresses
            persist_dir_name   - The configuration persistence directory name.

        """
        assert agent_mode in [RwmgmtAgentMode.CONFD, RwmgmtAgentMode.RWXML, RwmgmtAgentMode.AUTOMODE]
        self.collapsed = collapsed
        self.subcomponents = [] if colonies is None else colonies
        self.mode = mode
        self.zookeeper = zookeeper
        self.multi_broker = multi_broker
        self.multi_dtsrouter = multi_dtsrouter
        self.dtsperfmgr = dtsperfmgr
        self.valgrind = [] if valgrind is None else valgrind
        self.vm_bootstrap = [] if vm_bootstrap is None else vm_bootstrap
        self.proc_bootstrap = [] if proc_bootstrap is None else proc_bootstrap
        self.use_rw_shell = False
        self.northbound_listing = northbound_listing
        self.mock_cli = False
        self.agent_mode = agent_mode
        self.netconf_trace = netconf_trace
        self.mgmt_ip_list = mgmt_ip_list
        self.persist_dir_name = persist_dir_name

        def add_to_subcomponents(components):
            for component in components:
                if component not in self.subcomponents:
                    self.subcomponents.append(component)

        add_to_subcomponents(self.vm_bootstrap)
        add_to_subcomponents(self.proc_bootstrap)

    def __repr__(self):
        """Returns a representation of the object."""
        return repr({
            "multi_broker": self.multi_broker,
            "multi_dtsrouter": self.multi_dtsrouter,
            "dtsperfmgr": self.dtsperfmgr,
            "collapsed": self.collapsed,
            "zookeeper": self.zookeeper,
            "colonies": self.colonies,
            "mode": self.mode})

    def __iter__(self):
        """Returns an iterator over the system components"""
        for component in self.subcomponents:
            for cluster in component_iterator(component):
                yield cluster

    def parent(self, child):
        """Returns the parent of the specified component

        Arguments:
            child - the component whose parent we are trying to find

        Raises:
            If the component does not have a parent, a ValueError is raised.
            Note, that the SystemInfo is treated as a parent for top-level
            collections.

        """

        for component in self:
            try:
                if child in component.subcomponents:
                    return component
            except AttributeError:
                pass

        return None

    def remove(self, component):
        """Remove an component from the system

        This function traverses the tree of components and removed the
        specified object if it is found. If the component contains
        subcomponents, they are also remove from the system.

        Arguments:
            component - the component to remove

        Raises:
            A ValueError is raised if the specified object cannot be found in
            the system.

        """
        for c in component_iterator(self):
            try:
                c.subcomponents.remove(component)
                return
            except (AttributeError, ValueError):
                pass

        raise ValueError('unable to find component: {}'.format(component))

    @property
    def colonies(self):
        return [c for c in self.subcomponents if isinstance(c, Colony)]

    @property
    def ip_addresses(self):
        """The list of IP addresses associated with VMs in the system"""
        return {vm.ip: vm for vm in self.list_by_class(VirtualMachine)}

    def list_by_type(self, component_type):
        """Returns a list of components if the specific type.

        The components returned are tested for strict equality of type, i.e.
        derived types are not returned.

        Arguments:
            component_type - the type of component to match.

        Returns:
            A list of components

        """
        return [component for component in self if type(component) == component_type]

    def list_by_class(self, component_class):
        """Returns a list of components that inherit from a particular class.

        Arguments:
            component_class - the type of component to match.

        Returns:
            A list of components

        """
        return [component for component in self if isinstance(component, component_class)]

    def list_by_name(self, name):
        """Returns a list of components by name

        A simple sub-string comparison is performed to determine if the
        specified string is contained in the names of any components in the
        system.

        Arguments:
            name - the name to search for

        Returns:
            a list of components that contain the specified name

        """
        return [component for component in self if (component.name and name in component.name)]

    def find_by_class(self, component_class):
        """Return the first component matching the specified class

        Arguments:
            component_class - the class of the component to match

        Returns:
            a component of the specified class or None if no match is found.

        """
        for component in self:
            if isinstance(component, component_class):
                return component

        return None

    def find_by_name(self, name):
        """Return the first component found by name

        A simple sub-string comparison is performed to determine if the
        specified string is contained in the names of any components in the
        system.

        Arguments:
            name - the name of the component to match

        Returns:
            a component with the specified name or None if no match is found.

        """
        for component in self:
            if component.name and name in component.name:
                return component

        return None

    def find_by_type(self, component_type):
        """Return the first component matching the specified type

        Arguments:
            component_class - the type of the component to match

        Returns:
            a component of the specified type of None if no match is found.

        """
        for component in self:
            if type(component) == component_type:
                return component

        return None

    def find_ancestor_by_class(self, component_class, ancestor_class=VirtualMachine):
        """ Return first ancestor component which contains a component class.

        Arguments:
            component_class - the class of the component contained within
                              the ancestor.
            ancestor_class - the class of the ancestor to find component class in.
        """
        components = iter(self)
        for component in components:
            if isinstance(component, component_class):
                break
        else:
            return None

        for component in components:
            if isinstance(component, ancestor_class):
                return component

        return None


def component_iterator(obj):
    """Returns a generator that iterates over all subcomponents.

    The iterator performs a depth-first traversal of the component hierarchy and
    yields each component it encounters.

    Arguments:
        obj - the root object that the iterators starts at.

    Returns:
        a subcomponent generator

    """
    def traversal(root):
        try:
            for component in root.subcomponents:
                for child in traversal(component):
                    yield child
        except AttributeError:
            pass

        yield root

    return traversal(obj)


def list_by_type(obj, component_type):
    """Return a list of components that match the specified type

    Arguments:
        obj            - the root object that the iterators starts at
        component_type - the type that the components are tested against

    Returns:
        a list of components

    """
    def predicate(component):
        return type(component) == component_type

    return list_by_predicate(obj, predicate)


def list_by_class(obj, component_class):
    """Return a list of components that derive from the specified class

    Arguments:
        obj             - the root object that the iterators starts at
        component_class - the class that the components are tested against

    Returns:
        a list of components

    """
    def predicate(component):
        return isinstance(component, component_class)

    return list_by_predicate(obj, predicate)

def list_by_predicate(obj, predicate):
    """Returns a filtered list of components

    This function iterates over all of the components in the specified object
    and returns a list of the components that return true when passed to the
    predicate function.

    Arguments:
        obj       - the root object that the iterators starts at.
        predicate - a function that return True/False when a component is passed
                    to it.

    Returns:
        a list of components

    """
    return [c for c in component_iterator(obj) if predicate(c)]


def find_by_class(obj, component_class):
    """Returns the first component of the specified class

    This function traverses the component hierarchy and returns the first
    component of the specified class or that inherits from the specified class.

    Arguments:
        obj - the root object of the search

    Returns:
        A component in the hierarchy or None

    """
    for component in component_iterator(obj):
        if isinstance(component, component_class):
            return component

    return None


def find_by_type(obj, component_type):
    """Returns the first component of the specified type

    This function traverses the component hierarchy and returns the first
    component of the specified type.

    Arguments:
        obj - the root object of the search

    Returns:
        A component in the hierarchy or None

    """
    for component in component_iterator(obj):
        if type(component) == component_type:
            return component

    return None
