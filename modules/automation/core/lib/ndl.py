"""
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

@file ndl.py
@author Jeremy Mordkoff (Jeremy.Mordkoff@riftio.com)
@date 4/20/14
@brief These classes and functional implement the testbed functions and interface to the reservation system

"""

import itertools
import logging
from netifaces import interfaces, ifaddresses, AF_INET
import os
import requests
import socket
import sys
import urllib

try:
  import urllib.request
except ImportError:
  import urllib2
  urllib.request = urllib2


logger = logging.getLogger(__name__)
if '--debug' in sys.argv:
    logger.setLevel(logging.DEBUG)

'''
Major classes

  Host -- represents a real host (e.g. grunt) or a VM. includes a collection of Ports
  Switch -- represents a network switch
  Connection -- reprsents a cable going from a Host to a Wwitch -- links to both
  Testbed -- represents the collection of the above

'''

class Host(object):
    ''' represents a single host, virtual or real
    '''
    def __init__(self, tb, **kwargs):
        ''' create a Host
        '''
        self.VMS = []
        self.connections = []
        self._tb = tb
        self.compute_host = None
        self.ipaddress = None
        self.controller_host = None
        self.compute_host = None
        self.storage = None

        if 'name' in kwargs:
            self.name = kwargs['name']
            logger.debug("created host %s with no data" % self.name)
            return
        if 'json' in kwargs:
            self._from_json(kwargs['json'])
            return
        logger.error("created host with no name or data")

    def _from_json(self, data):
        ''' initialize the Testbed and create all of the dependent objects

        args:
            data: dictionary of parameters, e.g.
                {u'available': True,
                u'openstackCompute': None,
                u'name': u'rwopenstack_grunt2_vm4',
                u'MbRam': 8192,
                u'cpus': 4,
                u'storage': 80,
                u'comments': None,
                u'owner': None,
                u'flavor': u'rw.msg',
                u'openstackController': u'grunt2',
                u'ipaddress': u'10.0.2.6',
                u'id': 510}
                u'connections': [
                   {u'switch': u'f10-grunt00', u'speed': 10000, u'port': u'0000.00.05.0'},
                ]
        '''
        # logger.debug("creating host from json \"%s\"" % data)
        self._data = data
        self.name = data['name']
        self.cpus = data['cpus']
        self.ram = data['MbRam']
        self.owner = data['owner']
        self.flavor = data['flavor']
        self.ipaddress = data['ipaddress']
        self.storage = data.get('storage', None)
        self.hugepages = data.get('hugepages', 'False') == 'True'
        compute_host_name = data['openstackCompute']
        if compute_host_name is not None:
            self.compute_host = self._tb.find_host(compute_host_name)
            self.compute_host.add_vm(self)
        controller_host_name = data['openstackController']
        if controller_host_name is not None:
            self.controller_host = self._tb.find_host(controller_host_name)

    def __unicode__(self):
        return "host %s" % self.name

    @property
    def controller_name(self):
        if self.controller_host is None:
            return ''
        else:
            return self.controller_host.name

    def add_vm(self, vm):
        ''' note that a vm is hosted on this host

        Args:
            vm: the vm being hosted
        '''
        self.VMS.append(vm)

    @property
    def isa_vm(self):
        ''' is this Host a vm
        '''
        return self.compute_host != None

    @property
    def has_vms(self):
        ''' does this system host any vms
        '''
        return len(self.VMS) > 0


    def get_connections_by_net(self, net):
        switch = self._tb.find_switch(net)
        return self.get_connections_by_switch(switch)

    def get_connections_by_switch(self, switch):
        return [ conn for conn in self.connections if conn.switch == switch ]

    def get_connections(self, otherhost=None):
        ''' get connections on this host

        Args:
            otherhost: Host object default None -- if specified, only return a list
            of ports on this host that face the other host

        if otherhost is none, returns all ports on this host
        '''
        if otherhost is None:
            return self.connections
        return [ c for c in self.connections if c.get_switch() is not None and len(c.get_switch().get_connections(otherhost))]

    def get_connection(self, name):
        for c in self.connections:
            if c.name == name:
                return c
        return None

    def get_switch(self, connection):
        c = self.get_connection(connection)
        if c is None:
            raise Exception("host %s has no connection %s" % (self.name, connection.name))
        return p.get_switch()

    def get_link_speed(self, otherhost):
        '''
        get the total throughput of all links that face otherhost
        '''
        me = sum([ l.speed for l in self.get_connections(otherhost) ])
        him = sum([ l.speed for l in otherhost.get_connections(self) ])
        logger.debug("get_link_speed: me %d him %d" % (me, him))
        if him < me:
            return him
        return me

    def add_connection(self, connection):
        if connection in self.connections:
            logger.error("connection already exists to switch")
        else:
            self.connections.append(connection)


    def reserve(self, user=None):
        self._tb.reserve([self.name], user)

    def release(self, user=None):
        self._tb.release([self.name], user)

    def mark_doa(self, user=None):
        self._tb.mark_doa(self.name, user)



class Switch(object):
    def __init__(self, name):
        ''' create a new switch
        args:
            name: unique name for the switch
            data: connection data, e.g.
                {u'switch': u'f10-grunt00', u'speed': 10000, u'port': u'0000.00.05.0'},
        '''
        self.name = name
        self.connections = []

    def add_connection(self, connection):
        if connection in self.connections:
            logger.error("connection already exists to switch")
        else:
            self.connections.append(connection)

    def get_connections(self, host):
        '''
        get connections that are attached to a port on particular host
        '''
        return [ c for c in self.connections if c.host == host ]



class Connection(object):
    '''
    represents the link from a host to a switch
    '''

    def __init__(self, host, switch, data):
        ''' create a connection
        args:
            host: a Host object
            switch: a Switch object
            data: a dictionary with all of the parameters
                {u'switch': u'f10-grunt00', u'speed': 10000, u'port': u'0000.00.05.0'},
        '''
        self.host = host
        self.switch = switch
        self.port = data['port']
        self.speed = data['speed']
        switch.add_connection(self)
        host.add_connection(self)

    def get_switch(self):
        return self.switch


#############################################################
#
# Testbed
#
#############################################################

class Testbed(object):
    '''
    It contains all the data about the current testbed
    '''

    def __init__(self, **kwargs):
        '''
        Constructor
        open and parse xml file
        '''
        self.reserve = False
        if 'reserve' in kwargs and kwargs['reserve']:
            self.reserve = True
        self.hosts = dict()
        self.switches = dict()
        self.self_managed = False
        self.in_use = set()
        self._released = []
        #self.RESERVATION_SERVER = os.getenv('RESERVATION_SERVER', 'http://reservation.blr.riftio.com')
        #This is for dts-branch only. if seen in main line please revert to
        self.RESERVATION_SERVER = os.getenv('RESERVATION_SERVER', 'http://reservation.eng.riftio.com:80')

        if 'json' in kwargs:
            self.load(kwargs['json'])
        else:
            url = kwargs.get('url', 'auto')
            if url == 'auto':
                url = self.RESERVATION_SERVER + '/testlab/REST/GetTestbed/'
            if kwargs.get('user', '') != '':
                url = url + kwargs['user'] + "/"
                self.self_managed = True
            self.load_url(url)
        self.vms = [ host.name for host in self.hosts.values() if host.isa_vm ]
        logger.info(self.__str__())

    def __str__(self):
        return "Testbed containing %d hosts and %d switches" % (len(self.hosts), len(self.switches))

    @property
    def has_content(self):
        return len(self.hosts) > 0

    def load_url(self, url):
        if self.reserve:
            logger.info("creating Testbed via url %s" % url)
            response = requests.get(url)
            if response.status_code != 200:
                raise Exception("error %d retrieving testbed defintion from %s" % (response.status_code, url))
            try:
                data = response.json()
            except:
                raise Exception("error parsing data: " + response.json())

            self.load(data)

    def load(self, data):
        for d in data:
            self.new_host(d)

    def reserve(self, suts, user=None):
        ''' reserve resources

            resources can be managed locally (self_managed) or by the reservation server

            ARGS
                suts: list of strings naming the suts to be reserved
                user: (optional) username under which to reserve the resources

            raises
                urllib.request.HTTPError on comms error
                KeyError if you attempt to reserve the same resource twice
        '''
        if self.self_managed:
            for sut in suts:
                if sut in self.in_use:
                    raise KeyError("attempt to reserve resource that is already in use: %s" % sut)
                else:
                    self.in_use.add(sut)
        else:
            systems = ",".join(suts)
            self._NDL_API('reserve', {'resources': systems, }, user)
        logger.debug("reserved %s" % ",".join(suts))


    def _release(self):
        failed_release = []
        for user, systems in self._released:
            try:
                self._NDL_API('release', {'resources': systems, }, user)
                logger.debug("released %s" % systems)
            except Exception as e:
                failed_release.append((user, systems))
        self._released = failed_release


    def release(self, suts, user=None):
        ''' release resources that were previously reserved with reserve()

            ARGS
                suts: list of strings naming the suts to be released
                user: (optional) username under which the resources were reserved (default is current user)

            raises
                KeyError if the resource was not reserved

            catches most errors amnd retries the release
        '''

        if self.self_managed:
            for sut in suts:
                self.in_use.remove(sut)
        else:
            systems = ",".join(suts)
            ''' each item in _released is a comma separated list of systems
            '''
            self._released.append((user, systems))
            self._release()


    def merge(self, other):
        ''' merge all of the components in the 'other' testbed into this one

        @TYPE switch: Switch
        '''
        for host in other.hosts.values():
            if not host.name in self.hosts:
                self.hosts[host.name] = host
        ''' in all likelihood all of the switches in the other testbed already
            exist in this one, so we need to copy the connectors
            instead

            if we bring over the switch as is, then we can continue to use
            the connection objects that refer to it, but if we already have a
            switch with that name, then we need to create new connection objects
        '''
        for switch in other.switches.values():
            if not switch.name in self.switches:
                self.switches[switch.name] = switch
            else:
                sw = self.find_switch(switch.name)
                for connection in switch.connections:
                    Connection(connection.host, sw, { 'port': connection.port, 'speed': connection.speed })




    def new_host(self, host_data):
        host = Host(self, json=host_data)
        self.hosts[host_data['name']] = host
        for connection_data in host_data['connections']:
            switch = self.find_switch(connection_data['switch'])
            # connection wiill register itself with the Host and the Switch
            Connection(host, switch, connection_data)



        '''
        rwopenstack_grunt2_vm4 is on                 None
        managed by               grunt2
        has   4 cpus and   8192 Mb RAM and connections to
        {u'available': True,
        u'openstackCompute': None,
        u'name': u'rwopenstack_grunt2_vm4',
        u'MbRam': 8192,
        u'cpus': 4,
        u'comments': None,
        u'owner': None,
        u'flavor': u'rw.msg',
        u'openstackController': u'grunt2',
        u'ipaddress': u'10.0.2.6',
        u'id': 510}
        u'connections': [
                {u'switch': u'f10-grunt00', u'speed': 10000, u'port': u'0000.00.05.0'},
        ]

        '''



    def find_port(self, portname):
        ''' find a port by name

        ARGS:
            portname: format host.port.portname, e.g. grunt1.port.p785p1
        '''
        for host in self.hosts.itervalues():
            for port in host.ports:
                if port.name == portname:
                    return port
        logger.error("cannot find port %s" % portname)
        return None


    def find_host(self, name):
        ''' find a host by name

        ARGS:
            name: name of the host that you wish to find. By convention all host names are UNQUALIFIED
        '''
        if not name in self.hosts:
            host = Host(self, name=name)
            self.hosts[name] = host
        return self.hosts[name]

    def find_switch(self, name):
        ''' find a switch by name

        ARGS:
            name: switch name
        '''
        if isinstance(name, dict):
            name = name['name']
        if not name in self.switches:
            switch = Switch(name)
            self.switches[name] = switch
        return self.switches[name]

    def select_one_sut(self, criteria, inuse, selected):
        ''' find one system in the process of trying to select a set of systems to meet the needs of a test

        ARGS:
            criteria: a dictionary describing the needs of this test
            inuse: a set of resources that have already been allocated
            selected: a list of resources that have already been selected by
                previous invocations of this tool

        This is called by select_suts and really does not have any other uses
        '''
        if len(criteria) == 0:
            return selected
        todo = criteria[0]
        left = criteria[1:]
        for hostname in self.hosts:
            if hostname in inuse:
                continue
            h = self.find_host(hostname)
            acceptable = True
            for attr, value in todo.items():
                if attr == 'name':
                    continue
                elif h.get_link_speed(self.find_host(selected[attr])) < value:
                    acceptable = False
                    break
            if acceptable:
                selected[todo['name']] = hostname
                newlist = inuse.copy()
                newlist.add(hostname)
                res = self.select_one_sut(left, newlist, selected)
                if res is not None:
                    return res


    def select_suts(self, criteria, all_avail=False):
        '''  select SUTs from the testbed file that meet the criteria

        ARGS:
            criteria: dictionary that describes the needs of this test

        This function retrieves a list of available VMs and then tries to find
        a subset of them that meet the needs as described by the criteria

        '''

        logger.debug("select suts with in_use set to %s" % ",".join(self.in_use))
        inuse = self.in_use
        availableHosts = dict()
        if all_avail:
            for hostname in self.hosts:
                availableHosts[hostname] = True
        else:
            avail = self._NDL_API('available', {})
            for hostname in avail.split(","):
                availableHosts[hostname] = True
        for hostname in self.hosts.keys():
            if not hostname in availableHosts:
                inuse.add(hostname)
                logger.debug("%s is in use" % hostname)
        res = self.select_one_sut(criteria, inuse, dict())
        if res is None:
            logger.error('unable to find a set of SUTs that meet criteria')
            return None
        logger.debug("Criteria Met:")
        for sut, host in res.items():
            logger.debug("    using %s for %s" % (host, sut))
        return res


    def _acceptable_sut(self, test_cfg, nets, vms):
        ''' verify if nets and vms meets the needs of cfg
        @ARGS
            test_config a TestConfiguration object
            nets -- a dictionary mapping the nets in the test config to switches
            vms -- a dictionary mapping the test vms in the test_config to actual VMs

        @TYPE actual_vm: Host
        @TYPE conn: Connection
        '''
        for test_vm in test_cfg.vms:
            conns_used = []
            actual_vm = self.find_host(vms[test_vm['name']])
            if test_vm.get('hugepages', 'False') == 'True' and not actual_vm.hugepages:
                logger.debug('cannot use %s for %s because of hugepages' % ( actual_vm.name, test_vm['name'] ))
                return False
            if test_vm.get('memory', 0) > actual_vm.ram:
                logger.debug('cannot use %s for %s because of memory: need %d found %d' % ( actual_vm.name, test_vm['name'], test_vm.get('memory', 0), actual_vm.ram  ))
                return False
            if test_vm.get('cpus', 0) > actual_vm.cpus:
                logger.debug('cannot use %s for %s because of vcpusm: need %d found %d' % ( actual_vm.name, test_vm['name'], test_vm.get('cpus', 0), actual_vm.cpus  ))
                return False
            if test_vm.get('storage', 0) > actual_vm.storage:
                logger.debug('cannot use %s for %s because of storage: need %d found %d' % ( actual_vm.name, test_vm['name'], test_vm.get('storage', 0), actual_vm.storage  ))
                return False
            for net_needed, speed in test_vm.items():
                if net_needed in test_cfg.networks:
                    logger.debug("%s needs a %d mbps connection to %s" % ( test_vm['name'], speed, net_needed))
                    actual_net = self.find_switch(nets[net_needed])
                    while speed > 0:
                        conns = [ c for c in actual_vm.get_connections_by_switch(actual_net) if not c in conns_used ]
                        if len(conns) == 0:
                            logger.debug("vm %s cannot use %s for %s: not enough bandwidth %d Mbps short" % ( test_vm['name'], actual_net.name, net_needed, speed))
                            return False
                        conns_used.append(conns[0])
                        speed -= conns[0].speed
                        logger.debug("vm %s is connected to %s at %d" % ( test_vm['name'], actual_net.name, conns[0].speed))
        return True

    def sorted_vm_names(self, min_mbRam, min_vcpus, min_storage, controller):
        '''
            return the names of all VMs that are available in order from the least
            resource intensive to the most
        '''
        vms = [vm for vm in self.hosts.values() if vm.isa_vm
                                                    and vm.name not in self.in_use
                                                    and vm.ram >= min_mbRam
                                                    and vm.cpus  >= min_vcpus
                                                    and vm.storage >= min_storage
                                                    and vm.controller_name == controller]
        vms.sort(key=lambda vm: vm.cpus * 1000000 + vm.ram)
        return [ vm.name for vm in vms ]

    def networks_by_hosts(self, hostnames):
        nets = set()
        for hostname in hostnames:
            host = self.find_host(hostname)
            for conn in host.connections:
                nets.add(conn.switch.name)
        return nets

    def select_by_config(self, test_config):
        ''' select the networks and VMs to be used for a test run

        test_config is a testbed config object (TestConfiguration)

        return is a tuple of dictionaries. The first maps the (logical) network names from the config
        file to actual network names in this testbed
        (note that a single actual network can be used to carry traffic for multiple logical networks)
        and the second is a dictionary that maps the VMs in the config file to the VMs in this testbed

        actually selecting the interfaces to use is left as an exercise for the user

        basic logic
            partition the set of actual VMs by test controller as a test config can never span controllers
            for each partition,
                eliminate any VMs that do not have the minimum required CPUs or RAM
                generate a set of nets that these VMs connect to
                assign an actual network to each logical network. There are MxN combinations, in most cases 3x2 = 6
                so this is not that painful
                attempt to find a set of VMs that meet the requirements given these networks

        '''
        if not test_config.vms:
            test_config._config['target_vm'] = 'VM'
            return {}, {'VM': socket.gethostname()}

        min_ram = min([vm.get('memory',0) for vm in test_config.vms])
        logger.debug("min ram is %d" % min_ram )
        min_cpus = min([vm.get('cpus', 1) for vm in test_config.vms])
        logger.debug("min vcpus is %d" % min_cpus )
        min_storage = min([vm.get('storage', 0) for vm in test_config.vms])
        logger.debug("min storage is %d" % min_storage )

        test_vms = [ vm['name'] for vm in test_config.vms]

        controllers = set(host.controller_name for host in self.hosts.values())
        logger.debug("found %d controllers" % len(controllers))

        count = 0
        for controller in controllers:
            if controller == '':
                continue
            possible_hosts = self.sorted_vm_names(min_ram, min_cpus, min_storage, controller)

            if len(possible_hosts) < len(test_vms):
                logger.debug("not enough hosts managed by controller %s. Need %d, found %d" % ( controller, len(test_vms), len(possible_hosts)))
                continue
            possible_nets = self.networks_by_hosts(possible_hosts)
            logger.debug("controller %s has %d hosts and %d nets" % ( controller, len(possible_hosts), len(possible_nets)))
            for vm_mapping in itertools.permutations(possible_hosts, len(test_config.vms)):
                vm_dict = dict(zip(test_vms, vm_mapping))
                for net_mapping in itertools.combinations_with_replacement(possible_nets, len(test_config.networks)):
                    net_dict = dict(zip(test_config.networks, net_mapping))
                    if self._acceptable_sut(test_config, net_dict, vm_dict):
                        logger.debug("test is using %s for nets and %s for vms" % (net_dict.values(), vm_dict.values()))
                        return net_dict, vm_dict
                    count += 1
                    if count % 100000 == 0:
                        logger.debug("tried %d permutations" % count)
        logger.debug("unable to find acceptable resource configuration for test after %d tries" % count)
        return None, None

    def set_server(self, url):
        ''' set the address of the reservation server

        used for testing. Override the default reservation server
        '''
        if url[-1] == "/":
            url = url[0:-1]
        self.RESERVATION_SERVER = url
        logger.info("RESERVATION SERVER URL set to \"%s\"" % self.RESERVATION_SERVER)


    def _NDL_API(self, api, args, user=None):
        """ Private function used by the following functions for maintainability

        Args:
            api: string that is appended to the base URL
            args: a dictionary that will be url-encoded and posted
            user: optional user name. It will be extracted from the environment if not supplied

        Returns:
            the HTTP response body

        Raises:
            urllib.request:HTTPError if the request fails

        TODO:
            should raise KeyError if you attempt to reserve the same resource twice

        """

        if user is None:
            user = os.environ['USER']
        args['username'] = user
        req = urllib.request.Request(self.RESERVATION_SERVER + "/testlab/API/" + api)
        if sys.version_info >= (3,3):
            req.data=urllib.parse.urlencode(args).encode('UTF-8')
        else:
            req.data=urllib.urlencode(args).encode('UTF-8')

        logger.debug("ndl::_NDL_API::request %s" % req.get_full_url() )
        try:
            response = urllib.request.urlopen(req)
        except urllib.request.HTTPError as e:
            logger.error("Error %d from reservation server: %s" % (e.getcode(), e.readline()))
            raise
        if response.getcode() != 200:
            raise urllib.resuest.HTTPError
        return response.read().decode('UTF-8')



    def release_all(self, user=None):
        """ Release all resources reserved

        This will release everythinh you have reserved, regardless of system

        see NDL_API for other details
        """

        self._NDL_API('releaseall', {}, user)


    def get_vms(self, user=None, count=None):
        """ Get the Management IP Addresses for all VMs reserved

        Args:
            count is an option count of the number of VMs required. If it is not specified, then
            the IPs for all VMs currently reserved will be returned. If it is specified, then the system
            will try to make sure that you have at least that many reserved that are all part of the same
            openstack instance

        Returns
            a list of IP addresses (as strings)

        see NDL_API for other details
        """
        crit = dict()
        if count is not None:
            crit['count'] = count
        s = self._NDL_API('getvms', crit, user)
        if len(s) == 0:
            return []
        ips = s.split(',')
        # if the localhost's IP is in the list, move it to the front
        localips = getmyips()
        for i in range(len(ips)):
            if ips[i] in localips:
                x = ips[i]
                del ips[i]
                return [ x, ] + ips
        # otherwise order does not matter?
        return ips

    def add_resource(self, name, controller, ipaddress, ram, cpus, storage, owner=None, flavor='', compute=None, huge_pages=False):
        """ Add a resource to the reservation system

        Only used by cloud tool to register a new VM as it creates them

        Args:
            name resource name e.g. rwopenstack_grunt2_vm1
            controller hostname e.g. grunt2
            ipaddress - management IP address
            ram - memory in MB
            cpus -- count
            owner -- if specified, the resource will be immediately reserved

        see NDL_API for other details
        """
        if compute is None: compute = controller
        args = { 'vm': name,
                    'controller': controller,
                    'ipaddress': ipaddress,
                    'ram': ram,
                    'cpus': cpus,
                    'storage': storage,
                    'flavor': flavor,
                    'compute': compute,
                    'hugepages': huge_pages,
                    }
        if owner is not None:
            args['owner'] = owner
        self._NDL_API('addresource', args, None)

    def remove_resource(self, name):
        """ Remove a resource from the system

        used by cloudtool when it destroys a VM

        see NDL_API for other details
        """
        self._NDL_API('removeresource', { 'vm': name, }, None)

    def mark_doa(self, name, user=None):
        """ Mark a resource as broken. Used by the harness when
            fix_this_vm fails
            see NDL_API for other details
        """
        self._NDL_API('markdoa', { 'vm': name, }, user )

    def get_controllers(self):
        """ Get a list of openstack controllers

         Returns
             list of controller names

        see NDL_API for other details
        """
        s = self._NDL_API('getcontrollers', {})
        return s.split(",")


# \cond INTERNAL
def getmyips():
    ''' retrieve the ip addresses assigned to this device

    '''
    res = []
    for ifaceName in interfaces():
        adds = ifaddresses(ifaceName)
        if AF_INET in adds:
            for add in adds[AF_INET]:
                res.append(add['addr'])
    return res
# \endcond INTERNAL

def get_vms(count=None, user=None):
    testbed = Testbed()
    return testbed.get_vms(count=count, user=user)










# \cond UNITTEST
if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s', level=logging.DEBUG)

    tb = Testbed(url='auto')

    before = tb.get_vms()
    '''
    This is an example for how the test harness could select which systems to run a test suite on
    It is up to the harness to maintain the list of systems that are currently in use

    It might be easier in the long run to keep the testbed config and the system status in a database.

    TODO: track switch usage
    '''

    crit = [ { 'name': 'SUT1', }, { 'name': 'SUT2', 'SUT1': 100 } ]
    suts = tb.select_suts(crit)
    if suts is None:
        print("selection failed")
        exit(1)
    else:
        for sut, dut in suts.items():
            print("using %s for %s" % (dut, sut))
        tb.reserve(suts.values())
        tb.release(suts.values())



    '''
    This is an example as to how a test suite could find and interrogate SUTs
    suts is a list that would be handed in by the test harness
    '''
    g1 = tb.find_host(suts['SUT1'])
    g2 = tb.find_host(suts['SUT2'])

    connections = g1.get_connections(g2)
    print("g1 has %d connections pointing towards g2: %s" % (len(connections), ','.join([p.port for p in connections])))

    connections = g2.get_connections(g1)
    print("g2 has %d connections pointing towards g1: %s" % (len(connections), ','.join([p.port for p in connections])))

    print("link speed is %d Mbps" % g1.get_link_speed(g2))

    avail = 0
    reserved = 0
    for hostname, host in tb.hosts.items():
        try:
            host.reserve()
        except Exception as e:
            print("ERROR reserving %s: %s" % (hostname, e))
            reserved += 1
        else:
            host.release()
            avail += 1
    print("%d resources reserved and released, %d were already reserved" % (avail, reserved))

    systems = tb.get_vms(count=7)
    print("my %d systems are  %s" % (len(systems), ",".join(systems)))
    tb.release_all()

    after=tb.get_vms()
    print(before)
    print(after)
    if len(before) == len(after):
        print("verified %d resources before and after" % len(before))
    else:
        print("ERROR: resources chnaged from %d to %d" % ( len(before), len(after)))


# \endcond


