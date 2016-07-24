"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file onstraints.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 03/25/2015
#

The constraint class defines in this file are used by the
LegacyManifestCompiler to validate either the system info (prior to
compilation) or the manifest (following compilation).

"""

import abc
import collections
import logging

import rift.vcs

from . import exc


logger = logging.getLogger(__name__)


class SystemConstraint(object):
    """
    This class is used to check some aspect of a SystemInfo object.
    """

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def __call__(self, sysinfo):
        raise NotImplemented()


class ManifestConstraint(object):
    """
    This class is used to check some aspect of an RaManifest object.
    """

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def __call__(self, manifest, sysinfo):
        raise NotImplemented()


class UniqueUAgent(SystemConstraint):
    def __call__(self, sysinfo):
        """Check that the sysinfo contains one uAgent

        Arguments:
            sysinfo - a SystemInfo object

        Raises:
            A ConstraintError is raised if there is either no uAgent tasklet or
            if there is more than 1.

        """
        uagents = sysinfo.list_by_class(rift.vcs.uAgentTasklet)
        a_uagent, s_uagent = None, None
        for uagent in uagents:
            if uagent.mode_active:
                if not a_uagent:
                    a_uagent = uagent
                else:
                    raise exc.ConstraintError('more than one active uAgent')
            if not uagent.mode_active:
                if not s_uagent:
                    s_uagent = uagent
                else:
                    raise exc.ConstraintError('more than one standby uAgent')
        if s_uagent  and s_uagent in uagents: uagents.remove(s_uagent)

        uagents += sysinfo.list_by_class(rift.vcs.MgmtMockTasklet)
        if len(uagents) != 1:
            raise exc.ConstraintError('non-unique uagent')


class AdjacentAgentRestconfTasklets(SystemConstraint):
    def __call__(self, sysinfo):
        """Check that the system contains adjacent uAgent and restconf tasklets

        Each virtual machine in the system must contain both uAgent and restconf
        tasklets, or neither, i.e. these tasklets must be present at the same
        time.

        Arguments:
            sysinfo - a SystemInfo object

        Raises:
            A ConstraintError is raised if there is a virtual machine that
            contains a uAgent tasklet or restconf tasklet without the other.

        """
        # Define the types of components that we are looking for
        component_types = (rift.vcs.uAgentTasklet, rift.vcs.RestconfTasklet)

        # Iterate through all of the components in the system. Note that the
        # iteration represents a depth-first traversal of the components so the
        # components of a virtual machine will be encountered before the
        # virtual machine itself. Thus we track the instance of the component
        # types we are interested in and when the next virtual machine is
        # encountered, we know that that is the virtual machine that the
        # components belong to.
        components = []
        for component in sysinfo:
            # If the component is one of the types that we are interested in,
            # add it to the list of components
            if isinstance(component, component_types) and component.mode_active:
                components.append(component)
                continue

            # If the component is a virtual machine, the list of components is
            # a list of the components contained within this virtual machine.
            # There should be either no components for a virtual machine or all
            # of the target components for the system to be valid.
            if isinstance(component, rift.vcs.VirtualMachine):
                if len(components) not in (0, 2):
                    raise exc.ConstraintError("Agent and restconf must be in the same VM")

                # Reset the component list
                components = []

class UniqueInstanceIds(ManifestConstraint):
    def __call__(self, manifest, sysinfo):
        """Check that the system does not contain duplicate instance IDs

        Arguments:
            manifest - an RaManifest object
            sysinfo  - a SystemInfo object

        Raises:
            A ConstraintError is raised if two or more components are found
            with the same instance ID.

        """
        known = set()
        for component in manifest:
            if component.instance_id is not None:
                if component.instance_id in known:
                    msg = 'duplicate instance ID found in {}'
                    raise exc.ConstraintError(msg.format(component.name))
                else:
                    known.add(component.instance_id)


class CheckFastpathInducedUniqueNames(SystemConstraint):
    def __call__(self, sysinfo):
        """Check that ancestors of fastpath tasklets have unique names

        Each ancestor of a faspath tasklet is required to be a distinct
        component, because each fastpath tasklet is distinct.

        Arguments:
            sysinfo - a SystemInfo object

        Raises:
            A ConstraintError is raised if an ancestor of a fastpath tasklets
            does not have a unique component name.

        """
        # This dictionary will map a child component to its parent.
        parent = {}

        # The traversal function will recursively traverse the tree and
        # construct the parent dict.
        def traversal(root):
            try:
                for component in root.subcomponents:
                    parent[component] = root
                    traversal(component)

            except AttributeError:
                pass

        # Iterate over all of the components in the system and construct the
        # complete mapping of children to parents.
        for component in sysinfo.subcomponents:
            parent[component] = None
            traversal(component)

        # Count the number of times each component name is used in the
        # manifest.
        count = collections.Counter(c.name for c in sysinfo)

        # Check that all of the components that are ancestors of fastpath
        # tasklets have unique names.
        for tasklet in sysinfo.list_by_class(rift.vcs.Fastpath):
            current = parent[tasklet]
            while current is not None:
                if count[current.name] > 1:
                    msg = '{} is not a unique name'.format(current.name)
                    raise exc.ConstraintError(msg)

                current = parent[current]


class CheckNumberOfBrokers(ManifestConstraint):
    def __call__(self, manifest, sysinfo):
        """Check that number of brokers in the manifest

        In single-broker mode (the default), there should be no more than one
        broker in the manifest. In multi-broker mode, VCS will handle the
        creation of the necessary brokers, so there should be no brokers in the
        manifest.

        Arguments:
            manifest - an RaManifest object
            sysinfo  - a SystemInfo object

        """
        # There should never be more than one broker
        brokers = manifest.list_by_class(rift.vcs.manifest.RaBroker)
        if len(brokers) > 1:
            msg = 'A manifest should have no more than 1 message broker (single broker)'
            raise exc.ConstraintError(msg)

        if sysinfo.multi_broker and len(brokers) != 0:
            raise exc.ConstraintError(
                    'No component should have a descendent message broker (multi-broker)')

class CheckNumberOfDtsRouters(ManifestConstraint):
    def __call__(self, manifest, sysinfo):
        """Check that number of dtsrouters in the manifest

        In non multi-dtsrouter mode (the default), there should be no more than one
        dtsrouter in the manifest. In multi-dtsrouter mode, VCS will handle the
        creation of the necessary dtsrouters, so there should be no dtsrouters in the
        manifest.

        Arguments:
            manifest - an RaManifest object
            sysinfo  - a SystemInfo object

        """
        # There should never be more than one dtsrouter
        dtsrouters = manifest.list_by_class(rift.vcs.manifest.RaDtsRouter)
        if len(dtsrouters) > 1:
            msg = 'A manifest should have no more than 1 dtsrouter (single dtsrouter)'
            raise exc.ConstraintError(msg)

        if manifest.multi_dtsrouter and len(dtsrouters) != 0:
            raise exc.ConstraintError(
                    'No component should have a descendent dtsrouter (multi-dtsrouter)')

class CheckNumberOfDtsPerfMgr(ManifestConstraint):
    def __call__(self, manifest, sysinfo):
        """Check number of dtsperfmgr in manifest

        In dtsperfmgr mode, VCS will handle the creation of the
        necessary dtsperfmgr instance, therefore there should be no instance
        of dtsperfmgr in the manifest.

        Arguments:
            manifest - an RaManifest object
            sysinfo  - a SystemInfo object
        """

        dtsperfmgr = manifest.list_by_class(rift.vcs.manifest.RaDtsPerfMgrTasklet)
        if manifest.dtsperfmgr and len(dtsperfmgr) != 0:
            raise exc.ConstraintError(
                'No component should have a descendent dtsperfmgr (with-dts-perf)')

class CheckNumberOfLogd(ManifestConstraint):
    def __call__(self, manifest, sysinfo):
        """Check the number of rwlog instances in the manifest

        There should be exactly one rwlog instance per physical VM.

        Arguments:
            manifest - an RaManifest object
            sysinfo  - a SystemInfo object
        """
        logds = manifest.list_by_class(rift.vcs.manifest.Logd)
        if sysinfo.collapsed:
            if len(logds) != 1:
                raise exc.ConstraintError('Exactly one Logd must be defined per physical machine')
        else:
            if len(logds) != 0:
                raise exc.ConstraintError('Logd should not be explictly added, already in bootstrap')

            for component in manifest.vm_bootstrap:
                if isinstance(component, rift.vcs.manifest.Logd):
                    break
            else:
                raise exc.ConstraintError('Logd not added in RWVM bootstrap phase')


class CheckComponentNamesAreNotNone(ManifestConstraint):
    def __call__(self, manifest, sysinfo):
        """Check that every component in the manifest has a name

        Arguments:
            manifest - an RaManifest object
            sysinfo  - a SystemInfo object

        """
        for component in manifest:
            if component.name is None:
                msg = 'All components must have a name. {} does not.'
                raise exc.ConstraintError(msg.format(component))

# vim: sw=4
