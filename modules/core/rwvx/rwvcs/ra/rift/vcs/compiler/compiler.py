"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file compiler.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 09/18/2014
#

The classes in this module are used to 'compile' an RaManifest object from a
SystemInfo object. The SystemInfo object contains a set of objects that
represent the raw data required to generate a manifest and bring up the system.
These objects have essentially no functionality or dependencies, and are
designed to be easy for anyone to use to define a riftware system. All of the
complexity that goes into manifest generation is intended to reside in the
classes in this module.

The ManifestCompiler (as it's name suggest) is responsible for compiling the
manifest from the SystemInfo object. The way that it does this is through a set
of Constructor objects. Each Constructor object converts a raw data object into
an object that is a part of the manifest. Constructors can be thought of as an
implementation of the Factory Pattern. The reason for using Constructors is to
provide a way to extend the classes the compiler can support without modifying
the compiler itself. In this way, client libraries can provide their own set of
constructors and do not require modification of the compiler.

"""

import copy
import logging

import rift.vcs
import rift.vcs.core
import rift.vcs.manifest

from .constructors import (
        FastpathTaskletCtor,
        CliTaskletCtor,
        GenericComponentCtor,
        LegacyProcCtor,
        LegacyTaskletCtor,
        LegacyUAgentCtor,
        LegacyVmCtor,
        NativeProcessCtor,
        TaskletCtor,
        VirtualMachineCtor,
        )
from .constraints import (
        AdjacentAgentRestconfTasklets,
        CheckComponentNamesAreNotNone,
        CheckFastpathInducedUniqueNames,
        CheckNumberOfBrokers,
        CheckNumberOfDtsRouters,
        CheckNumberOfDtsPerfMgr,
        CheckNumberOfLogd,
        UniqueInstanceIds,
        UniqueUAgent,
        )
from .transforms import (
        AddLogd,
        AddValgrind,
        AssignBrokerVM,
        AssignClusterIds,
        AssignDtsRouterVM,
        AssignFastpathIds,
        AssignIPAddresses,
        AssignFastpathInducedUniqueNames,
        AssignInitVM,
        AssignVirtualMachineIds,
        AssignWebServersUAgentPort,
        AssignWebserversConfdHost,
        SetManifestFlags,
        StartupColony,
        EnforceDtsPerfMgrPolicy,
        EnforceDtsRouterPolicy,
        EnforceMessageBrokerPolicy,
        WebServersConfdHost,
        AssignNetconfHostToRiftCli,
        UseMockCli
        )

logger = logging.getLogger(__name__)


class ManifestCompiler(object):
    """
    The ManifestCompiler is reponsible for compiling a manifest from an system
    info object. The way that it does this is through a set of Constructor
    objects. Each Constructor object converts a raw data object into an object
    that is a part of the manifest. Constructors can be thought of as an
    implementation of the Factory Pattern. The reason for using Constructors is
    to provide a way to extend the classes the compiler can support without
    modifying the compiler itself. In this way, client libraries can provide
    their own set of constructors and do not need to modify the compiler itself.
    """

    def __init__(self):
        """Creates a ManifestCompiler object"""

        # The constructors dictionary maps an object class to a constructor
        # object. The types listed here are the core types, which are
        # essentially represent raw data, that are used create the objects in an
        # actual manifest.
        self.constructors = {
                rift.vcs.Colony: GenericComponentCtor(self, rift.vcs.manifest.RaColony),
                rift.vcs.Cluster: GenericComponentCtor(self, rift.vcs.manifest.RaCluster),
                rift.vcs.VirtualMachine: VirtualMachineCtor(self, rift.vcs.manifest.RaVm),
                rift.vcs.Proc: GenericComponentCtor(self, rift.vcs.manifest.RaProc),
                rift.vcs.Tasklet: TaskletCtor(self, rift.vcs.manifest.RaTasklet),
                rift.vcs.NativeProcess: NativeProcessCtor(self, rift.vcs.manifest.RaNativeProcess),
                }

    def create(self, obj):
        """Creates and returns an object.

        The object passed into this function is used to determine which type of
        manifest object to create. The type of the object is a key that is used
        to determine which constructor to use to create the manifest object, and
        the object itself contains the data necessary to create the manifest
        object.

        All constructors in the compiler should call this function when they
        need to create a manifest object in order to create the correct type of
        manifest object.

        Arguments:
            obj - an object that maps to a manifest object

        Returns:
            a manifest object

        Raises:
            A ValueError is raised if there is no associated between the
            provided objects type and the constructors known to the compiler.

        """
        # Find the the closest ancestor that the object inherits from, which maps
        # to a constructor, and use that constructor.
        objtype = type(obj)
        while objtype:
            if objtype in self.constructors:
                break
            logger.debug('-- {} does not have a constructor'.format(objtype.__name__))
            objtype = objtype.__base__
        else:
            raise ValueError('Unable to create object for {}'.format(objtype))

        # Create the manifest object
        constructor = self.constructors[objtype]
        logger.debug('creating {} using {}'.format(
            type(obj).__name__,
            type(constructor).__name__))
        return constructor(obj)

    def compile(self, sysinfo):
        """Returns a compiled manifest object

        Arguments:
            sysinfo - a SystemInfo object that contains all of the information
                      necessary to create the manifest.

        Returns:
            a manifest object

        """
        manifest = rift.vcs.manifest.RaManifest(
                northbound_listing=sysinfo.northbound_listing,
                persist_dir_name=sysinfo.persist_dir_name,
                netconf_trace=sysinfo.netconf_trace,
                agent_mode=sysinfo.agent_mode,
                test_name=sysinfo.test_name,
                rift_var_root=sysinfo.rift_var_root,
                no_heartbeat=sysinfo.no_heartbeat,
        )

        for colony in sysinfo.colonies:
            manifest.add_component(self.create(colony))

        for component in sysinfo.vm_bootstrap:
            manifest.vm_bootstrap.append(self.create(component))

        for component in sysinfo.proc_bootstrap:
            manifest.proc_bootstrap.append(self.create(component))

        return manifest


class LegacyManifestCompiler(ManifestCompiler):
    """
    This class is a specialization of the ManifestCompiler and is used to
    isolate the manifest compiler from some of the special-case logic that is
    currently present in manifest generation. The goal is that there should be
    no special handling of specialized classes; specializations should provide
    data in the way that the core classes are defined so that special handling
    of specialized classes will be unnecessary. However, until the code has
    envolved to that point, it is necessary to support legacy assumptions in
    order to maintain functionality.
    """

    def __init__(self):
        """Creates a LegacyManifestCompiler instance"""
        super(LegacyManifestCompiler, self).__init__()

        self.system_constraints = []
        self.system_transforms = []
        self.manifest_constraints = []
        self.manifest_transforms = []

        # Create constructors for the specialized tasklets
        def tasklet_ctor(prototype, cls):
            return prototype, LegacyTaskletCtor(self, cls)

        def native_ctor(prototype, cls):
            return prototype, NativeProcessCtor(self, cls)

        self.constructors.update([
            tasklet_ctor(rift.vcs.ApplicationManager, rift.vcs.manifest.RaAppMgr),
            tasklet_ctor(rift.vcs.Controller, rift.vcs.manifest.RaFpCtrl),
            tasklet_ctor(rift.vcs.InterfaceManager, rift.vcs.manifest.RaIfMgr),
            tasklet_ctor(rift.vcs.NetworkContextManager, rift.vcs.manifest.RaNcMgr),
            tasklet_ctor(rift.vcs.MsgBrokerTasklet, rift.vcs.manifest.RaBroker),
            tasklet_ctor(rift.vcs.DtsRouterTasklet, rift.vcs.manifest.RaDtsRouter),
            tasklet_ctor(rift.vcs.LogdTasklet, rift.vcs.manifest.Logd),
            tasklet_ctor(rift.vcs.ToyTasklet, rift.vcs.manifest.RaToyTasklet),
            tasklet_ctor(rift.vcs.ToyTaskletPython, rift.vcs.manifest.RaToyTaskletPython),
            tasklet_ctor(rift.vcs.DtsTaskletPython, rift.vcs.manifest.RaDtsTaskletPython),
            tasklet_ctor(rift.vcs.MockCliTasklet, rift.vcs.manifest.RaMockCli),
            native_ctor(rift.vcs.RedisCluster, rift.vcs.manifest.RaRedisCluster),
            native_ctor(rift.vcs.RedisServer, rift.vcs.manifest.RaRedisServer),
            native_ctor(rift.vcs.Webserver, rift.vcs.manifest.RaWebServer),
            native_ctor(rift.vcs.RiftCli, rift.vcs.manifest.RaCliProc),
            ])

        # Over-ride existing constructors
        self.constructors[rift.vcs.Proc] = LegacyProcCtor(self)
        self.constructors[rift.vcs.VirtualMachine] = LegacyVmCtor(self)
        self.constructors[rift.vcs.CliTasklet] = CliTaskletCtor(self)

        # Add system constraints and transforms
        self.add_system_constraint(CheckFastpathInducedUniqueNames())
        self.add_system_constraint(UniqueUAgent())
        self.add_system_constraint(AdjacentAgentRestconfTasklets())
        self.add_system_transform(AssignWebserversConfdHost())
        self.add_system_transform(WebServersConfdHost())
        self.add_system_transform(AssignWebServersUAgentPort())
        self.add_system_transform(AssignFastpathIds())
        self.add_system_transform(AssignClusterIds())
        self.add_system_transform(AssignFastpathInducedUniqueNames())
        self.add_system_transform(AssignVirtualMachineIds())
        self.add_system_transform(EnforceMessageBrokerPolicy())
        self.add_system_transform(EnforceDtsPerfMgrPolicy())
        self.add_system_transform(EnforceDtsRouterPolicy())
        self.add_system_transform(AddLogd())
        self.add_system_transform(AssignNetconfHostToRiftCli())
        self.add_system_transform(UseMockCli())

        # Add manifest constraints and transforms
        self.add_manifest_constraint(UniqueInstanceIds())
        self.add_manifest_constraint(CheckNumberOfBrokers())
        self.add_manifest_constraint(CheckNumberOfDtsRouters())
        self.add_manifest_constraint(CheckNumberOfDtsPerfMgr())
        self.add_manifest_constraint(CheckNumberOfLogd())
        self.add_manifest_constraint(CheckComponentNamesAreNotNone())
        self.add_manifest_transform(AddValgrind())
        self.add_manifest_transform(SetManifestFlags())
        self.add_manifest_transform(AssignIPAddresses())
        self.add_manifest_transform(AssignInitVM())
        self.add_manifest_transform(AssignBrokerVM())
        self.add_manifest_transform(AssignDtsRouterVM())
        self.add_manifest_transform(StartupColony())

    def add_system_constraint(self, constraint):
        self.system_constraints.append(constraint)

    def add_system_transform(self, transform):
        self.system_transforms.append(transform)

    def clear_system_constraints(self):
        self.system_constraints = []

    def clear_system_transforms(self):
        self.system_transforms = []

    def add_manifest_constraint(self, constraint):
        self.manifest_constraints.append(constraint)

    def add_manifest_transform(self, transform):
        self.manifest_transforms.append(transform)

    def clear_manifest_constraints(self):
        self.manifest_constraints = []

    def clear_manifest_transforms(self):
        self.manifest_transforms = []

    def compile(self, sysinfo):
        """Returns a compiled manifest object

        This function compiles the manifest like the ManifestCompiler class, but
        also performs several post-compilation steps to ensure that the manifest
        is in a usable state.

        Arguments:
            sysinfo - a SystemInfo object that contains all of the information
                      necessary to create the manifest.

        Returns:
            a manifest object

        """
        # For debugging write the lists of constraints
        logger.debug('compiler constraints')
        logger.debug('- system constraints')
        for constraint in self.system_constraints:
            logger.debug('  - {}'.format(constraint.__class__.__name__))

        logger.debug('- manifest constraints')
        for constraint in self.manifest_constraints:
            logger.debug('  - {}'.format(constraint.__class__.__name__))

        # For debugging write the lists of transforms
        logger.debug('compiler transforms')
        logger.debug('- system transforms')
        for transform in self.system_transforms:
            logger.debug('  - {}'.format(transform.__class__.__name__))

        logger.debug('- manifest transforms')
        for transform in self.manifest_transforms:
            logger.debug('  - {}'.format(transform.__class__.__name__))

        # A deep copy of the sytem info object is created because the
        # transforms my modify the object, and the original object passed into
        # the compiler should not be modified by the compilation itself.
        sysinfo = copy.deepcopy(sysinfo)

        # @HACK several of the constructors used by the legacy compiler are
        # stateful. To ensure that successive compilations start with clean
        # state, the constructors are created immediately prior to their use.
        # NB: this is not a mechanism that should be available in the
        # ManifestCompiler class itself (constructors should not be stateful).
        self.constructors[rift.vcs.Fastpath] = FastpathTaskletCtor(self)

        # The uAgent and WebServer ctors need to be reset for each compilation
        # because they need to know if there is a confd process in the system
        # that they need to be aware of.
        self.constructors[rift.vcs.uAgentTasklet] = LegacyUAgentCtor(self, sysinfo)

        # Transform any system components
        for transform in self.system_transforms:
            transform(sysinfo)

        # Check the system constraints
        for constraint in self.system_constraints:
            constraint(sysinfo)

        # compile the manifest from the system info
        manifest = super(LegacyManifestCompiler, self).compile(sysinfo)

        # Transform the manifest
        for transform in self.manifest_transforms:
            transform(manifest, sysinfo)

        # Check the manifest
        for constraint in self.manifest_constraints:
            constraint(manifest, sysinfo)

        # Return both the modified systeminfo object and the associated manifest
        return sysinfo, manifest

# vim: sw=4
