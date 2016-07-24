#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import json
import logging

from keystoneclient import v3 as ksclientv3
from keystoneclient.v2_0 import client as ksclientv2
from novaclient.v2 import client as nova_client
from neutronclient.neutron import client as ntclient
from glanceclient.v2 import client as glclient
from ceilometerclient import client as ceilo_client

# Exceptions
import novaclient.exceptions as NovaException
import keystoneclient.exceptions as KeystoneExceptions
import neutronclient.common.exceptions as NeutronException
import glanceclient.exc as GlanceException

logger = logging.getLogger('rwcal.openstack.drv')
logger.setLevel(logging.DEBUG)

class ValidationError(Exception):
    pass


class KeystoneDriver(object):
    """
    Driver base-class for keystoneclient APIs
    """
    def __init__(self, ksclient):
        """
        Constructor for KeystoneDriver base class
        Arguments: None
        Returns: None
        """
        self.ksclient = ksclient

    def get_username(self):
        """
        Returns the username associated with keystoneclient connection
        """
        return self._username

    def get_password(self):
        """
        Returns the password associated with keystoneclient connection
        """
        return self._password

    def get_tenant_name(self):
        """
        Returns the tenant name associated with keystoneclient connection
        """
        return self._tenant_name

    def _get_keystone_connection(self):
        """
        Returns object of class python-keystoneclient class
        """
        if not hasattr(self, '_keystone_connection'):
            self._keystone_connection = self.ksclient(**self._get_keystone_credentials())
        return self._keystone_connection

    def is_auth_token_valid(self, token_expiry, time_fmt):
        """
        Performs validity on auth_token
        Arguments:
          token_expiry (string): Expiry time for token
          time_fmt (string)    : Format for expiry string in auth_ref

        Returns:
        True/False (Boolean):  (auth_token is valid or auth_token is invalid)
        """
        import time
        import datetime
        import dateutil.parser
        try:
            now = datetime.datetime.timetuple(datetime.datetime.utcnow())
            expires_at = dateutil.parser.parse(token_expiry)
            t_now = time.mktime(now)
            t_expiry = time.mktime(expires_at.timetuple())

            if (t_expiry <= t_now) or ((t_expiry - t_now) < 300 ):
                ### Token has expired or about to expire (5 minute)
                delattr(self, '_keystone_connection')
                return False
            else:
                return True
        except Exception as e:
            logger.error("Received except %s during auth_token validity check" %str(e))
            logger.info("Can not validate the auth_token. Assuming invalid")
            return False
        

    def get_service_endpoint(self, service_type, endpoint_type):
        """
        Returns requested type of endpoint for requested service type
        Arguments:
          service_type (string): Service Type (e.g. computev3, image, network)
          endpoint_type(string): Endpoint Type (e.g. publicURL,adminURL,internalURL)
        Returns:
          service_endpoint(string): Service endpoint string
        """
        endpoint_kwargs   = {'service_type'  : service_type,
                             'endpoint_type' : endpoint_type}
        try:
            ksconn = self._get_keystone_connection()
            service_endpoint  = ksconn.service_catalog.url_for(**endpoint_kwargs)
        except Exception as e:
            logger.error("OpenstackDriver: Service Catalog discovery operation failed for service_type: %s, endpoint_type: %s. Exception: %s" %(service_type, endpoint_type, str(e)))
            raise
        return service_endpoint


    def get_raw_token(self):
        """
        Returns a valid raw_auth_token string

        Returns (string): raw_auth_token string
        """
        ksconn = self._get_keystone_connection()
        try:
            raw_token = ksconn.get_raw_token_from_identity_service(auth_url = self._auth_url,
                                                                   token    = self.get_auth_token())
        except KeystoneExceptions.AuthorizationFailure as e:
            logger.error("OpenstackDriver: get_raw_token_from_identity_service Failure. Exception: %s" %(str(e)))
            return None

        except Exception as e:
            logger.error("OpenstackDriver: Could not retrieve raw_token. Exception: %s" %(str(e)))

        return raw_token

    def get_tenant_id(self):
        """
        Returns tenant_id for the project/tenant. Tenant name is provided during
        class instantiation

        Returns (string): Tenant ID
        """
        ksconn = self._get_keystone_connection()
        return ksconn.tenant_id

    def get_security_mode(self):
        """
        Returns certificate_validation policy in case of SSL/TLS connection.
        This policy is provided during class instantiation

        Returns (boolean):
        The boolean returned are designed to match the python-client class instantiation ("insecure") value.
        for nova/neutron/glance/keystone clients

        True: No certificate validation required -- Insecure mode
        False: Certificate validation required -- Secure mode
        """
        return self._insecure
    
    def tenant_list(self):
        """
        Returns list of tenants
        """
        pass

    def tenant_create(self, name):
        """
        Create a new tenant
        """
        pass

    def tenant_delete(self, tenant_id):
        """
        Deletes a tenant identified by tenant_id
        """
        pass

    def roles_list(self):
        pass

    def roles_create(self):
        pass

    def roles_delete(self):
        pass

class KeystoneDriverV2(KeystoneDriver):
    """
    Driver class for keystoneclient V2 APIs
    """
    def __init__(self, username, password, auth_url,tenant_name, insecure):
        """
        Constructor for KeystoneDriverV3 class
        Arguments:
        username (string)  : Username
        password (string)  : Password
        auth_url (string)  : Authentication URL
        tenant_name(string): Tenant Name

        Returns: None
        """
        self._username    = username
        self._password    = password
        self._auth_url    = auth_url
        self._tenant_name = tenant_name
        self._insecure    = insecure
        super(KeystoneDriverV2, self).__init__(ksclientv2.Client)

    def _get_keystone_credentials(self):
        """
        Returns the dictionary of kwargs required to instantiate python-keystoneclient class
        """
        creds                 = {}
        #creds['user_domain'] = self._domain_name
        creds['username']     = self._username
        creds['password']     = self._password
        creds['auth_url']     = self._auth_url
        creds['tenant_name']  = self._tenant_name
        creds['insecure']     = self.get_security_mode()
        return creds

    def get_auth_token(self):
        """
        Returns a valid auth_token

        Returns (string): auth_token string
        """
        ksconn = self._get_keystone_connection()
        return ksconn.auth_token

    def is_auth_token_valid(self):
        """
        Performs validity on auth_token
        Arguments:

        Returns:
        True/False (Boolean):  (auth_token is valid or auth_token is invalid)
        """
        ksconn = self._get_keystone_connection()
        result = super(KeystoneDriverV2, self).is_auth_token_valid(ksconn.auth_ref['token']['expires'],
                                                                   "%Y-%m-%dT%H:%M:%SZ")
        return result


class KeystoneDriverV3(KeystoneDriver):
    """
    Driver class for keystoneclient V3 APIs
    """
    def __init__(self, username, password, auth_url,tenant_name, insecure):
        """
        Constructor for KeystoneDriverV3 class
        Arguments:
        username (string)  : Username
        password (string)  : Password
        auth_url (string)  : Authentication URL
        tenant_name(string): Tenant Name

        Returns: None
        """
        self._username    = username
        self._password    = password
        self._auth_url    = auth_url
        self._tenant_name = tenant_name
        self._insecure    = insecure
        super(KeystoneDriverV3, self).__init__(ksclientv3.Client)

    def _get_keystone_credentials(self):
        """
        Returns the dictionary of kwargs required to instantiate python-keystoneclient class
        """
        creds                 = {}
        #creds['user_domain'] = self._domain_name
        creds['username']     = self._username
        creds['password']     = self._password
        creds['auth_url']     = self._auth_url
        creds['project_name'] = self._tenant_name
        creds['insecure']     = self._insecure
        return creds

    def get_auth_token(self):
        """
        Returns a valid auth_token

        Returns (string): auth_token string
        """
        ksconn = self._get_keystone_connection()
        return ksconn.auth_ref['auth_token']

    def is_auth_token_valid(self):
        """
        Performs validity on auth_token
        Arguments:

        Returns:
        True/False (Boolean):  (auth_token is valid or auth_token is invalid)
        """
        ksconn = self._get_keystone_connection()
        result = super(KeystoneDriverV3, self).is_auth_token_valid(ksconn.auth_ref['expires_at'],
                                                                   "%Y-%m-%dT%H:%M:%S.%fZ")
        return result

class NovaDriver(object):
    """
    Driver for openstack nova_client
    """
    def __init__(self, ks_drv, service_name, version):
        """
        Constructor for NovaDriver
        Arguments: KeystoneDriver class object
        """
        self.ks_drv = ks_drv
        self._service_name = service_name
        self._version = version

    def _get_nova_credentials(self):
        """
        Returns a dictionary of kwargs required to instantiate python-novaclient class
        """
        creds               = {}
        creds['version']    = self._version
        creds['bypass_url'] = self.ks_drv.get_service_endpoint(self._service_name, "publicURL")
        creds['username']   = self.ks_drv.get_username()
        creds['project_id'] = self.ks_drv.get_tenant_name()
        creds['auth_token'] = self.ks_drv.get_auth_token()
        creds['insecure']   = self.ks_drv.get_security_mode()
        return creds

    def _get_nova_connection(self):
        """
        Returns an object of class python-novaclient
        """
        if not hasattr(self, '_nova_connection'):
            self._nova_connection = nova_client.Client(**self._get_nova_credentials())
        else:
            # Reinitialize if auth_token is no longer valid
            if not self.ks_drv.is_auth_token_valid():
                self._nova_connection = nova_client.Client(**self._get_nova_credentials())
        return self._nova_connection

    def _flavor_get(self, flavor_id):
        """
        Get flavor by flavor_id
        Arguments:
           flavor_id(string): UUID of flavor_id

        Returns:
        dictionary of flavor parameters
        """
        nvconn = self._get_nova_connection()
        try:
            flavor = nvconn.flavors.get(flavor_id)
        except Exception as e:
            logger.info("OpenstackDriver: Did not find flavor with flavor_id : %s. Exception: %s"%(flavor_id, str(e)))
            raise

        try:
            extra_specs = flavor.get_keys()
        except Exception as e:
            logger.info("OpenstackDriver: Could not get the EPA attributes for flavor with flavor_id : %s. Exception: %s"%(flavor_id, str(e)))
            raise

        response = flavor.to_dict()
        assert 'extra_specs' not in response, "Key extra_specs present as flavor attribute"
        response['extra_specs'] = extra_specs
        return response

    def flavor_get(self, flavor_id):
        """
        Get flavor by flavor_id
        Arguments:
           flavor_id(string): UUID of flavor_id

        Returns:
        dictionary of flavor parameters
        """
        return self._flavor_get(flavor_id)

    def flavor_list(self):
        """
        Returns list of all flavors (dictionary per flavor)

        Arguments:
           None
        Returns:
           A list of dictionaries. Each dictionary contains attributes for a single flavor instance
        """
        flavors = []
        flavor_info = []
        nvconn =  self._get_nova_connection()
        try:
            flavors = nvconn.flavors.list()
        except Exception as e:
            logger.error("OpenstackDriver: List Flavor operation failed. Exception: %s"%(str(e)))
            raise
        if flavors:
            flavor_info = [ self.flavor_get(flv.id) for flv in flavors ]
        return flavor_info

    def flavor_create(self, name, ram, vcpu, disk, extra_specs):
        """
        Create a new flavor

        Arguments:
           name   (string):  Name of the new flavor
           ram    (int)   :  Memory in MB
           vcpus  (int)   :  Number of VCPUs
           disk   (int)   :  Secondary storage size in GB
           extra_specs (dictionary): EPA attributes dictionary

        Returns:
           flavor_id (string): UUID of flavor created
        """
        nvconn =  self._get_nova_connection()
        try:
            flavor = nvconn.flavors.create(name        = name,
                                           ram         = ram,
                                           vcpus       = vcpu,
                                           disk        = disk,
                                           flavorid    = 'auto',
                                           ephemeral   = 0,
                                           swap        = 0,
                                           rxtx_factor = 1.0,
                                           is_public    = True)
        except Exception as e:
            logger.error("OpenstackDriver: Create Flavor operation failed. Exception: %s"%(str(e)))
            raise

        if extra_specs:
            try:
                flavor.set_keys(extra_specs)
            except Exception as e:
                logger.error("OpenstackDriver: Set Key operation failed for flavor: %s. Exception: %s" %(flavor.id, str(e)))
                raise
        return flavor.id

    def flavor_delete(self, flavor_id):
        """
        Deletes a flavor identified by flavor_id

        Arguments:
           flavor_id (string):  UUID of flavor to be deleted

        Returns: None
        """
        assert flavor_id == self._flavor_get(flavor_id)['id']
        nvconn =  self._get_nova_connection()
        try:
            nvconn.flavors.delete(flavor_id)
        except Exception as e:
            logger.error("OpenstackDriver: Delete flavor operation failed for flavor: %s. Exception: %s" %(flavor_id, str(e)))
            raise


    def server_list(self):
        """
        Returns a list of available VMs for the project

        Arguments: None

        Returns:
           A list of dictionaries. Each dictionary contains attributes associated
           with individual VM
        """
        servers     = []
        server_info = []
        nvconn      = self._get_nova_connection()
        try:
            servers     = nvconn.servers.list()
        except Exception as e:
            logger.error("OpenstackDriver: List Server operation failed. Exception: %s" %(str(e)))
            raise
        server_info = [ server.to_dict() for server in servers]
        return server_info

    def _nova_server_get(self, server_id):
        """
        Returns a dictionary of attributes associated with VM identified by service_id

        Arguments:
          server_id (string): UUID of the VM/server for which information is requested

        Returns:
          A dictionary object with attributes associated with VM identified by server_id
        """
        nvconn = self._get_nova_connection()
        try:
            server = nvconn.servers.get(server = server_id)
        except Exception as e:
            logger.info("OpenstackDriver: Get Server operation failed for server_id: %s. Exception: %s" %(server_id, str(e)))
            raise
        else:
            return server.to_dict()

    def server_get(self, server_id):
        """
        Returns a dictionary of attributes associated with VM identified by service_id

        Arguments:
          server_id (string): UUID of the VM/server for which information is requested

        Returns:
          A dictionary object with attributes associated with VM identified by server_id
        """
        return self._nova_server_get(server_id)

    def server_create(self, **kwargs):
        """
        Creates a new VM/server instance

        Arguments:
          A dictionary of following key-value pairs
         {
           server_name(string)        : Name of the VM/Server
           flavor_id  (string)        : UUID of the flavor to be used for VM
           image_id   (string)        : UUID of the image to be used VM/Server instance
           network_list(List)         : A List of network_ids. A port will be created in these networks
           port_list (List)           : A List of port-ids. These ports will be added to VM.
           metadata   (dict)          : A dictionary of arbitrary key-value pairs associated with VM/server
           userdata   (string)        : A script which shall be executed during first boot of the VM
           availability_zone (string) : A name of the availability zone where instance should be launched
           scheduler_hints (string)   : Openstack scheduler_hints to be passed to nova scheduler 
         }
        Returns:
          server_id (string): UUID of the VM/server created

        """
        nics = []
        if 'network_list' in kwargs:
            for network_id in kwargs['network_list']:
                nics.append({'net-id': network_id})

        if 'port_list' in kwargs:
            for port_id in kwargs['port_list']:
                nics.append({'port-id': port_id})

        nvconn = self._get_nova_connection()

        try:
            server = nvconn.servers.create(kwargs['name'],
                                           kwargs['image_id'],
                                           kwargs['flavor_id'],
                                           meta                 = kwargs['metadata'],
                                           files                = None,
                                           reservation_id       = None,
                                           min_count            = None,
                                           max_count            = None,
                                           userdata             = kwargs['userdata'],
                                           security_groups      = kwargs['security_groups'],
                                           availability_zone    = kwargs['availability_zone'],
                                           block_device_mapping = None,
                                           nics                 = nics,
                                           scheduler_hints      = kwargs['scheduler_hints'],
                                           config_drive         = None)
        except Exception as e:
            logger.info("OpenstackDriver: Create Server operation failed. Exception: %s" %(str(e)))
            raise
        return server.to_dict()['id']

    def server_delete(self, server_id):
        """
        Deletes a server identified by server_id

        Arguments:
           server_id (string): UUID of the server to be deleted

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.delete(server_id)
        except Exception as e:
            logger.error("OpenstackDriver: Delete server operation failed for server_id: %s. Exception: %s" %(server_id, str(e)))
            raise

    def server_start(self, server_id):
        """
        Starts a server identified by server_id

        Arguments:
           server_id (string): UUID of the server to be started

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.start(server_id)
        except Exception as e:
            logger.error("OpenstackDriver: Start Server operation failed for server_id : %s. Exception: %s" %(server_id, str(e)))
            raise

    def server_stop(self, server_id):
        """
        Arguments:
           server_id (string): UUID of the server to be stopped

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.stop(server_id)
        except Exception as e:
            logger.error("OpenstackDriver: Stop Server operation failed for server_id : %s. Exception: %s" %(server_id, str(e)))
            raise

    def server_pause(self, server_id):
        """
        Arguments:
           server_id (string): UUID of the server to be paused

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.pause(server_id)
        except Exception as e:
            logger.error("OpenstackDriver: Pause Server operation failed for server_id : %s. Exception: %s" %(server_id, str(e)))
            raise

    def server_unpause(self, server_id):
        """
        Arguments:
           server_id (string): UUID of the server to be unpaused

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.unpause(server_id)
        except Exception as e:
            logger.error("OpenstackDriver: Resume Server operation failed for server_id : %s. Exception: %s" %(server_id, str(e)))
            raise


    def server_suspend(self, server_id):
        """
        Arguments:
           server_id (string): UUID of the server to be suspended

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.suspend(server_id)
        except Exception as e:
            logger.error("OpenstackDriver: Suspend Server operation failed for server_id : %s. Exception: %s" %(server_id, str(e)))


    def server_resume(self, server_id):
        """
        Arguments:
           server_id (string): UUID of the server to be resumed

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.resume(server_id)
        except Exception as e:
            logger.error("OpenstackDriver: Resume Server operation failed for server_id : %s. Exception: %s" %(server_id, str(e)))
            raise

    def server_reboot(self, server_id, reboot_type):
        """
        Arguments:
           server_id (string) : UUID of the server to be rebooted
           reboot_type(string):
                         'SOFT': Soft Reboot
                         'HARD': Hard Reboot
        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.reboot(server_id, reboot_type)
        except Exception as e:
            logger.error("OpenstackDriver: Reboot Server operation failed for server_id: %s. Exception: %s" %(server_id, str(e)))
            raise

    def server_rebuild(self, server_id, image_id):
        """
        Arguments:
           server_id (string) : UUID of the server to be rebooted
           image_id (string)  : UUID of the image to use
        Returns: None
        """

        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.rebuild(server_id, image_id)
        except Exception as e:
            logger.error("OpenstackDriver: Rebuild Server operation failed for server_id: %s. Exception: %s" %(server_id, str(e)))
            raise


    def server_add_port(self, server_id, port_id):
        """
        Arguments:
           server_id (string): UUID of the server
           port_id   (string): UUID of the port to be attached

        Returns: None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.interface_attach(server_id,
                                            port_id,
                                            net_id = None,
                                            fixed_ip = None)
        except Exception as e:
            logger.error("OpenstackDriver: Server Port Add operation failed for server_id : %s, port_id : %s. Exception: %s" %(server_id, port_id, str(e)))
            raise

    def server_delete_port(self, server_id, port_id):
        """
        Arguments:
           server_id (string): UUID of the server
           port_id   (string): UUID of the port to be deleted
        Returns: None

        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.interface_detach(server_id, port_id)
        except Exception as e:
            logger.error("OpenstackDriver: Server Port Delete operation failed for server_id : %s, port_id : %s. Exception: %s" %(server_id, port_id, str(e)))
            raise

    def floating_ip_list(self):
        """
        Arguments:
            None
        Returns:
            List of objects of floating IP nova class (novaclient.v2.floating_ips.FloatingIP)
        """
        nvconn =  self._get_nova_connection()
        try:
            ip_list = nvconn.floating_ips.list()
        except Exception as e:
            logger.error("OpenstackDriver: Floating IP List operation failed. Exception: %s" %str(e))
            raise
        
        return ip_list

    def floating_ip_create(self, pool):
        """
        Arguments:
           pool (string): Name of the pool (optional)
        Returns:
           An object of floating IP nova class (novaclient.v2.floating_ips.FloatingIP)
        """
        nvconn =  self._get_nova_connection()
        try:
            floating_ip = nvconn.floating_ips.create(pool)
        except Exception as e:
            logger.error("OpenstackDriver: Floating IP Create operation failed. Exception: %s"  %str(e))
            raise

        return floating_ip

    def floating_ip_delete(self, floating_ip):
        """
        Arguments:
           floating_ip: An object of floating IP nova class (novaclient.v2.floating_ips.FloatingIP)
        Returns:
           None
        """
        nvconn =  self._get_nova_connection()
        try:
            floating_ip = nvconn.floating_ips.delete(floating_ip)
        except Exception as e:
            logger.error("OpenstackDriver: Floating IP Delete operation failed. Exception: %s"  %str(e))
            raise

    def floating_ip_assign(self, server_id, floating_ip, fixed_ip):
        """
        Arguments:
           server_id (string)  : UUID of the server
           floating_ip (string): IP address string for floating-ip
           fixed_ip (string)   : IP address string for the fixed-ip with which floating ip will be associated
        Returns:
           None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.add_floating_ip(server_id, floating_ip, fixed_ip)
        except Exception as e:
            logger.error("OpenstackDriver: Assign Floating IP operation failed. Exception: %s"  %str(e))
            raise

    def floating_ip_release(self, server_id, floating_ip):
        """
        Arguments:
           server_id (string)  : UUID of the server
           floating_ip (string): IP address string for floating-ip
        Returns:
           None
        """
        nvconn =  self._get_nova_connection()
        try:
            nvconn.servers.remove_floating_ip(server_id, floating_ip)
        except Exception as e:
            logger.error("OpenstackDriver: Release Floating IP operation failed. Exception: %s"  %str(e))
            raise

    def group_list(self):
        """
        List of Server Affinity and Anti-Affinity Groups
        
        Arguments:
            None
        Returns:
           List of dictionary objects where dictionary is representation of class (novaclient.v2.server_groups.ServerGroup)
        """
        nvconn =  self._get_nova_connection()
        try:
            group_list = nvconn.server_groups.list()
        except Exception as e:
            logger.error("OpenstackDriver: Server Group List operation failed. Exception: %s"  %str(e))
            raise

        group_info = [ group.to_dict() for group in group_list ]
        return group_info



class NovaDriverV2(NovaDriver):
    """
    Driver class for novaclient V2 APIs
    """
    def __init__(self, ks_drv):
        """
        Constructor for NovaDriver
        Arguments: KeystoneDriver class object
        """
        super(NovaDriverV2, self).__init__(ks_drv, 'compute', '2')

class NovaDriverV21(NovaDriver):
    """
    Driver class for novaclient V2 APIs
    """
    def __init__(self, ks_drv):
        """
        Constructor for NovaDriver
        Arguments: KeystoneDriver class object
        """
        super(NovaDriverV21, self).__init__(ks_drv, 'computev21', '3')

class GlanceDriver(object):
    """
    Driver for openstack glance-client
    """
    def __init__(self, ks_drv, service_name, version):
        """
        Constructor for GlanceDriver
        Arguments: KeystoneDriver class object
        """
        self.ks_drv = ks_drv
        self._service_name = service_name
        self._version = version

    def _get_glance_credentials(self):
        """
        Returns a dictionary of kwargs required to instantiate python-glanceclient class

        Arguments: None

        Returns:
           A dictionary object of arguments
        """
        creds             = {}
        creds['version']  = self._version
        creds['endpoint'] = self.ks_drv.get_service_endpoint(self._service_name, 'publicURL')
        creds['token']    = self.ks_drv.get_auth_token()
        creds['insecure'] = self.ks_drv.get_security_mode()
        return creds

    def _get_glance_connection(self):
        """
        Returns a object of class python-glanceclient
        """
        if not hasattr(self, '_glance_connection'):
            self._glance_connection = glclient.Client(**self._get_glance_credentials())
        else:
            # Reinitialize if auth_token is no longer valid
            if not self.ks_drv.is_auth_token_valid():
                self._glance_connection = glclient.Client(**self._get_glance_credentials())
        return self._glance_connection

    def image_list(self):
        """
        Returns list of dictionaries. Each dictionary contains attributes associated with
        image

        Arguments: None

        Returns: List of dictionaries.
        """
        glconn = self._get_glance_connection()
        images = []
        try:
            image_info = glconn.images.list()
        except Exception as e:
            logger.error("OpenstackDriver: List Image operation failed. Exception: %s" %(str(e)))
            raise
        images = [ img for img in image_info ]
        return images

    def image_create(self, **kwargs):
        """
        Creates an image
        Arguments:
           A dictionary of kwargs with following keys
           {
              'name'(string)         : Name of the image
              'location'(string)     : URL (http://....) where image is located
              'disk_format'(string)  : Disk format
                    Possible values are 'ami', 'ari', 'aki', 'vhd', 'vmdk', 'raw', 'qcow2', 'vdi', 'iso'
              'container_format'(string): Container format
                                       Possible values are 'ami', 'ari', 'aki', 'bare', 'ovf'
              'tags'                 : A list of user tags
           }
        Returns:
           image_id (string)  : UUID of the image

        """
        glconn = self._get_glance_connection()
        try:
            image = glconn.images.create(**kwargs)
        except Exception as e:
            logger.error("OpenstackDriver: Create Image operation failed. Exception: %s" %(str(e)))
            raise

        return image.id

    def image_upload(self, image_id, fd):
        """
        Upload the image

        Arguments:
            image_id: UUID of the image
            fd      : File descriptor for the image file
        Returns: None
        """
        glconn = self._get_glance_connection()
        try:
            glconn.images.upload(image_id, fd)
        except Exception as e:
            logger.error("OpenstackDriver: Image upload operation failed. Exception: %s" %(str(e)))
            raise

    def image_add_location(self, image_id, location, metadata):
        """
        Add image URL location

        Arguments:
           image_id : UUID of the image
           location : http URL for the image

        Returns: None
        """
        glconn = self._get_glance_connection()
        try:
            image = glconn.images.add_location(image_id, location, metadata)
        except Exception as e:
            logger.error("OpenstackDriver: Image location add operation failed. Exception: %s" %(str(e)))
            raise

    def image_update(self):
        pass

    def image_delete(self, image_id):
        """
        Delete an image

        Arguments:
           image_id: UUID of the image

        Returns: None

        """
        assert image_id == self._image_get(image_id)['id']
        glconn = self._get_glance_connection()
        try:
            glconn.images.delete(image_id)
        except Exception as e:
            logger.error("OpenstackDriver: Delete Image operation failed for image_id : %s. Exception: %s" %(image_id, str(e)))
            raise


    def _image_get(self, image_id):
        """
        Returns a dictionary object of VM image attributes

        Arguments:
           image_id (string): UUID of the image

        Returns:
           A dictionary of the image attributes
        """
        glconn = self._get_glance_connection()
        try:
            image = glconn.images.get(image_id)
        except Exception as e:
            logger.info("OpenstackDriver: Get Image operation failed for image_id : %s. Exception: %s" %(image_id, str(e)))
            raise
        return image

    def image_get(self, image_id):
        """
        Returns a dictionary object of VM image attributes

        Arguments:
           image_id (string): UUID of the image

        Returns:
           A dictionary of the image attributes
        """
        return self._image_get(image_id)

class GlanceDriverV2(GlanceDriver):
    """
    Driver for openstack glance-client V2
    """
    def __init__(self, ks_drv):
        super(GlanceDriverV2, self).__init__(ks_drv, 'image', 2)

class NeutronDriver(object):
    """
    Driver for openstack neutron neutron-client
    """
    def __init__(self, ks_drv, service_name, version):
        """
        Constructor for NeutronDriver
        Arguments: KeystoneDriver class object
        """
        self.ks_drv = ks_drv
        self._service_name = service_name
        self._version = version

    def _get_neutron_credentials(self):
        """
        Returns a dictionary of kwargs required to instantiate python-neutronclient class

        Returns:
          Dictionary of kwargs
        """
        creds                 = {}
        creds['api_version']  = self._version
        creds['endpoint_url'] = self.ks_drv.get_service_endpoint(self._service_name, 'publicURL')
        creds['token']        = self.ks_drv.get_auth_token()
        creds['tenant_name']  = self.ks_drv.get_tenant_name()
        creds['insecure']     = self.ks_drv.get_security_mode()
        return creds

    def _get_neutron_connection(self):
        """
        Returns an object of class python-neutronclient
        """
        if not hasattr(self, '_neutron_connection'):
            self._neutron_connection = ntclient.Client(**self._get_neutron_credentials())
        else:
            # Reinitialize if auth_token is no longer valid
            if not self.ks_drv.is_auth_token_valid():
                self._neutron_connection = ntclient.Client(**self._get_neutron_credentials())
        return self._neutron_connection

    def network_list(self):
        """
        Returns list of dictionaries. Each dictionary contains the attributes for a network
        under project

        Arguments: None

        Returns:
          A list of dictionaries
        """
        networks = []
        ntconn   = self._get_neutron_connection()
        try:
            networks = ntconn.list_networks()
        except Exception as e:
            logger.error("OpenstackDriver: List Network operation failed. Exception: %s" %(str(e)))
            raise
        return networks['networks']

    def network_create(self, **kwargs):
        """
        Creates a new network for the project

        Arguments:
          A dictionary with following key-values
        {
          name (string)              : Name of the network
          admin_state_up(Boolean)    : True/False (Defaults: True)
          external_router(Boolean)   : Connectivity with external router. True/False (Defaults: False)
          shared(Boolean)            : Shared among tenants. True/False (Defaults: False)
          physical_network(string)   : The physical network where this network object is implemented (optional).
          network_type               : The type of physical network that maps to this network resource (optional).
                                       Possible values are: 'flat', 'vlan', 'vxlan', 'gre'
          segmentation_id            : An isolated segment on the physical network. The network_type attribute
                                       defines the segmentation model. For example, if the network_type value
                                       is vlan, this ID is a vlan identifier. If the network_type value is gre,
                                       this ID is a gre key.
        }
        """
        params = {'network':
                  {'name'                 : kwargs['name'],
                   'admin_state_up'       : kwargs['admin_state_up'],
                   'tenant_id'            : self.ks_drv.get_tenant_id(),
                   'shared'               : kwargs['shared'],
                   #'port_security_enabled': port_security_enabled,
                   'router:external'      : kwargs['external_router']}}

        if 'physical_network' in kwargs:
            params['network']['provider:physical_network'] = kwargs['physical_network']
        if 'network_type' in kwargs:
            params['network']['provider:network_type'] = kwargs['network_type']
        if 'segmentation_id' in kwargs:
            params['network']['provider:segmentation_id'] = kwargs['segmentation_id']

        ntconn = self._get_neutron_connection()
        try:
            logger.debug("Calling neutron create_network() with params: %s", str(params))
            net = ntconn.create_network(params)
        except Exception as e:
            logger.error("OpenstackDriver: Create Network operation failed. Exception: %s" %(str(e)))
            raise
        logger.debug("Got create_network response from neutron connection: %s", str(net))
        network_id = net['network']['id']
        if not network_id:
            raise Exception("Empty network id returned from create_network. (params: %s)" % str(params))

        return network_id

    def network_delete(self, network_id):
        """
        Deletes a network identified by network_id

        Arguments:
          network_id (string): UUID of the network

        Returns: None
        """
        assert network_id == self._network_get(network_id)['id']
        ntconn = self._get_neutron_connection()
        try:
            ntconn.delete_network(network_id)
        except Exception as e:
            logger.error("OpenstackDriver: Delete Network operation failed. Exception: %s" %(str(e)))
            raise

    def _network_get(self, network_id):
        """
        Returns a dictionary object describing the attributes of the network

        Arguments:
           network_id (string): UUID of the network

        Returns:
           A dictionary object of the network attributes
        """
        ntconn = self._get_neutron_connection()
        network = ntconn.list_networks(id = network_id)['networks']
        if not network:
            raise NeutronException.NotFound("Network with id %s not found"%(network_id))

        return network[0]

    def network_get(self, network_id):
        """
        Returns a dictionary object describing the attributes of the network

        Arguments:
           network_id (string): UUID of the network

        Returns:
           A dictionary object of the network attributes
        """
        return self._network_get(network_id)

    def subnet_create(self, network_id, cidr):
        """
        Creates a subnet on the network

        Arguments:
           network_id(string): UUID of the network where subnet needs to be created
           cidr (string)     : IPv4 address prefix (e.g. '1.1.1.0/24') for the subnet

        Returns:
           subnet_id (string): UUID of the created subnet
        """
        params = {'subnets': [{'cidr': cidr,
                               'ip_version': 4,
                               'network_id': network_id,
                               'gateway_ip': None}]}
        ntconn = self._get_neutron_connection()
        try:
            subnet = ntconn.create_subnet(params)
        except Exception as e:
            logger.error("OpenstackDriver: Create Subnet operation failed. Exception: %s" %(str(e)))
            raise

        return subnet['subnets'][0]['id']

    def subnet_list(self):
        """
        Returns a list of dictionaries. Each dictionary contains attributes describing the subnet

        Arguments: None

        Returns:
           A dictionary of the objects of subnet attributes
        """
        ntconn = self._get_neutron_connection()
        try:
            subnets = ntconn.list_subnets()['subnets']
        except Exception as e:
            logger.error("OpenstackDriver: List Subnet operation failed. Exception: %s" %(str(e)))
            raise
        return subnets

    def _subnet_get(self, subnet_id):
        """
        Returns a dictionary object describing the attributes of a subnet.

        Arguments:
           subnet_id (string): UUID of the subnet

        Returns:
           A dictionary object of the subnet attributes
        """
        ntconn = self._get_neutron_connection()
        subnets = ntconn.list_subnets(id=subnet_id)
        if not subnets['subnets']:
            logger.error("OpenstackDriver: Get subnet operation failed for subnet_id: %s" %(subnet_id)) 
            #raise NeutronException.NotFound("Could not find subnet_id %s" %(subnet_id))
            return None
        else:
            return subnets['subnets'][0]

    def subnet_get(self, subnet_id):
        """
        Returns a dictionary object describing the attributes of a subnet.

        Arguments:
           subnet_id (string): UUID of the subnet

        Returns:
           A dictionary object of the subnet attributes
        """
        return self._subnet_get(subnet_id)

    def subnet_delete(self, subnet_id):
        """
        Deletes a subnet identified by subnet_id

        Arguments:
           subnet_id (string): UUID of the subnet to be deleted

        Returns: None
        """
        ntconn = self._get_neutron_connection()
        assert subnet_id == self._subnet_get(self,subnet_id)
        try:
            ntconn.delete_subnet(subnet_id)
        except Exception as e:
            logger.error("OpenstackDriver: Delete Subnet operation failed for subnet_id : %s. Exception: %s" %(subnet_id, str(e)))
            raise

    def port_list(self, **kwargs):
        """
        Returns a list of dictionaries. Each dictionary contains attributes describing the port

        Arguments:
            kwargs (dictionary): A dictionary for filters for port_list operation

        Returns:
           A dictionary of the objects of port attributes

        """
        ports  = []
        ntconn = self._get_neutron_connection()

        kwargs['tenant_id'] = self.ks_drv.get_tenant_id()

        try:
            ports  = ntconn.list_ports(**kwargs)
        except Exception as e:
            logger.info("OpenstackDriver: List Port operation failed. Exception: %s" %(str(e)))
            raise
        return ports['ports']

    def port_create(self, **kwargs):
        """
        Create a port in network

        Arguments:
           A dictionary of following
           {
              name (string)      : Name of the port
              network_id(string) : UUID of the network_id identifying the network to which port belongs
              subnet_id(string)  : UUID of the subnet_id from which IP-address will be assigned to port
              vnic_type(string)  : Possible values are "normal", "direct", "macvtap"
           }
        Returns:
           port_id (string)   : UUID of the port
        """
        params = {
            "port": {
                "admin_state_up"    : kwargs['admin_state_up'],
                "name"              : kwargs['name'],
                "network_id"        : kwargs['network_id'],
                "fixed_ips"         : [ {"subnet_id": kwargs['subnet_id']}],
                "binding:vnic_type" : kwargs['port_type']}}

        ntconn = self._get_neutron_connection()
        try:
            port  = ntconn.create_port(params)
        except Exception as e:
            logger.error("OpenstackDriver: Port Create operation failed. Exception: %s" %(str(e)))
            raise
        return port['port']['id']

    def _port_get(self, port_id):
        """
        Returns a dictionary object describing the attributes of the port

        Arguments:
           port_id (string): UUID of the port

        Returns:
           A dictionary object of the port attributes
        """
        ntconn = self._get_neutron_connection()
        port   = ntconn.list_ports(id=port_id)['ports']
        if not port:
            raise NeutronException.NotFound("Could not find port_id %s" %(port_id))
        return port[0]

    def port_get(self, port_id):
        """
        Returns a dictionary object describing the attributes of the port

        Arguments:
           port_id (string): UUID of the port

        Returns:
           A dictionary object of the port attributes
        """
        return self._port_get(port_id)

    def port_delete(self, port_id):
        """
        Deletes a port identified by port_id

        Arguments:
           port_id (string) : UUID of the port

        Returns: None
        """
        assert port_id == self._port_get(port_id)['id']
        ntconn = self._get_neutron_connection()
        try:
            ntconn.delete_port(port_id)
        except Exception as e:
            logger.error("Port Delete operation failed for port_id : %s. Exception: %s" %(port_id, str(e)))
            raise

class NeutronDriverV2(NeutronDriver):
    """
    Driver for openstack neutron neutron-client v2
    """
    def __init__(self, ks_drv):
        """
        Constructor for NeutronDriver
        Arguments: KeystoneDriver class object
        """
        super(NeutronDriverV2, self).__init__(ks_drv, 'network', '2.0')



class CeilometerDriver(object):
    """
    Driver for openstack ceilometer client
    """

    def __init__(self, ks_drv, service_name, version):
        """
        Constructor for CeilometerDriver
        Arguments: KeystoneDriver class object
        """
        self.ks_drv = ks_drv
        self._service_name = service_name
        self._version = version
        self._client = None

    @property
    def version(self):
        """The version of the ceilometer client used by the driver"""
        return self._version

    @property
    def client(self):
        """The instance of ceilometer client used by the driver"""
        if self._client is None or not self.ks_drv.is_auth_token_valid():
            self._client = ceilo_client.Client(**self.credentials)

        return self._client

    @property
    def auth_token(self):
        """The authorization token for the ceilometer client"""
        try:
            return self.ks_drv.get_auth_token()
        except KeystoneExceptions.EndpointNotFound as e:
            logger.error("OpenstackDriver: unable to get authorization token for ceilometer. Exception: %s" %(str(e)))
            raise

    @property
    def security_mode(self):
        """The security mode for the ceilometer client"""
        try:
            return self.ks_drv.get_security_mode()
        except KeystoneExceptions.EndpointNotFound as e:
            logger.error("OpenstackDriver: unable to get security mode for ceilometer. Exception: %s" %(str(e)))
            raise

    @property
    def endpoint(self):
        """The service endpoint for the ceilometer client"""
        try:
            return self.ks_drv.get_service_endpoint(self._service_name, "publicURL")
        except KeystoneExceptions.EndpointNotFound as e:
            logger.error("OpenstackDriver: unable to get endpoint for ceilometer. Exception: %s" %(str(e)))
            raise

    @property
    def credentials(self):
        """A dictionary of credentials for the ceilometer client"""
        return dict(
                version=self.version,
                endpoint=self.endpoint,
                token=self.auth_token,
                insecure=self.security_mode,
                )

    @property
    def meters(self):
        """A list of the available meters"""
        try:
            return self.client.meters.list()
        except Exception as e:
            logger.error("OpenstackDriver: List meters operation failed. Exception: %s" %(str(e)))
            raise

    @property
    def alarms(self):
        """The ceilometer client alarms manager"""
        return self.client.alarms

    def query_samples(self, vim_instance_id, counter_name, limit=1):
        """Returns a list of samples

        Arguments:
            vim_instance_id - the ID of the VIM that the samples are from
            counter_name    - the counter that the samples will come from
            limit           - a limit on the number of samples to return
                              (default: 1)

        Returns:
            A list of samples

        """
        try:
            filter = json.dumps({
                "and": [
                    {"=": {"resource": vim_instance_id}},
                    {"=": {"counter_name": counter_name}}
                    ]
                })
            result = self.client.query_samples.query(filter=filter, limit=limit)
            return result[-limit:]

        except Exception as e:
            logger.exception(e)

        return []


class CeilometerDriverV2(CeilometerDriver):
    """
    Driver for openstack ceilometer ceilometer-client
    """
    def __init__(self, ks_drv):
        """
        Constructor for CeilometerDriver
        Arguments: CeilometerDriver class object
        """
        super(CeilometerDriverV2, self).__init__(ks_drv, 'metering', '2')

class OpenstackDriver(object):
    """
    Driver for openstack nova, neutron, glance, keystone, swift, cinder services
    """
    def __init__(self, username, password, auth_url, tenant_name, mgmt_network = None, cert_validate = False):
        """
        OpenstackDriver Driver constructor
        Arguments:
          username (string)                   : Username for project/tenant.
          password (string)                   : Password
          auth_url (string)                   : Keystone Authentication URL.
          tenant_name (string)                : Openstack project name
          mgmt_network(string, optional)      : Management network name. Each VM created with this cloud-account will
                                                have a default interface into management network. 
          cert_validate (boolean, optional)   : In case of SSL/TLS connection if certificate validation is required or not.

        """
        insecure = not cert_validate
        if auth_url.find('/v3') != -1:
            self.ks_drv        = KeystoneDriverV3(username, password, auth_url, tenant_name, insecure)
            self.glance_drv    = GlanceDriverV2(self.ks_drv)
            self.nova_drv      = NovaDriverV21(self.ks_drv)
            self.neutron_drv   = NeutronDriverV2(self.ks_drv)
            self.ceilo_drv     = CeilometerDriverV2(self.ks_drv)
        elif auth_url.find('/v2') != -1:
            self.ks_drv        = KeystoneDriverV2(username, password, auth_url, tenant_name, insecure)
            self.glance_drv    = GlanceDriverV2(self.ks_drv)
            self.nova_drv      = NovaDriverV2(self.ks_drv)
            self.neutron_drv   = NeutronDriverV2(self.ks_drv)
            self.ceilo_drv     = CeilometerDriverV2(self.ks_drv)
        else:
            logger.error("Could not identity the version information for openstack service endpoints. Auth_URL should contain \"/v2\" or \"/v3\" string in it")
            raise NotImplementedError("Auth URL is wrong or invalid. Only Keystone v2 & v3 supported")

        if mgmt_network != None:
            self._mgmt_network = mgmt_network

            networks = []
            try:
                ntconn   = self.neutron_drv._get_neutron_connection()
                networks = ntconn.list_networks()
            except Exception as e:
                logger.error("OpenstackDriver: List Network operation failed. Exception: %s" %(str(e)))
                raise

            network_list = [ network for network in networks['networks'] if network['name'] == mgmt_network ]

            if not network_list:
                raise NeutronException.NotFound("Could not find network %s" %(mgmt_network))
            self._mgmt_network_id = network_list[0]['id']

    def validate_account_creds(self):
        try:
            ksconn = self.ks_drv._get_keystone_connection()
        except KeystoneExceptions.AuthorizationFailure as e:
            logger.error("OpenstackDriver: Unable to authenticate or validate the existing credentials. Exception: %s" %(str(e)))
            raise ValidationError("Invalid Credentials: "+ str(e))
        except Exception as e:
            logger.error("OpenstackDriver: Could not connect to Openstack. Exception: %s" %(str(e)))
            raise ValidationError("Connection Error: "+ str(e))

    def get_mgmt_network_id(self):
        return self._mgmt_network_id

    def glance_image_create(self, **kwargs):
        if not 'disk_format' in kwargs:
            kwargs['disk_format'] = 'qcow2'
        if not 'container_format' in kwargs:
            kwargs['container_format'] = 'bare'
        if not 'min_disk' in kwargs:
            kwargs['min_disk'] = 0
        if not 'min_ram' in kwargs:
            kwargs['min_ram'] = 0
        return self.glance_drv.image_create(**kwargs)

    def glance_image_upload(self, image_id, fd):
        self.glance_drv.image_upload(image_id, fd)

    def glance_image_add_location(self, image_id, location):
        self.glance_drv.image_add_location(image_id, location)

    def glance_image_delete(self, image_id):
        self.glance_drv.image_delete(image_id)

    def glance_image_list(self):
        return self.glance_drv.image_list()

    def glance_image_get(self, image_id):
        return self.glance_drv.image_get(image_id)


    def nova_flavor_list(self):
        return self.nova_drv.flavor_list()

    def nova_flavor_create(self, name, ram, vcpus, disk, epa_specs):
        extra_specs = epa_specs if epa_specs else {}
        return self.nova_drv.flavor_create(name,
                                           ram         = ram,
                                           vcpu        = vcpus,
                                           disk        = disk,
                                           extra_specs = extra_specs)

    def nova_flavor_delete(self, flavor_id):
        self.nova_drv.flavor_delete(flavor_id)

    def nova_flavor_get(self, flavor_id):
        return self.nova_drv.flavor_get(flavor_id)

    def nova_server_create(self, **kwargs):
        assert kwargs['flavor_id'] == self.nova_drv.flavor_get(kwargs['flavor_id'])['id']
        image = self.glance_drv.image_get(kwargs['image_id'])
        if image['status'] != 'active':
            raise GlanceException.NotFound("Image with image_id: %s not found in active state. Current State: %s" %(image['id'], image['status']))
        
        # if 'network_list' in kwargs:
        #     kwargs['network_list'].append(self._mgmt_network_id)
        # else:
        #     kwargs['network_list'] = [self._mgmt_network_id]

        if 'security_groups' not in kwargs:
            nvconn = self.nova_drv._get_nova_connection()
            sec_groups = nvconn.security_groups.list()
            if sec_groups:
                ## Should we add VM in all availability security_groups ???
                kwargs['security_groups'] = [x.name for x in sec_groups]
            else:
                kwargs['security_groups'] = None

        return self.nova_drv.server_create(**kwargs)

    def nova_server_add_port(self, server_id, port_id):
        self.nova_drv.server_add_port(server_id, port_id)

    def nova_server_delete_port(self, server_id, port_id):
        self.nova_drv.server_delete_port(server_id, port_id)

    def nova_server_start(self, server_id):
        self.nova_drv.server_start(server_id)

    def nova_server_stop(self, server_id):
        self.nova_drv.server_stop(server_id)

    def nova_server_delete(self, server_id):
        self.nova_drv.server_delete(server_id)

    def nova_server_reboot(self, server_id):
        self.nova_drv.server_reboot(server_id, reboot_type='HARD')

    def nova_server_rebuild(self, server_id, image_id):
        self.nova_drv.server_rebuild(server_id, image_id)

    def nova_floating_ip_list(self):
        return self.nova_drv.floating_ip_list()

    def nova_floating_ip_create(self, pool = None):
        return self.nova_drv.floating_ip_create(pool)

    def nova_floating_ip_delete(self, floating_ip):
        self.nova_drv.floating_ip_delete(floating_ip)

    def nova_floating_ip_assign(self, server_id, floating_ip, fixed_ip):
        self.nova_drv.floating_ip_assign(server_id, floating_ip, fixed_ip)

    def nova_floating_ip_release(self, server_id, floating_ip):
        self.nova_drv.floating_ip_release(server_id, floating_ip)

    def nova_server_list(self):
        return self.nova_drv.server_list()

    def nova_server_get(self, server_id):
        return self.nova_drv.server_get(server_id)

    def nova_server_group_list(self):
        return self.nova_drv.group_list()
    
    def neutron_network_list(self):
        return self.neutron_drv.network_list()

    def neutron_network_get(self, network_id):
        return self.neutron_drv.network_get(network_id)

    def neutron_network_create(self, **kwargs):
        return self.neutron_drv.network_create(**kwargs)

    def neutron_network_delete(self, network_id):
        self.neutron_drv.network_delete(network_id)

    def neutron_subnet_list(self):
        return self.neutron_drv.subnet_list()

    def neutron_subnet_get(self, subnet_id):
        return self.neutron_drv.subnet_get(subnet_id)

    def neutron_subnet_create(self, network_id, cidr):
        return self.neutron_drv.subnet_create(network_id, cidr)

    def netruon_subnet_delete(self, subnet_id):
        self.neutron_drv.subnet_delete(subnet_id)

    def neutron_port_list(self, **kwargs):
        return self.neutron_drv.port_list(**kwargs)

    def neutron_port_get(self, port_id):
        return self.neutron_drv.port_get(port_id)

    def neutron_port_create(self, **kwargs):
        subnets = [subnet for subnet in self.neutron_drv.subnet_list() if subnet['network_id'] == kwargs['network_id']]
        assert len(subnets) == 1
        kwargs['subnet_id'] = subnets[0]['id']
        if not 'admin_state_up' in kwargs:
            kwargs['admin_state_up'] = True
        port_id =  self.neutron_drv.port_create(**kwargs)

        if 'vm_id' in kwargs:
            self.nova_server_add_port(kwargs['vm_id'], port_id)
        return port_id

    def neutron_port_delete(self, port_id):
        self.neutron_drv.port_delete(port_id)

    def ceilo_meter_endpoint(self):
        return self.ceilo_drv.endpoint

    def ceilo_meter_list(self):
        return self.ceilo_drv.meters

    def ceilo_nfvi_metrics(self, vim_id):
        """Returns a dict of NFVI metrics for a given VM

        Arguments:
            vim_id - the VIM ID of the VM to retrieve the metrics for

        Returns:
            A dict of NFVI metrics

        """
        def query_latest_sample(counter_name):
            try:
                filter = json.dumps({
                    "and": [
                        {"=": {"resource": vim_id}},
                        {"=": {"counter_name": counter_name}}
                        ]
                    })
                orderby = json.dumps([{"timestamp": "DESC"}])
                result = self.ceilo_drv.client.query_samples.query(
                        filter=filter,
                        orderby=orderby,
                        limit=1,
                        )
                return result[0]

            except IndexError:
                pass

            except Exception as e:
                logger.error("Got exception while querying ceilometer, exception details:%s " %str(e))

            return None

        memory_usage = query_latest_sample("memory.usage")
        disk_usage = query_latest_sample("disk.usage")
        cpu_util = query_latest_sample("cpu_util")

        metrics = dict()

        if memory_usage is not None:
            memory_usage.volume = 1e6 * memory_usage.volume
            metrics["memory_usage"] = memory_usage.to_dict()

        if disk_usage is not None:
            metrics["disk_usage"] = disk_usage.to_dict()

        if cpu_util is not None:
            metrics["cpu_util"] = cpu_util.to_dict()

        return metrics

    def ceilo_alarm_list(self):
        """Returns a list of ceilometer alarms"""
        return self.ceilo_drv.client.alarms.list()

    def ceilo_alarm_create(self,
                           name, 
                           meter,
                           statistic,
                           operation,
                           threshold,
                           period,
                           evaluations,
                           severity='low',
                           repeat=True,
                           enabled=True,
                           actions=None,
                           **kwargs):
        """Create a new Alarm

        Arguments:
            name        - the name of the alarm
            meter       - the name of the meter to measure
            statistic   - the type of statistic used to trigger the alarm
                          ('avg', 'min', 'max', 'count', 'sum')
            operation   - the relational operator that, combined with the
                          threshold value, determines  when the alarm is
                          triggered ('lt', 'le', 'eq', 'ge', 'gt')
            threshold   - the value of the statistic that will trigger the
                          alarm
            period      - the duration (seconds) over which to evaluate the
                          specified statistic
            evaluations - the number of samples of the meter statistic to
                          collect when evaluating the threshold
            severity    - a measure of the urgency or importance of the alarm
                          ('low', 'moderate', 'critical')
            repeat      - a flag that indicates whether the alarm should be
                          triggered once (False) or repeatedly while the alarm
                          condition is true (True)
            enabled     - a flag that indicates whether the alarm is enabled
                          (True) or disabled (False)
            actions     - a dict specifying the URLs for webhooks. The dict can
                          have up to 3 keys: 'insufficient_data', 'alarm',
                          'ok'. Each key is associated with a list of URLs to
                          webhooks that will be invoked when one of the 3
                          actions is taken.
            kwargs      - an arbitrary dict of keyword arguments that are
                          passed to the ceilometer client

        """
        ok_actions = actions.get('ok') if actions is not None else None
        alarm_actions = actions.get('alarm') if actions is not None else None
        insufficient_data_actions = actions.get('insufficient_data') if actions is not None else None

        return self.ceilo_drv.client.alarms.create(
                name=name,
                meter_name=meter,
                statistic=statistic,
                comparison_operator=operation,
                threshold=threshold,
                period=period,
                evaluation_periods=evaluations,
                severity=severity,
                repeat_actions=repeat,
                enabled=enabled,
                ok_actions=ok_actions,
                alarm_actions=alarm_actions,
                insufficient_data_actions=insufficient_data_actions,
                **kwargs
                )

    def ceilo_alarm_update(self, alarm_id, **kwargs):
        """Updates an existing alarm

        Arguments:
            alarm_id - the identifier of the alarm to update
            kwargs   - a dict of the alarm attributes to update

        """
        return self.ceilo_drv.client.alarms.update(alarm_id, **kwargs)

    def ceilo_alarm_delete(self, alarm_id):
        self.ceilo_drv.client.alarms.delete(alarm_id)
