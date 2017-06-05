"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file demo.py
@author Joshua Downer (joshua.downer@riftio.com)
@date 10/06/2014

"""
import argparse
import collections
import copy
import itertools
import logging
import os
import sys
import tempfile
import xml.etree.ElementTree
import yaml

import gi
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('NsdYang', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwYang', '1.0')

import gi.repository.RwVnfdYang as rwvnfd
import gi.repository.NsdYang as rwnsd
import gi.repository.RwManifestYang as rwmanifest
import gi.repository.RwYang
import ndl
from gi.repository.RwManifestYang import NetconfTrace

import rift.auto.proxy
import rift.vcs
import rift.vcs.api
import rift.vcs.compiler
import rift.vcs.core
import rift.vcs.fastpath
import rift.vcs.manifest
import rift.vcs.mgmt

import rift.auto.ip_utils
import rift.auto.proxy
import rift.auto.vm_utils

logger = logging.getLogger(__name__)


class MissingModeError(Exception):
    pass


class UnsupportedModeError(Exception):
    pass


class ReservationError(Exception):
    pass


class ConfigurationError(Exception):
    pass

class ResourceNotFoundError(Exception):
    pass


class Demo(object):
    """
    This class is a convenience class for setting up a demo. It encapsulates a
    setup of steps that are common to most demos, and provides some helper
    functions to make that process simpler.
    """

    def __init__(self, sysinfo, port_map=None, port_names=None, port_groups=None):
        """Create a Demo object.

        Arguments:
            sysinfo     - A SystemInfo object the describes the generic topology
                          of the system.
            port_map    - A dictionary mapping an IP address to a list of logical
                          port names.
            port_names  - A dictionary containing a mapping from logical names to
                          real names for each mode that is supported by this demo.
            port_groups - A dictionary containing a mapping from group name to a
                          list of logical port names that are physically connected.

        """
        self._sysinfo = copy.deepcopy(sysinfo)
        self._port_map = {} if port_map is None else port_map
        self._port_names = {} if port_map is None else port_names
        self._port_groups = {} if port_map is None else port_groups

    def _create_port_map(self):
        """Convert the port map data into a form that can be passed the
        rift.vcs.create_port_from_logical_name function.

        This function uses the demo port map data to construct the generic port
        map for the system. The port map data is mapping from an IP address to a
        description of the portmap for the IP. The result is a port map for each
        fastpath tasklet that contains a placeholder logical name and port type,
        but not real port name (the real port name is determined later using the
        mode that the system is going to be in).
        """
        new_port_map = collections.defaultdict(list)
        for ip in self._port_map:
            for port_name in self._port_map[ip]:
                new_port_map[ip].append(rift.vcs.port.create_port_from_logical_name(port_name))

        return new_port_map

    def _find_port_in_port_map(self, logical_name):
        """ Find a port in the current instance's port map using
        a logical name.

        Arguments:
            logical_name - The logical name to search for.

        Returns: The port if we find a Port with a matching logical name,
                 None otherwise.
        """
        for ip in self._port_map:
            for port in self._port_map[ip]:
                if port.logical_name == logical_name:
                    return port

        return None

    @property
    def port_modes(self):
        return list(self._port_names.keys())

    @property
    def sysinfo(self):
        """ The Demo's SystemInfo instance """
        return self._sysinfo

    def _set_up_logging(self, verbose):
        """ Set up logging level based on the verbose flag and preserve the logging
        level of the zookeepr client, kazoo.

        Arguments:
            verbose - verbose logging (i.e. logging.DEBUG) when True
        """
        kazoo_log_level = logging.getLogger('kazoo').getEffectiveLevel()
        logging.getLogger().setLevel(logging.DEBUG if verbose else logging.INFO)
        logging.getLogger('kazoo').setLevel(kazoo_log_level)

    def prepare_system(self,
                       mode,
                       collapsed=True,
                       mock_cli=False,
                       multi_broker=True,
                       multi_dtsrouter=True,
                       dtsperfmgr=False,
                       ip_list=None,
                       mgmt_ip_list=[],
                       valgrind=None,
                       verbose=False,
                       use_gdb=False,
                       track_memory=False,
                       northbound_listing="rwbase_schema_listing.txt",
                       netconf_trace=NetconfTrace.AUTO,
                       agent_mode="AUTOMODE",
                       persist_dir_name="persist.riftware",
                       test_name=None,
                       rift_var_root=None,
                       no_heartbeat=False,
                       ):
        """Returns a PreparedSystem object encapsulating the SystemInfo and PortNetwork objects

        Arguments:
            mode            - the mode that the system will be run in
            collapsed       - a flag indicating whether the system is to be run in
                              collapsed mode or not.
            mock_cli        - a flag indicating whether the system is to be run with
                              a fake cli instead of the real one
            multi_broker    - a flag indicating whether the system is operating in
                              multi-broker mode or not (single-broker).
            multi_dtsrouter - a flag indicating whether the system is operating in
                              multi-dtsrouter mode or not (single-dtsrouter).
            dtsperfmgr      - a flag indicating whether the system is running the DTS
                              Performance Manager or not
            ip_list         - a list of ips to provision to VM instances
            mgmt_ip_list    - a list of management ips
            valgrind        - a list of component names to execute under valgrind
            verbose         - enable debug logging
            use_gdb         - riftware launched under gdb
            track_memory    - riftware launched with LD_PRELOAD based memory-tracking
            northbound_listing - The northbound schema list.
            netconf_trace   - Knob to control netconf trace generation
            agent_mode      - Management Agent mode; CONFD or RWXML
            persist_dir_name - The configuration persistence directory name
            test_name        - Testcase name
            rift_var_root    - prefix for RIFT ROOT path
            no_heartbeat     - Disable heartbeat 

        Raises:
            An UnsupportedModeError is raised if the specified mode is not in
            this demos 'port names' map.

        Returns:
            a tuple containing a SystemInfo object and a PortNetwork object.

        """
        if agent_mode not in ["CONFD", "RWXML"]:
            agent_mode = rift.vcs.mgmt.default_agent_mode()

        if self._port_names and mode not in self._port_names:
            raise UnsupportedModeError("Mode (%s) not found in port_names." % mode)

        #setting up debug option for G_SLICE for 12578 
        if collapsed:
            import os
            os.environ["G_SLICE"] = "debug-blocks"
        

        # The system info contain in this object is a template. We do not want
        # to change it so create a deep copy.
        sysinfo = copy.deepcopy(self._sysinfo)

        sysinfo.mode = mode
        sysinfo.collapsed = collapsed
        sysinfo.northbound_listing = northbound_listing
        sysinfo.netconf_trace = netconf_trace
        sysinfo.agent_mode = agent_mode
        sysinfo.persist_dir_name = persist_dir_name
        sysinfo.test_name = test_name
        sysinfo.rift_var_root = rift_var_root
        sysinfo.no_heartbeat = no_heartbeat 
        sysinfo.multi_broker = multi_broker
        sysinfo.multi_dtsrouter = multi_dtsrouter
        sysinfo.dtsperfmgr = dtsperfmgr
        sysinfo.valgrind = [] if valgrind is None else valgrind
        sysinfo.mock_cli = mock_cli
        self._set_up_logging(verbose)
        sysinfo.zookeeper = rift.vcs.manifest.RaZookeeper(
                master_ip = self.find_master_ip(sysinfo),
                # Use zake in collapsed mode and zookeeper in expanded mode
                zake=collapsed)

        port_network =  None
        if self._port_map:
            # Create a dict of port objects that will be configured and shared by
            # the SytemInfo and PortNetwork objects. The ports are indexed by the
            # generic logical name. NB: these objects are shared to ensure that the
            # SystemInfo and PortNetwork objects contain the same information.
            ports = {}
            for ip in self._port_map:
                for name in self._port_map[ip]:
                    ports[name] = rift.vcs.port.create_port_from_logical_name(name)

            # Create a map from the IP addresses to a list of ports objects
            port_map = collections.defaultdict(list)
            for ip in self._port_map:
                for name in self._port_map[ip]:
                    port_map[ip].append(ports[name])

            # Iterate through the VMs and assign the port to the appropriate tasklet
            Demo.assign_port_maps(sysinfo, port_map)

            # Define the real port names of the ports
            if self._port_names:
                Demo.assign_real_port_names(sysinfo, self._port_names)

            # Define the logical names of the ports based on the component hierarchy
            Demo.assign_logical_port_names(sysinfo)

            # Create the port network
            if self._port_groups:
                port_network = Demo.create_port_network(self._port_groups, ports)
            else:
                port_network = None

        if ip_list is not None:
            self.remap_ip_addresses(sysinfo, ip_list)

        sysinfo.mgmt_ip_list = mgmt_ip_list

        return sysinfo, port_network


    @staticmethod
    def create_port_network(port_groups, ports):
        """Return a populated PortNetwork

        Create a PortNetwork using the ports named in the port_groups and the
        actual port objects in the ports dict.

        Arguments:
            port_groups - A dictionary of port_names mapped to a list of logical port names.
                          (e.g. {"<port_name>":["<logical_port>", ...]})
            ports       - A dictionary mapping logical port names to port
                          objects

        Returns:
            A new PortNetwork object with new Groups named from the dictionary keys
            and associated Port instances found from in the port_map.

        """
        network = rift.vcs.port.PortNetwork()

        for group_name in port_groups:
            group = rift.vcs.port.PortGroup(group_name)
            for logical_name in port_groups[group_name]:
                group.add_port(ports[logical_name])

            network.add_port_group(group)

        return network

    @staticmethod
    def assign_port_maps(sysinfo, port_map):
        """Add port maps to fastpath tasklets

        Arguments:
            sysinfo  - the SystemInfo object to add the port map to
            port_map - a dict mapping IP addresses to lists of ports

        """
        for vm in sysinfo.list_by_class(rift.vcs.VirtualMachine):
            fastpath = rift.vcs.core.find_by_class(vm, rift.vcs.Fastpath)
            if fastpath is not None:
                fastpath.port_map = port_map[vm.ip]

    @staticmethod
    def assign_real_port_names(sysinfo, port_names):
        """Assign real port names to the ports in the SystemInfo object

        Arguments:
            sysinfo    - a SystemInfo object
            port_names - a dict mapping logical port names to real port names

        """
        rift.vcs.fastpath.assign_port_names(
                sysinfo,
                sysinfo.mode,
                port_names[sysinfo.mode])

    @staticmethod
    def assign_logical_port_names(sysinfo):
        """Assign the correct logical names to the port maps

        The first part of an external ports logical name is the name of the
        cluster that contains them. Therefore each cluster must be named, and
        the name must be unique. The first part of a fabric ports logical name
        is just 'vfap'.

        Arguments:
            sysinfo - the SystemInfo object to be modified

        Raises:
            A ConfigurationError will be raised if:

                1. A cluster does not have a name or the name is not unique
                2. An unrecognized port type in encountered
                3. Not all of the fastpath tasklets were renamed

        """
        tasklets = []
        names = []
        for component in sysinfo:
            # If a fastpath tasklet is found add it to the list of tasklets
            if isinstance(component, rift.vcs.Fastpath):
                tasklets.append(component)

            # If a cluster is found, all of the tasklets in the tasklets list
            # will be fastpath tasklet within the cluster. Thus all of these
            # tasklets will have the same prefix for external ports (the name of
            # the cluster).
            elif isinstance(component, rift.vcs.Cluster):
                name = component.name

                # The cluster must have a name
                if name is None or name == "":
                    raise ConfigurationError('All clusters must be named')

                # The name must be unique
                if name in names:
                    raise ConfigurationError('Cluster names must be unique')

                names.append(name)

                # Go through all of the cached tasklets and rename the logical
                # port names accordingly.
                for tasklet in tasklets:
                    for port in tasklet.port_map:
                        _, cid, uid = port.logical_name.split('/')
                        if isinstance(port, rift.vcs.Fabric):
                            port.logical_name = '/'.join(['vfap', cid, uid])
                        elif isinstance(port, rift.vcs.External):
                            port.logical_name = '/'.join([name, cid, uid])
                        else:
                            raise ConfigurationError('Unrecognized port type')

                # Clear the tasklet cache
                tasklets = []

        if tasklets:
            raise ConfigurationError('Some tasklets were unassigned')

    def find_master_ip(self, sysinfo):
        """Return the IP address of the master VM

        The master VM is defined as the virtual machine that contains the CLI
        tasklet. This function searches the system for the virtual machine that
        contains the CLI tasklet and returns its IP address.

        Arguments:
            sysinfo - a SystemInfo object

        Raises:
            A ConfigurationError is raised if the CLI tasklet cannot be found or
            if the CLI tasklet is not contained by a virtual machine.

        Returns:
            the IP address of the master VM

        """
        components = iter(sysinfo)

        # Iterate through the components until the Cli is found
        for component in components:
            if isinstance(component, rift.vcs.RiftCli):
                break
        else:
            raise ConfigurationError('Unable to find the CLI tasklet')

        # Iterate through the remaining components until a virtual machine is
        # found. This will be the VM that contains the CLI tasklet.
        for component in components:
            if isinstance(component, rift.vcs.VirtualMachine):
                return component.ip
        else:
            raise ConfigurationError('CliProc is not contained by a VM')

    def remap_ip_addresses(self, sysinfo, ip_list):
        """Replace all IP addresses with the list provided

        Arguments:
            sysinfo - A SystemInfo object.
            ip_list - a list of IP addresses.

        """
        virtual_machines = sysinfo.list_by_class(rift.vcs.VirtualMachine)
        for vm, ip in zip(virtual_machines, ip_list):
            logger.debug('remapping {} --> {}'.format(vm.ip, ip))
            vm.ip = ip

        # Make sure that the IP of the 'master' VM used by the zookeeper is also
        # remapped.
        sysinfo.zookeeper.master_ip = self.find_master_ip(sysinfo)


class PreparedSystem(object):
    """ This class provides a common high-level interface to control the behavior of
    a prepared SystemInfo instance.
    """

    def __init__(self,
                 sysinfo,
                 port_network=None,
                 trace_level=5,
                 xml_output=None,
                 only_gen_xml=False,
                 no_huge=False,
                 keep_netns=False,
                 skip_prepare_vm=False,
                 use_gdb=False,
                 track_memory=False,
                 resources_file=None,
		 bootstrap_log_period = 30,
		 bootstrap_log_severity = 6,
		 console_log_severity =3
                 ):
        """ Creates an instance of PreparedSystem

        Arguments:
            sysinfo - A fully compiled sysinfo object which is produced from the
                      output of Demo.prepare_system()
            port_network - A PortNetwork instance for the system
            trace_level - The trace level to start the system with (default 5).
            xml_output - Filepath to output the XML manifest
            only_gen_xml - Exit after generating the XML manifest
            no_huge - Disable huge pages
            keep_netns - Keep existing network namespaces when preparing VM's.
            use_gdb - Run rwmain under GDB
            track_memory - Run rwmain with memory-tracking enabled using LD_PRELOAD
            bootstrap_log_period - Duration during which logs are collected during system bootstrap
            bootstrap_log_severity - Severity for bootstrap logs written to local syslog file, max 7 (debug)
            console_log_severity - Severity for console logs for all categories
        """
        self._sysinfo = sysinfo
        self._port_network = port_network
        self._trace_level = trace_level
        self._bootstrap_log_period = bootstrap_log_period
        self._bootstrap_log_severity = bootstrap_log_severity
        self._console_log_severity = console_log_severity
        self._xml_output = xml_output
        if xml_output is None:
            _, self._xml_output = tempfile.mkstemp(
                    suffix='.xml',
                    prefix="manifest",
                    dir=os.environ['RIFT_ARTIFACTS'],
                    )
        self._only_gen_xml = only_gen_xml
        self._no_huge = no_huge
        self._keep_netns = keep_netns
        self._skip_prepare_vm = skip_prepare_vm
        self._use_gdb = use_gdb
        self._track_memory = track_memory

        self._resources_file = resources_file

        self._vcs = None

        if no_huge:
            cmdargs = rift.vcs.fastpath.CommandLineArguments()
            cmdargs.no_huge = True
            rift.vcs.fastpath.set_fastpath_cmdargs(sysinfo, cmdargs)

        self._compiler = rift.vcs.compiler.LegacyManifestCompiler()
        self._manifest = None

    @property
    def compiler(self):
        return self._compiler

    @property
    def sysinfo(self):
        return self._sysinfo

    @property
    def ip_list(self):
        virtual_machines = self.sysinfo.list_by_class(rift.vcs.VirtualMachine)
        ip_list = [vm.ip for vm in virtual_machines if vm.ip is not None]

        return ip_list

    @property
    def mgmt_ip_list(self):
        return self.sysinfo.mgmt_ip_list

    @property
    def port_network(self):
        return self._port_network

    @property
    def manifest(self):
        return self._manifest

    @property
    def vcs(self):
        if self._vcs is None:
            self._vcs = rift.vcs.api.RaVcsApi()

        return self._vcs

    def _generate_manifest(self):
        """Compiles the RaManifest object
        """
        # Compile the manifest
        print (self._sysinfo.mgmt_ip_list)
        self._sysinfo, self._manifest = self._compiler.compile(self.sysinfo)
        self._manifest.traceLevel = self._trace_level
        self._manifest.bootstrapLogPeriod = self._bootstrap_log_period
        self._manifest.bootstrapLogSeverity = self._bootstrap_log_severity
        self._manifest.consoleLogSeverity = self._console_log_severity

    def _get_ip_resource(self, ip_address):
        with open(self._resources_file.name) as hdl:
            yaml_data = yaml.load(hdl.read())
            resources = yaml_data["resources"]

        matches = [r for r in resources if r['ip_address'] == ip_address]
        if not matches:
            raise ResourceNotFoundError("Could not find resource for %s" % ip_address)

        return matches[0]

    def _generate_vm_epa_from_resource(self, resource=None):
        epa_attrs = rwvnfd.EpaAttributes()
        if resource is None:
            return epa_attrs

        epa_attributes = resource["epa_attributes"]
        if "epa_attributes" not in resource:
            logger.warning("Could not find epa_attributes key")
            return epa_attrs

        for attribute in epa_attrs.fields:
            if attribute not in epa_attributes:
                logger.warning("Could not find attribute %s for resource %s",
                        attribute, resource["name"])
                continue

            try:
                setattr(epa_attrs, attribute, epa_attributes[attribute])
            except Exception as e:
                logger.warning("Could not set EPA attribute %s in protobuf: %s",
                        attribute, str(e))

        logger.debug("Generated EPA attributes for resource %s: %s",
                resource["name"], epa_attrs)
        return epa_attrs

    def _generate_vnfd(self, path):
        """Write a vnfd to the specified path.

        This includes a bunch of assumptions about the network and VCS
        hierarchy which are listed below.  Right now we do not have have
        anywhere close to the necessary infrastructure to send a vnfd to a
        RIFT.environment.  The only place that even begins to have the
        information that would be contained in a vnfd is the automation
        framework.  Assumptions were being made in a variety of places in
        differing ways on how to extract a "vnfd" from the VCS hierarchy.  This
        at least moves all of those assumptions into a single place.

        Assumptions:

        - Each VNF can be mapped to a single VCS cluster that contains at least
          one fpath instance
        - Each VDU maps to a single instance of a VCS component
        """
        model = gi.repository.RwYang.Model.create_libncx()
        model.load_schema_ypbc(rwvnfd.get_schema())
        model.load_schema_ypbc(rwmanifest.get_schema())

        vnfds = rwvnfd.VNFDList()

        # Find each "vnfd" in the system.
        clusters = [c for c
                in self._sysinfo.list_by_class(rift.vcs.Cluster)
                if rift.vcs.core.find_by_type(c, rift.vcs.Fastpath)]

        for cluster in clusters:
            if not isinstance(cluster, rift.vcs.VNFD):
                vnf = rift.vcs.VNFD(
                        collection=cluster,
                        vnfd_type=cluster.name,
                        vnfd_id=cluster.uid,
                        )
            else:
                vnf = cluster

            vnfd = vnfds.vnfd.add()
            vnfd.name = vnf.name
            vnfd.type_yang = vnf.vnfd_type
            vnfd.vnfd_id = vnf.vnfd_id
            vnfd.provider = vnf.provider
            vnfd.version = vnf.version

            # Find each "vdu" in the "vnfd"
            for vm in rift.vcs.core.list_by_class(cluster, rift.vcs.VirtualMachine):
                if not isinstance(vm, rift.vcs.VDU):
                    vdu = rift.vcs.VDU(vm=vm, name=vm.name, id=vm.uid)
                else:
                    vdu = vm

                vdud = vnfd.vdus.add()
                vdud.name = vdu.name
                vdud.id = vdu.id

                resource = None
                if self._resources_file is not None:
                    try:
                        resource = self._get_ip_resource(vm.ip)
                    except ResourceNotFoundError as e:
                        logger.warning("Could not get VM %s resource: %s", vm.ip, str(e))

                epa_attrs = self._generate_vm_epa_from_resource(resource)
                vdud.rwvm_epa = epa_attrs

                def create_vm_flavor(vdu):
                    if resource is not None:
                        if "vcpu" in resource:
                            vdu.rwvm_flavor.vcpu_count = resource["vcpu"]

                        if "ram" in resource:
                            vdu.rwvm_flavor.memory_mb = resource["ram"]

                        if "disk" in resource:
                            vdu.rwvm_flavor.storage_gb = resource["disk"]

                        vdu.image = "EXISTING"
                        if "image_name" not in resource:
                            logger.warning("Did not find image_name in resource %s",
                                    resource.name)

                            vdu.image = resource.image_name

                def create_virtual_interface(iface):
                    iface.virtual_interface.virtual_interface_type = "VIRTIO"

                    if resource is not None and "epa_attributes" in resource:
                        epa_attributes = resource["epa_attributes"]
                        if "nic" not in epa_attributes:
                            logger.warning("Could not find nic key in %s epa_attributes entry",
                                    vm.ip)
                        else:
                            iface.virtual_interface.nic_hardware_type = epa_attributes["nic"]

                        if "pci_passthrough" not in epa_attributes:
                            logger.warning("Could not find pci_passthrough key in in %s", vm.ip)
                        else:
                            if epa_attributes["pci_passthrough"]:
                                iface.virtual_interface.virtual_interface_type = "PCI_PASSTHROUGH"

                create_vm_flavor(vdud)

                # Find the attached interfaces.
                for fpath in rift.vcs.core.list_by_type(vm, rift.vcs.Fastpath):
                    for port in fpath.port_map:
                        if isinstance(port, rift.vcs.External):
                            iface = rwvnfd.VirtualNetworkFunctionDescriptor_Vdus_ExternalInterfaces()
                            iface.name = port.logical_name
                            create_virtual_interface(iface)
                            try:
                                iface.connection_point = self._port_network.find_group(port).name
                            except AttributeError:
                                logger.error("Could not find port %s group", port)
                                raise

                            vdud.external_interfaces.append(iface)


                            # Connection point has the format <vnfd_id>_<vdu_id>_<group>_<num>
                            cp_prefix = "_".join([str(vnfd.vnfd_id), str(vdud.id), iface.connection_point])
                            cp_count = len([cp for cp in vnfd.connection_points
                                            if cp.id.startswith(cp_prefix)])

                            cp = rwvnfd.VirtualNetworkFunctionDescriptor_ConnectionPoints()
                            cp.id = "_".join([cp_prefix, str(cp_count + 1)])
                            vnfd.connection_points.append(cp)
                        else:
                            iface = rwvnfd.VirtualNetworkFunctionDescriptor_Vdus_InternalInterfaces()
                            iface.name = port.logical_name
                            create_virtual_interface(iface)
                            try:
                                iface.internal_connection_point = self._port_network.find_group(port).name
                            except AttributeError:
                                logger.error("Could not find port %s group", port)
                                raise

                            vdud.internal_interfaces.append(iface)

                            # Connection point has the format <vnfd_id>_<vdu_id>_<group>_<num>
                            cp_prefix = "_".join([str(vnfd.vnfd_id), str(vdud.id), iface.internal_connection_point])
                            cp_count = len([cp.id for cp in vnfd.internal_connection_points
                                            if cp.id.startswith(cp_prefix)])

                            cp = rwvnfd.VirtualNetworkFunctionDescriptor_InternalConnectionPoints()
                            cp.id = "_".join([cp_prefix, str(cp_count + 1)])
                            vnfd.internal_connection_points.append(cp)

        with open(path, 'w') as fp:
            fp.write(vnfds.to_xml_v2(model))

        logger.info('Wrote VNFD XML file with %d VNFDs: %s', len(vnfds.vnfd), path)

        return vnfds.vnfd

    def _generate_nsd(self, vnfds, path):
        """Write a nsd to the specified path. """

        model = gi.repository.RwYang.Model.create_libncx()
        model.load_schema_ypbc(rwnsd.get_schema())

        nsd = rwnsd.NetworkServiceDescriptor()
        colony = self._sysinfo.find_by_class(rift.vcs.Colony)
        if colony is None:
            logger.error("Colony not found, could not generate NSD")
            return

        if not isinstance(colony, rift.vcs.NSD):
            ns = rift.vcs.NSD(
                    collection=colony,
                    name=colony.name,
                    )
        else:
            ns = colony

        nsd.name = ns.nsd_name
        nsd.nsd_id = ns.id
        nsd.provider = ns.provider
        nsd.version = ns.version

        # Add all vnf id's to the nsd
        for vnfd in vnfds:
            nsd.constituent_vnfs.add().vnf_id = vnfd.vnfd_id

        # Create a mapping of link name (port_group) to a list
        # of all VNFD Connection points connected to that link
        link_points = collections.defaultdict(list)
        for vnfd in vnfds:
            for conn_point in vnfd.connection_points:
                link_name = conn_point.id.split("_")[2]
                link_points[link_name].append(conn_point.id)

        # Add a virtual link descriptor for each link name and
        # add all the vnf connections points to it.
        vld_id_gen = itertools.count(1)
        for link_name in link_points:
            vld = nsd.vlds.add()
            vld.id = next(vld_id_gen)
            vld.type_yang = "ELAN"
            for vnf_cp_id in link_points[link_name]:
                vld.connection_points.add().id = vnf_cp_id

        with open(path, 'w') as fp:
            fp.write(nsd.to_xml_v2(model))

        logger.info('Wrote NSD XML file %s: %s', path, nsd)

    def _prepare_vms(self):
        # If we're in collapsed mode, we only need to prepare a single VM.
        ip_list = self.ip_list if not self.sysinfo.collapsed else ["127.0.0.1"]

        # If the user requested to skip preparing the VM or starting the system
        if not self._skip_prepare_vm:
            utils = rift.auto.vm_utils.VmPreparer(ip_list, delete_ip_netns=not self._keep_netns)
            utils.prepare_vms()

    def _apply_config(self, confd_host, xml_file):
        """ Configure the system with desired configuration

        Arguments:
            xml_file - XML configuration file to be applied to the system
            confd_host - The confd netconf host address
        """

        proxy = rift.auto.proxy.NetconfProxy(confd_host)
        proxy.connect()
        rift.vcs.mgmt.wait_until_system_started(proxy)

        for child in xml.etree.ElementTree.parse(xml_file).getroot():
            proxy.merge_config(xml.etree.ElementTree.tostring(child))

    def start(self, xml_output=None, only_gen_xml=None, apply_xml_file=None):
        """Start the system

        This launches the system defined by the instances SystemInfo object.

        This function never returns.
        """

        if xml_output is None:
            xml_output = self._xml_output

        self._generate_manifest()

        # if the top level component is set not to start then config readiness of all the
        # sub-components is set to false.
        for component in self.manifest:
            for descendant in component.descendents():
                descendant.config_ready = descendant.config_ready and component.start

        self.vcs.manifest_generate_xml(self.manifest, xml_output)

        # vnfd_path = os.path.join(
        #         os.path.dirname(xml_output),
        #         "vnfd_" + os.path.basename(xml_output),
        #         )
        # nsd_path = os.path.join(
        #         os.path.dirname(xml_output),
        #         "nsd_" + os.path.basename(xml_output),
        #         )

        # vnfds = self._generate_vnfd(vnfd_path)
        # self._generate_nsd(vnfds, nsd_path)

        # Export his here until we have a real way to send vnfd's to the
        # vnfmgr.  Using an environment variable instead of cmdargs or other
        # variable passing mechanisms in the manifest in order to keep the
        # whole vnfd hack out of manifest generation.
        # os.environ['RIFT_VNFD_XML_PATH'] = vnfd_path
        # os.environ['RIFT_NSD_XML_PATH'] = nsd_path

        if only_gen_xml is None:
            only_gen_xml = self._only_gen_xml

        if only_gen_xml:
            logger.info("User requested exit after generating manifest.")
            sys.exit(0)

        self._prepare_vms()

        # apply config xml to the system in a child process
        if apply_xml_file is not None:
            if os.fork() == 0:
                confd_vm = self.sysinfo.find_ancestor_by_class(rift.vcs.uAgentTasklet)
                if confd_vm is None:
                    raise ConfigurationError("Could not find virtual machine which contains confd")
                self._apply_config(confd_vm.ip, apply_xml_file)
                sys.exit(0)
        self.vcs.exec_rwmain(xml_output, use_gdb=self._use_gdb, track_memory=self._track_memory)


def prepared_system_from_demo_and_args(demo, args,
                                       netconf_trace_override=False,
                                       northbound_listing="rwbase_schema_listing.txt"):
    """ Creates a PreparedSystem from a Demo instance and DemoArgParser namespace.

    Arguments:
        demo - A instance of Demo
        args - A argparse namespace created from DemoArgParser.parse_args()
    """
    netconf_trace = NetconfTrace.AUTO
    if netconf_trace_override:
        netconf_trace = NetconfTrace.ENABLE

    if args.use_xml_mode:
        agent_mode = "RWXML"
    else:
        agent_mode = rift.vcs.mgmt.default_agent_mode()

    sysinfo, port_network = demo.prepare_system(
            mode=args.mode,
            collapsed=args.collapsed,
            dtsperfmgr=args.dtsperfmgr,
            mock_cli=args.mock_cli,
            multi_broker=args.multi_broker,
            multi_dtsrouter=args.multi_dtsrouter,
            ip_list=args.ip_list,
            mgmt_ip_list=args.mgmt_ip_list,
            valgrind=args.valgrind,
            verbose=args.verbose,
            use_gdb=args.gdb,
            track_memory=args.track_memory,
            northbound_listing=northbound_listing,
            netconf_trace=netconf_trace,
            agent_mode=agent_mode,
            persist_dir_name=args.persist_dir_name,
            test_name=args.test_name,
            rift_var_root=args.rift_var_root,
            no_heartbeat=args.no_heartbeat,
    )

    prepared_system_params = {"sysinfo": sysinfo,
                              "port_network": port_network}

    print("args=", args)

    if args.trace is not None:
        prepared_system_params["trace_level"] = args.trace
    if args.xml_output is not None:
        prepared_system_params["xml_output"] = os.path.abspath(args.xml_output.name)
    if args.only_gen_xml is not None:
        prepared_system_params["only_gen_xml"] = args.only_gen_xml
    if args.no_huge is not None:
        prepared_system_params["no_huge"] = args.no_huge
    if args.keep_netns is not None:
        prepared_system_params["keep_netns"] = args.keep_netns
    if args.skip_prepare_vm is not None:
        prepared_system_params["skip_prepare_vm"] = args.skip_prepare_vm
    if args.gdb is not None:
        prepared_system_params["use_gdb"] = args.gdb
    if args.track_memory is not None:
        prepared_system_params["track_memory"] = args.track_memory
    if args.resources_file is not None:
        prepared_system_params["resources_file"] = args.resources_file
    if args.bootstrap_log_period is not None:
        prepared_system_params["bootstrap_log_period"] = args.bootstrap_log_period
    if args.bootstrap_log_severity is not None:
        prepared_system_params["bootstrap_log_severity"] = args.bootstrap_log_severity
    if args.console_log_severity is not None:
        prepared_system_params["console_log_severity"] = args.console_log_severity

    # Validate the Ha mode
    if args.ha_mode is not None:
        if args.ha_mode not in ["LS", "LSS"]:
            print ("ERROR: Incorrect ha-mode provided : {}".format(args.ha_mode))
            sys.exit(1)

    # Validate the IP list for HA mode
    ip_map = {"LS": 2, "LSS": 3} # Value represents number if ip addresses

    if args.ha_mode:
        if len(args.mgmt_ip_list) < ip_map[args.ha_mode]:
            print ("ERROR: {0} Mgmt ip addresses must be given for {1} HA mode.".format(ip_map[args.ha_mode], args.ha_mode))
            sys.exit(1)

    # Collapsed mode should not be used for HA
    if args.ha_mode and args.collapsed:
        print ("ERROR: Collapsed mode should be turned of for HA.")
        sys.exit(1)
        
    system = PreparedSystem(**prepared_system_params)

    return system


class DemoArgParser(argparse.ArgumentParser):
    """
    This class subclasses ArgumentParser and adds various arguments that are
    generic across all Demo scripts.
    """

    def __init__(self, *args, **kwargs):
        """ Creates a argparse parser with default demo arguments.

        Arguments:
           *args, **kwargs:  Any positional and/or keyword arguments
               that is accepted in argparse.ArgumentParser constructor.
        """

        super(DemoArgParser, self).__init__(*args, **kwargs)

        self.add_argument('-c', '--collapsed',
                          action='store_true',
                          help="A flag indicating that the system should be run in a "
                               "collapsed mode (the opposite of the --expanded flag). "
                               "This is the default mode.")

        self.add_argument('-e', '--expanded',
                          dest='collapsed',
                          action='store_false',
                          help="A flag indicating that the system should be run in "
                               "expanded mode (the opposite of the --collapsed flag).")

        self.add_argument('--gdb',
                          action='store_true',
                          help="A flag indicating that rwmain should be launched under gdb")

        self.add_argument('--track-memory',
                          action='store_true',
                          help="A flag indicating that rwmain should be launched with LD_PRELOAD based memory-tracking")

        self.add_argument('--apply-xml',
                        help='Apply a pre-generated XML configuration file to the system',
                        type=argparse.FileType(mode='r'))

        self.add_argument('--ip-list',
                          type=DemoArgParser.parse_ip_list,
                          help="Specify a list of IP addresses to use. The addresses may "
                               "be a comma or space delimited list, or follow the more  "
                               "concise notation defined ip_utils. Alternatively, the "
                               "addresses may be acquired from the reservation system "
                               "if the user has a pool of VMs reserved. To do this, "
                               "simply pass the keyword 'auto' to this argument.")

        self.add_argument('--mgmt-ip-list',
                          type=DemoArgParser.parse_mgmt_ip_list,
                          help="Specify a list of IP addresses used for management domain."
                               "This be a comma or space delimited list, or follow the more "
                               "concise notation defined ip_utils.")

        self.add_argument('--keep-netns',
                          action='store_true',
                          help="Do not clean up the existing network namespaces when "
                               "Preparing the VM's for running the system.")

        self.add_argument('-m', '--mode',
                          choices=['ethsim', 'rawsocket', 'pci', 'custom'],
                          default='ethsim',
                          help="Defines the mode that the system is lauched in. "
                               "Acceptable values are: ethsim, rawsocket, pci, and custom (default: 'ethsim').")

        self.add_argument('--no-huge',
                          action='store_true',
                          help="Add --no-huge to the fastpath cmdargs")

        self.add_argument('--only-gen-xml',
                          action='store_true',
                          help="Instead of launching the system, exit after writing the manifest XML")

        self.add_argument('-o', "--xml-output",
                          type=argparse.FileType('w'),
                          help="Output the manifest file to a particular file path.")

        self.add_argument('--single-broker',
                          dest='multi_broker',
                          action='store_false',
                          help="Enable a per-VM message broker")

        self.add_argument('--with-dts-perf',
                          dest='dtsperfmgr',
                          action='store_true',
                          help="Enable DTS Performance Manager")

        self.add_argument('--mock-cli',
                          action='store_true',
                          help="Replace the real Cli with non-functional Mock")

        self.add_argument('--single-dtsrouter',
                          dest='multi_dtsrouter',
                          action='store_false',
                          help="Enable a per-VM dtsrouter")

        self.add_argument("--skip-prepare-vm",
                          action='store_true',
                          help="Skip preparing the VM's")

        self.add_argument("--resources-file",
                          type=argparse.FileType('r'),
                          help="Resources YAML file path")

        self.add_argument("--rift-var-root",
                          action='store',
                          help="Prefix for RIFT root path")

        self.add_argument('-t','--trace',
                          type=DemoArgParser.parse_trace_level,
                          help="Set the system trace level (0-9)",
                          default=5)

        self.add_argument('--boot-log-period',
                          type=int,
                          dest='bootstrap_log_period',
                          help="Bootstrap logging period")

        self.add_argument('--boot-log-sev',
                          type=int,
                          dest='bootstrap_log_severity',
                          help="Bootstrap Local syslog log severity")

        self.add_argument('--console-log-sev',
                          type=int,
                          dest='console_log_severity',
                          help="Console log severity")

        self.add_argument('--valgrind',
                          nargs='+',
                          help="Runs the specified components under valgrind")

        self.add_argument('--use-xml-mode',
                          action='store_true',
                          default=False,
                          help="Use the xml agent.")

        self.add_argument('-v', '--verbose',
                          action='store_true',
                          help="Logging is normally set to an INFO level. When this flag "
                               "is used logging is set to DEBUG. ")

        self.add_argument('--persist-dir-name',
                          dest='persist_dir_name',
                          default='persist.riftware',
                          help="The Configuration persistence directory name")

        self.add_argument('--test-name',
                          type=str,
                          dest='test_name',
                          help="Test Case Name")

        self.add_argument('--no-heartbeat',
                          action='store_true',
                          default=False,
                          help="Disable heartbeat for debugging")
                          
        self.add_argument('--ha-mode',
                          dest='ha_mode',
                          help="High availability mode for the system")

                          
                          

    def parse_args(self, argv):
        args = super().parse_args(argv)
        args.track_memory = True
        return args

    @staticmethod
    def parse_mgmt_ip_list(mgmt_ip_list):
        """Returns a sanitized list of IP addresses

        Arguments:
            mgmt_ip_list - this argument is string describing a list of management IP addresses

        Returns:
            A list of IP addresses.

        """
        mgmt_ip_list = rift.auto.ip_utils.ip_list_from_string(mgmt_ip_list)

        if len(mgmt_ip_list) not in range(4):
            raise argparse.ArgumentTypeError("Management IP list can have maximum 3 entries")

        return mgmt_ip_list

    @staticmethod
    def parse_ip_list(ip_list):
        """Returns a sanitized list of IP addresses

        Arguments:
            ip_list - this argument is string describing a list of IP addresses
                      or  the keyword 'auto', which automatically retrieves a
                      set of IP addresses from the reservation system (NB: the
                      user needs to have reserved a set of IP addresses with the
                      reservation system; this function only queries for VMs
                      that are available for the user).

        Returns:
            A list of IP addresses.

        """
        if ip_list == "auto":
            try:
                ip_list = ndl.get_vms()
            except:
                raise ReservationError()
        elif ip_list:
            ip_list = rift.auto.ip_utils.ip_list_from_string(ip_list)

        return ip_list

    @staticmethod
    def parse_trace_level(value):
        try:
            value = int(value)
            if value not in range(10):
                raise ValueError()
        except ValueError:
            raise argparse.ArgumentTypeError("Trace level must be an integer between 0 and 9")

        return value

# vim: sw=4
