"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file port.py
@author Joshua Downer (joshua.downer@riftio.com)
@author Austin Cormier (austin.cormier@riftio.com)
@date 09/30/2014
@brief classes related to VM port information and the connections
between ports.

"""
import logging
import re

logger = logging.getLogger(__name__)


class Port(object):
    """
    This class represents a port mapping on a virtual machine.
    """

    def __init__(self,
            logical_name=None,
            port_name=None,
            port_type=False):
        """Creates a Port object.

        Arguments:
            logical_name - The logical_name name of the port
            port_name    - If provided, this is expected to be a PortName object
            port_type    - A string indicating the type of port this represents

        """
        self.logical_name = logical_name
        self.port_type = port_type
        self.port_name = port_name

    def __repr__(self):
        """Returns a representation of the port"""
        return repr({
            "logical_name": self.logical_name,
            "port_type": self.port_type,
            "port_name": self.port_name})

    def __str__(self):
        """A string representation of the port mapping"""
        return "{}|{}|{}".format(self.logical_name, self.port_name, self.port_type)

    def __eq__(self, other):
        return (self.logical_name == other.logical_name and
                self.port_name == other.port_name and
                self.port_type == other.port_type)

    def __hash__(self):
        return hash(
                (self.logical_name, self.port_name, self.port_type)
                )

    def __ne__(self, other):
        return not self.__eq__(other)

    @property
    def port_name(self):
        """The real or physical name of the port (None if undefined)"""
        return None if self._port_name is None else str(self._port_name)

    @port_name.setter
    def port_name(self, port_name):
        """Sets the value of the real name of the port

        Arguments:
            port_name - a PortName defining the real name of the port

        """
        try:
            self._port_name = PortName.from_string(port_name)
        except (ValueError, TypeError):
            self._port_name = port_name


class Fabric(Port):
    """
    This is class specializes the Port class for fabric ports and is merely a
    convenience.
    """

    def __init__(self, logical_name=None, port_name=None):
        """Creates a Fabric object

        Arguments:
            logical_name   - the logical_name name of the port
            port_name - the real port name

        """
        super(Fabric, self).__init__(
                logical_name=logical_name,
                port_name=port_name,
                port_type="vfap")


class External(Port):
    """
    This is class specializes the Port class for external ports and is merely a
    convenience.
    """

    def __init__(self, logical_name=None, port_name=None):
        """Creates a External object

        Arguments:
            logical_name   - the logical_name name of the port
            port_name - the real port name

        """
        super(External, self).__init__(
                logical_name=logical_name,
                port_name=port_name,
                port_type="external")


class PortName(object):
    """
    This class provides an abtraction of a port mappings real name. A port
    mapping has 3 parts, but the logical name and the port type are immutable,
    whereas the real-name of the port can change based upon the mode of the
    sytem.
    """

    basenames = {
            'pci': 'eth_uio:pci={}',
            'ethsim': 'eth_sim:name={}',
            'rawsocket': 'eth_raw:name={}'}

    patterns = [
            ('pci', re.compile('eth_uio:pci=(.*)')),
            ('ethsim', re.compile('eth_sim:name=(.*)')),
            ('rawsocket', re.compile('eth_raw:name=(.*)'))]

    def __init__(self, mode, name):
        """Create a PortNam object

        Arguments:
            mode - the system  mode
            name - the name of the port

        """
        # If the mode is not able be extracted from the name (pattern matched),
        # check if it is in the list of recognized modes
        for curr_mode, pattern in PortName.patterns:
            match = pattern.match(name)
            if match is not None:
                self.mode = curr_mode
                self.name = match.group(1)
                break
        else:
            if mode in PortName.basenames:
                self.mode = mode
                self.name = name
            else:
                raise ValueError('Unrecognized mode')

    def __str__(self):
        """A string representation of the port name"""
        return PortName.basenames[self.mode].format(self.name)

    @classmethod
    def from_string(cls, text):
        """Convert string into PortName

        Arguments:
            text - the string to convert

        Raises:
            A ValueError is raised if the string cannot be converted into a
            PortName.

        Returns:
            A PortName object.

        """
        # Helper function to creation PortName
        def match_template(mode, pattern):
            name = pattern.match(text).group(1)
            return cls(mode, name)

        # Iterate through patterns until a match is found
        for mode, pattern in PortName.patterns:
            try:
                return match_template(mode, pattern)
            except AttributeError:
                pass

        raise ValueError('Unable to convert to PortName')


class PortInfo(object):
    """
    This class represents information retrieved about a port that is available
    on a virtual machine.
    """

    def __init__(self, name, group, external=False):
        """Creates a PortInfo object

        Arguments:
            name     - The name of the port.
            group    - A string indicating a group that the port belongs to.
                       Ports in the same group on the same virtual machine are
                       assumed to be interchangeable. This is probably the
                       switch ID.
            external - A flag indicating whether the port is external (True) or
                       fabric (False)

        """
        self._name = name
        self._group = group
        self._external = external

    @property
    def name(self):
        """The name of the port"""
        return self._name

    @property
    def group(self):
        """The group the port is associated with"""
        return self._group

    @property
    def external(self):
        """A flag indicating whether the port is external or fabric"""
        return self._external


class PortGroup(object):
    """
    This class provides a grouping of Port objects. Ports
    belonging to the same Port Group have Layer 2 connectivity (connected via
    a switch) and thus any interfaces assigned to Ports in the same Port Groups
    are able to communicate.
    """
    def __init__(self, name, port_list=None):
        """ Creates an named PortGroup instance.

        Arguments:
            name - The name of the Port Group.
            port_list - A list of Port objects that have Layer 2
                        connectivity.
        """
        self._name = name
        self._ports = set()

        if port_list is not None:
            for port in port_list:
                self.add_port(port)

    @property
    def name(self):
        """Name of the Port Group"""
        return self._name

    def __repr__(self):
        """Returns a representation of the Port Group"""
        return "PortGroup({}, {})".format(self.name, repr(self._ports))

    def __iter__(self):
        """Returns and iterator over the list of ports"""
        return iter(self._ports)

    def __len__(self):
        """Returns the number of ports in the group"""
        return len(self._ports)

    def get_ports(self):
        """Returns a copy of the list of all ports"""
        return list(self._ports)

    def add_port(self, port):
        """Adds a new Port to the current Port Group.

        Arguments:
            port - A Port instance to add.

        Raises:
            TypeError: The port parameter is not a Port instance.
        """
        if not isinstance(port, Port):
            raise TypeError("The port parameter is expected to be a Port object")

        self._ports.add(port)

    def find_by_logical_name(self, name):
        return [port for port in self if port.logical_name == name]

    def find_by_type(self, port_type):
        return [port for port in self if type(port) == port_type]

    def __contains__(self, find_port):
        """Returns True if find_port is in the Port Group, False otherwise.

        Arguments:
            find_port - The ravcs.core.Port to look for.
        """
        for port in self:
            if port == find_port:
                return True

        return False


class PortNetwork(object):
    """This class describes a complete set of Port Groups for a running Riftware instance."""
    def __init__(self, port_groups=None):
        """Creates an instance of PortNetwork

        Arguments:
            port_groups - A list of port_groups to initialize the PortNetwork with.
        """
        self._port_groups = set()

        if port_groups is not None:
            for group in port_groups:
                self.add_port_group(group)

    def __repr__(self):
        """Returns a representation of the PortNetwork"""
        return "PortNetwork({})".format([repr(group) for group in self.port_groups])

    def __iter__(self):
        """Returns an iterator over the port groups"""
        return iter(self._port_groups)

    def __len__(self):
        """Returns the number of port groups in the network"""
        return len(self._port_groups)

    @property
    def port_groups(self):
        """The current list of PortGroups within the PortNetwork"""
        return list(self._port_groups)

    @property
    def ports(self):
        """A list of all the ports in the network"""
        return [port for group in self for port in group]

    def add_port_group(self, new_port_group):
        """Adds a PortGroup to the current PortNetwork.

        Arguments:
            new_port_group - The PortGroup to add to the PortNetwork.

        Raises:
            TypeError: If new_port_group is not a PortGroup instance.
        """
        if not isinstance(new_port_group, PortGroup):
            raise TypeError("new_port_group argument must be a PortGroup object.")

        self._port_groups.add(new_port_group)

    def find_group(self, port):
        """Returns the PortGroup within the PortNetwork that contains the Port.

        Arguments:
            port - The Port object to find the PortGroup for.

        Returns:
            A PortGroup instance that contains the Port if found, None otherwise.
        """
        for group in self._port_groups:
            if port in group:
                return group

        return None

    def ports_in_same_group(self, port_list):
        """ Returns whether all Ports are in the same PortGroup.

        Arguments:
            port_list - A list of Port objects.

        Returns:
            True if all Ports are in the same PortGroup, False otherwise.
        """
        groups = {self.find_group(port) for port in port_list}

        return len(groups) == 1 and groups.pop() is not None


def create_port_from_logical_name(name):
    """Create a instance of Port of using a logical name.

    Arguments:
        name - A logical port name that begins with either "fab" or "ext".

    Returns:
        A Fabric Port instance if name begins with "fab" or an External Port
        instance if name begins with "ext".

    Raises:
        ValueError - If name does not begin with a known prefix.
    """
    port = None
    if name.startswith('fab'):
        port = Fabric(logical_name=name)
    elif name.startswith('ext'):
        port = External(logical_name=name)
    else:
        raise ValueError('Unrecognized port type')

    return port

