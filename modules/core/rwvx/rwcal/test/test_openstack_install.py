"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file test_openstack_install.py
# @author Varun Prasad (varun.prasad@riftio.com)
# @date 10/10/2015
# @brief Test Openstack/os install
#
"""

import logging
import re
import socket
import sys
import time
import tempfile

from keystoneclient.v3 import client
import paramiko
import pytest
import requests
import xmlrpc.client

from gi.repository import RwcalYang
from gi.repository.RwTypes import RwStatus
import rw_peas
import rwlogger


logger = logging.getLogger()
logging.basicConfig(level=logging.INFO)


class Host(object):
    """A wrapper on top of a host, which provides a ssh connection instance.

    Assumption:
    The username/password for the VM is default.
    """
    _USERNAME = "root"
    _PASSWORD = "riftIO"

    def __init__(self, hostname):
        """
        Args:
            hostname (str): Hostname (grunt3.qanet.riftio.com)
        """
        self.hostname = hostname
        try:
            self.ip = socket.gethostbyname(hostname)
        except socket.gaierror:
            logger.error("Unable to resolve the hostname {}".format(hostname))
            sys.exit(1)

        self.ssh = paramiko.SSHClient()
        # Note: Do not load the system keys as the test will fail if the keys
        # change.
        self.ssh.set_missing_host_key_policy(paramiko.WarningPolicy())

    def connect(self):
        """Set up ssh connection.
        """
        logger.debug("Trying to connect to {}: {}".format(
                self.hostname,
                self.ip))

        self.ssh.connect(
                self.ip,
                username=self._USERNAME,
                password=self._PASSWORD)

    def put(self, content, dest):
        """Creates a tempfile and puts it in the destination path in the HOST.
        Args:
            content (str): Content to be written to a file.
            dest (str): Path to store the content.
        """
        temp_file = tempfile.NamedTemporaryFile(delete=False)
        temp_file.write(content.encode("UTF-8"))
        temp_file.close()

        logger.info("Writing {} file in {}".format(dest, self.hostname))
        sftp = self.ssh.open_sftp()
        sftp.put(temp_file.name, dest)
        sftp.close()

    def clear(self):
        """Clean up
        """
        self.ssh.close()


class Grunt(Host):
    """A wrapper on top of grunt machine, provides functionalities to check
    if the grunt is up, IP resolution.
    """
    @property
    def grunt_name(self):
        """Extract the grunt name from the FQDN

        Returns:
            str: e.g. grunt3 from grunt3.qanet.riftio.com
        """
        return self.hostname.split(".")[0]

    @property
    def dns_server(self):
        """Hard-coded for now.
        """
        return "10.95.0.3"

    @property
    def floating_ip(self):
        return "10.95.1.0"

    @property
    def private_ip(self):
        """Construct the private IP from the grunt name. 10.0.xx.0 where xx is
        value of the grunt (3 in case of grunt3)
        """
        host_part = re.sub(r"[a-zA-z]+", "", self.grunt_name)
        return '10.0.{}.0'.format(host_part)

    def is_system_up(self):
        """Checks if system is up using ssh login.

        Returns:
            bool: Indicates if system is UP
        """
        try:
            self.connect()
        except OSError:
            return False

        return True

    def wait_till_system_is_up(self, timeout=50, check_openstack=False):
        """Blocking call to check if system is up.
        Args:
            timeout (int, optional): In mins(~).
            check_openstack (bool, optional): If true will also check if
                openstack is up and running on the system.

        Raises:
            OSError: If system start exceeds the timeout
        """

        TRY_DURATION = 20  # secs
        total_tries = timeout * (60 / TRY_DURATION)  # 3 tries/mins i.e. 20 secs.
        tries = 0

        while tries < total_tries:
            if self.is_system_up():
                if check_openstack and self.is_openstack_up():
                        return
                elif not check_openstack:
                    return

            logger.info("{} down: Sleeping for {} secs. Try {} of {}".format(
                    self.hostname,
                    TRY_DURATION,
                    tries,
                    int(total_tries)))

            time.sleep(TRY_DURATION)
            tries += 1

        raise OSError("Exception in system start {}({})".format(
                self.hostname,
                self.ip))

    def is_openstack_up(self):
        """Checks if openstack is UP, by verifying the URL.

        Returns:
            bool: Indicates if system is UP
        """
        url = "http://{}/dashboard/".format(self.ip)

        logger.info("Checking if openstack({}) is UP".format(url))

        try:
            requests.get(url)
        except requests.ConnectionError:
            return False

        return True


class Cobbler(Host):
    """A thin wrapper on cobbler and provides an interface using XML rpc client.

    Assumption:
    System instances are already added to cobbler(with ipmi). Adding instances
    can also be automated, can be taken up sometime later.
    """
    def __init__(self, hostname, username="cobbler", password="cobbler"):
        """
        Args:
            hostname (str): Cobbler host.
            username (str, optional): username.
            password (str, optional): password
        """
        super().__init__(hostname)

        url = "https://{}/cobbler_api".format(hostname)

        self.server = xmlrpc.client.ServerProxy(url)
        logger.info("obtained a cobbler instance for the host {}".format(hostname))

        self.token = self.server.login(username, password)
        self.connect()

    def create_profile(self, profile_name, ks_file):
        """Create the profile for the system.

        Args:
            profile_name (str): Name of the profile.
            ks_file (str): Path of the kick start file.
        """
        profile_attrs = {
                "name": profile_name,
                "kickstart": ks_file,
                "repos": ['riftware', 'rift-misc', 'fc21-x86_64-updates',
                          'fc21-x86_64', 'openstack-kilo'],
                "owners": ["admin"],
                "distro": "FC21.3-x86_64"
                }

        profile_id = self.server.new_profile(self.token)
        for key, value in profile_attrs.items():
            self.server.modify_profile(profile_id, key, value, self.token)
        self.server.save_profile(profile_id, self.token)

    def create_snippet(self, snippet_name, snippet_content):
        """Unfortunately the XML rpc apis don't provide a direct interface to
        create snippets, so falling back on the default sftp methods.

        Args:
            snippet_name (str): Name.
            snippet_content (str): snippet's content.

        Returns:
            str: path where the snippet is stored
        """
        path = "/var/lib/cobbler/snippets/{}".format(snippet_name)
        self.put(snippet_content, path)
        return path

    def create_kickstart(self, ks_name, ks_content):
        """Creates and returns the path of the ks file.

        Args:
            ks_name (str): Name of the ks file to be saved.
            ks_content (str): Content for ks file.

        Returns:
            str: path where the ks file is saved.
        """
        path = "/var/lib/cobbler/kickstarts/{}".format(ks_name)
        self.put(ks_content, path)
        return path

    def boot_system(self, grunt, profile_name, false_boot=False):
        """Boots the system with the profile specified. Also enable net-boot

        Args:
            grunt (Grunt): instance of grunt
            profile_name (str): A valid profile name.
            false_boot (bool, optional): debug only option.
        """
        if false_boot:
            return

        system_id = self.server.get_system_handle(
                grunt.grunt_name,
                self.token)
        self.server.modify_system(
                system_id,
                "profile",
                profile_name,
                self.token)

        self.server.modify_system(
                system_id,
                "netboot_enabled",
                "True",
                self.token)
        self.server.save_system(system_id, self.token)
        self.server.power_system(system_id, "reboot", self.token)


class OpenstackTest(object):
    """Driver class to automate the installation.
    """
    def __init__(
            self,
            cobbler,
            controller,
            compute_nodes=None,
            test_prefix="openstack_test"):
        """
        Args:
            cobbler (Cobbler): Instance of Cobbler
            controller (Controller): Controller node instance
            compute_nodes (TYPE, optional): A list of Grunt nodes to be set up
                    as compute nodes.
            test_prefix (str, optional): All entities created by the script are
                    prefixed with this string.
        """
        self.cobbler = cobbler
        self.controller = controller
        self.compute_nodes = [] if compute_nodes is None else compute_nodes
        self.test_prefix = test_prefix

    def _prepare_snippet(self):
        """Prepares the config based on the controller and compute nodes.

        Returns:
            str: Openstack config content.
        """
        content = ""

        config = {}
        config['host_name'] = self.controller.grunt_name
        config['ip'] = self.controller.ip
        config['dns_server'] = self.controller.dns_server
        config['private_ip'] = self.controller.private_ip
        config['floating_ip'] = self.controller.floating_ip

        content += Template.GRUNT_CONFIG.format(**config)
        for compute_node in self.compute_nodes:
            config["host_name"] = compute_node.grunt_name
            content += Template.GRUNT_CONFIG.format(**config)

        content = Template.SNIPPET_TEMPLATE.format(config=content)

        return content

    def prepare_profile(self):
        """Creates the cobbler profile.
        """
        snippet_content = self._prepare_snippet()
        self.cobbler.create_snippet(
                "{}.cfg".format(self.test_prefix),
                snippet_content)

        ks_content = Template.KS_TEMPATE
        ks_file = self.cobbler.create_kickstart(
                "{}.ks".format(self.test_prefix),
                ks_content)

        self.cobbler.create_profile(self.test_prefix, ks_file)
        return self.test_prefix

    def _get_cal_account(self):
        """
        Creates an object for class RwcalYang.CloudAccount()
        """
        account                        = RwcalYang.CloudAccount()
        account.account_type           = "openstack"
        account.openstack.key          = "{}_user".format(self.test_prefix)
        account.openstack.secret       = "mypasswd"
        account.openstack.auth_url     = 'http://{}:35357/v3/'.format(self.controller.ip)
        account.openstack.tenant       = self.test_prefix

        return account

    def start(self):
        """Starts the installation.
        """
        profile_name = self.prepare_profile()

        self.cobbler.boot_system(self.controller, profile_name)
        self.controller.wait_till_system_is_up(check_openstack=True)

        try:
            logger.info("Controller system is UP. Setting up compute nodes")
            for compute_node in self.compute_nodes:
                self.cobbler.boot_system(compute_node, profile_name)
                compute_node.wait_till_system_is_up()
        except OSError as e:
            logger.error("System set-up failed {}".format(e))
            sys.exit(1)

        # Currently we don't have wrapper on top of users/projects so using
        # keystone API directly
        acct = self._get_cal_account()

        keystone_conn = client.Client(
                auth_url=acct.openstack.auth_url,
                username='admin',
                password='mypasswd')

        # Create a test project
        project = keystone_conn.projects.create(
                acct.openstack.tenant,
                "default",
                description="Openstack test project")

        # Create an user
        user = keystone_conn.users.create(
                acct.openstack.key,
                password=acct.openstack.secret,
                default_project=project)

        # Make the newly created user as ADMIN
        admin_role = keystone_conn.roles.list(name="admin")[0]
        keystone_conn.roles.grant(
                admin_role.id,
                user=user.id,
                project=project.id)

        # nova API needs to be restarted, otherwise the new service doesn't play
        # well
        self.controller.ssh.exec_command("source keystonerc_admin && "
                "service openstack-nova-api restart")
        time.sleep(10)

        return acct

    def clear(self):
        """Close out all SFTP connections.
        """
        nodes = [self.controller]
        nodes.extend(self.compute_nodes)
        for node in nodes:
            node.clear()


###############################################################################
## Begin pytests
###############################################################################


@pytest.fixture(scope="session")
def cal(request):
    """
    Loads rw.cal plugin via libpeas
    """
    plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
    engine, info, extension = plugin()

    # Get the RwLogger context
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")

    cal = plugin.get_interface("Cloud")
    try:
        rc = cal.init(rwloggerctx)
        assert rc == RwStatus.SUCCESS
    except:
        logger.error("ERROR:Cal plugin instantiation failed. Aborting tests")
    else:
        logger.info("Openstack Cal plugin successfully instantiated")

    return cal


@pytest.fixture(scope="session")
def account(request):
    """Creates an openstack instance with 1 compute node and returns the newly
    created account.
    """
    cobbler = Cobbler("qacobbler.eng.riftio.com")
    controller = Grunt("grunt3.qanet.riftio.com")
    compute_nodes = [Grunt("grunt5.qanet.riftio.com")]

    test = OpenstackTest(cobbler, controller, compute_nodes)
    account = test.start()

    request.addfinalizer(test.clear)
    return account


def test_list_images(cal, account):
    """Verify if 2 images are present
    """
    status, resources = cal.get_image_list(account)
    assert len(resources.imageinfo_list) == 2

def test_list_flavors(cal, account):
    """Basic flavor checks
    """
    status, resources = cal.get_flavor_list(account)
    assert len(resources.flavorinfo_list) == 5


class Template(object):
    """A container to hold all cobbler related templates.
    """
    GRUNT_CONFIG = """
{host_name})
    CONTROLLER={ip}
    BRGIF=1
    OVSDPDK=N
    TRUSTED=N
    QAT=N
    HUGEPAGE=0
    VLAN=10:14
    PRIVATE_IP={private_ip}
    FLOATING_IP={floating_ip}
    DNS_SERVER={dns_server}
    ;;

    """

    SNIPPET_TEMPLATE = """
# =====================Begining of snippet=================
# snippet openstack_test.cfg
case $name in

{config}

*)
    ;;
esac

# =====================End of snippet=================

"""

    KS_TEMPATE = """
$SNIPPET('rift-repos')
$SNIPPET('rift-base')
%packages
@core
wget
$SNIPPET('rift-grunt-fc21-packages')
ganglia-gmetad
ganglia-gmond
%end

%pre
$SNIPPET('log_ks_pre')
$SNIPPET('kickstart_start')
# Enable installation monitoring
$SNIPPET('pre_anamon')
%end

%post --log=/root/ks_post.log
$SNIPPET('openstack_test.cfg')
$SNIPPET('ganglia')
$SNIPPET('rift-post-yum')
$SNIPPET('rift-post')
$SNIPPET('rift_fix_grub')

$SNIPPET('rdo-post')
echo "banner RDO test" >> /etc/profile

$SNIPPET('kickstart_done')
%end
"""
