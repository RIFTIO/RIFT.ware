
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import functools
import hashlib
import pytest
import os
import tempfile
import shutil
import subprocess

import gi
import rift.auto.session
import rift.mano.examples.ping_pong_nsd as ping_pong
import rift.vcs.vcs

gi.require_version('RwMcYang', '1.0')
from gi.repository import RwMcYang


class PackageError(Exception):
    pass

@pytest.fixture(scope='session', autouse=True)
def cloud_account_name(request):
    '''fixture which returns the name used to identify the cloud account'''
    return 'cloud-0'

@pytest.fixture(autouse=True)
def mc_only(request, standalone_launchpad):
    """Fixture to skip any tests that needs to be run only when a MC is used,
    and not in lp standalone mode.

    Arugments:
        request - pytest request fixture
        standalone_launchpad - indicates if the launchpad is running standalone
    """
    if request.node.get_marker('mc_only'):
        if standalone_launchpad:
            pytest.skip('Test marked skip for launchpad standalone mode')


@pytest.fixture(scope='session')
def launchpad_session(mgmt_session, mgmt_domain_name, session_type, standalone_launchpad):
    '''Fixture containing a rift.auto.session connected to the launchpad

    Arguments:
        mgmt_session         - session connected to the mission control instance 
                               (or launchpad in the case of a standalone session)
        mgmt_domain_name     - name of the mgmt_domain being used
        session_type         - Restconf or Netconf
        standalone_launchpad - indicates if the launchpad is running standalone
    '''
    if standalone_launchpad:
        return mgmt_session

    mc_proxy = mgmt_session.proxy(RwMcYang)
    launchpad_host = mc_proxy.get("/mgmt-domain/domain[name='%s']/launchpad/ip_address" % mgmt_domain_name)

    if session_type == 'netconf':
        launchpad_session = rift.auto.session.NetconfSession(host=launchpad_host)
    elif session_type == 'restconf':
        launchpad_session = rift.auto.session.RestconfSession(host=launchpad_host)

    launchpad_session.connect()
    rift.vcs.vcs.wait_until_system_started(launchpad_session)

    return launchpad_session


@pytest.fixture(scope='session')
def ping_pong_install_dir():
    '''Fixture containing the location of ping_pong installation
    '''
    install_dir = os.path.join(
        os.environ["RIFT_ROOT"],
        "images"
        )
    return install_dir

@pytest.fixture(scope='session')
def ping_vnfd_package_file(ping_pong_install_dir):
    '''Fixture containing the location of the ping vnfd package

    Arguments:
        ping_pong_install_dir - location of ping_pong installation
    '''
    ping_pkg_file = os.path.join(
            ping_pong_install_dir,
            "ping_vnfd_with_image.tar.gz",
            )
    if not os.path.exists(ping_pkg_file):
        raise_package_error()

    return ping_pkg_file


@pytest.fixture(scope='session')
def pong_vnfd_package_file(ping_pong_install_dir):
    '''Fixture containing the location of the pong vnfd package

    Arguments:
        ping_pong_install_dir - location of ping_pong installation
    '''
    pong_pkg_file = os.path.join(
            ping_pong_install_dir,
            "pong_vnfd_with_image.tar.gz",
            )
    if not os.path.exists(pong_pkg_file):
        raise_package_error()

    return pong_pkg_file


@pytest.fixture(scope='session')
def ping_pong_nsd_package_file(ping_pong_install_dir):
    '''Fixture containing the location of the ping_pong_nsd package

    Arguments:
        ping_pong_install_dir - location of ping_pong installation
    '''
    ping_pong_pkg_file = os.path.join(
            ping_pong_install_dir,
            "ping_pong_nsd.tar.gz",
            )
    if not os.path.exists(ping_pong_pkg_file):
        raise_package_error()

    return ping_pong_pkg_file

@pytest.fixture(scope='session')
def image_dirs():
    ''' Fixture containing a list of directories where images can be found
    '''
    rift_build = os.environ['RIFT_BUILD']
    rift_root = os.environ['RIFT_ROOT']
    image_dirs = [
        os.path.join(
            rift_build,
            "modules/core/mano/src/core_mano-build/examples/",
            "ping_pong_ns/ping_vnfd_with_image/images"
        ),
        os.path.join(
            rift_root,
            "images"
        )
    ]
    return image_dirs

@pytest.fixture(scope='session')
def image_paths(image_dirs):
    ''' Fixture containing a mapping of image names to their path images

    Arguments:
        image_dirs - a list of directories where images are located
    '''
    image_paths = {}
    for image_dir in image_dirs:
        if os.path.exists(image_dir):
            names = os.listdir(image_dir)
            image_paths.update({name:os.path.join(image_dir, name) for name in names})
    return image_paths

@pytest.fixture(scope='session')
def path_ping_image(image_paths):
    ''' Fixture containing the location of the ping image

    Arguments:
        image_paths - mapping of images to their paths
    '''
    return image_paths["Fedora-x86_64-20-20131211.1-sda-ping.qcow2"]

@pytest.fixture(scope='session')
def path_pong_image(image_paths):
    ''' Fixture containing the location of the pong image

    Arguments:
        image_paths - mapping of images to their paths
    '''
    return image_paths["Fedora-x86_64-20-20131211.1-sda-pong.qcow2"]

class PingPongFactory:
    def __init__(self, path_ping_image, path_pong_image, rsyslog_host, rsyslog_port):
        self.path_ping_image = path_ping_image
        self.path_pong_image = path_pong_image
        self.rsyslog_host = rsyslog_host
        self.rsyslog_port = rsyslog_port

    def generate_descriptors(self):
        '''Return a new set of ping and pong descriptors
        '''
        def md5sum(path):
            with open(path, mode='rb') as fd:
                md5 = hashlib.md5()
                for buf in iter(functools.partial(fd.read, 4096), b''):
                    md5.update(buf)
            return md5.hexdigest()

        ping_md5sum = md5sum(self.path_ping_image)
        pong_md5sum = md5sum(self.path_pong_image)

        ex_userdata = None
        if self.rsyslog_host and self.rsyslog_port:
            ex_userdata = '''
rsyslog:
  - "$ActionForwardDefaultTemplate RSYSLOG_ForwardFormat"
  - "*.* @{host}:{port}"
            '''.format(
                host=self.rsyslog_host,
                port=self.rsyslog_port,
            )

        descriptors = ping_pong.generate_ping_pong_descriptors(
                pingcount=1,
                ping_md5sum=ping_md5sum,
                pong_md5sum=pong_md5sum,
                ex_ping_userdata=ex_userdata,
                ex_pong_userdata=ex_userdata,
        )

        return descriptors

@pytest.fixture(scope='session')
def ping_pong_factory(path_ping_image, path_pong_image, rsyslog_host, rsyslog_port):
    '''Fixture returns a factory capable of generating ping and pong descriptors
    '''
    return PingPongFactory(path_ping_image, path_pong_image, rsyslog_host, rsyslog_port)

@pytest.fixture(scope='session')
def ping_pong_records(ping_pong_factory):
    '''Fixture returns the default set of ping_pong descriptors
    '''
    return ping_pong_factory.generate_descriptors()


@pytest.fixture(scope='session')
def descriptors(request, ping_pong_records):
    def pingpong_descriptors():
        """Generated the VNFDs & NSD files for pingpong NS.

        Returns:
            Tuple: file path for ping vnfd, pong vnfd and ping_pong_nsd
        """
        ping_vnfd, pong_vnfd, ping_pong_nsd = ping_pong_records

        tmpdir = tempfile.mkdtemp()
        rift_build = os.environ['RIFT_BUILD']
        MANO_DIR = os.path.join(
                rift_build,
                "modules/core/mano/src/core_mano-build/examples/ping_pong_ns")
        ping_img = os.path.join(MANO_DIR, "ping_vnfd_with_image/images/Fedora-x86_64-20-20131211.1-sda-ping.qcow2")
        pong_img = os.path.join(MANO_DIR, "pong_vnfd_with_image/images/Fedora-x86_64-20-20131211.1-sda-pong.qcow2")

        """ grab cached copies of these files if not found. They may not exist 
            because our git submodule dependency mgmt
            will not populate these because they live in .build, not .install
        """
        if not os.path.exists(ping_img):
            ping_img = os.path.join(
                        os.environ['RIFT_ROOT'], 
                        'images/Fedora-x86_64-20-20131211.1-sda-ping.qcow2')
            pong_img = os.path.join(
                        os.environ['RIFT_ROOT'], 
                        'images/Fedora-x86_64-20-20131211.1-sda-pong.qcow2')

        for descriptor in [ping_vnfd, pong_vnfd, ping_pong_nsd]:
            descriptor.write_to_file(output_format='xml', outdir=tmpdir)

        ping_img_path = os.path.join(tmpdir, "{}/images/".format(ping_vnfd.name))
        pong_img_path = os.path.join(tmpdir, "{}/images/".format(pong_vnfd.name))
        os.makedirs(ping_img_path)
        os.makedirs(pong_img_path)

        shutil.copy(ping_img, ping_img_path)
        shutil.copy(pong_img, pong_img_path)

        for dir_name in [ping_vnfd.name, pong_vnfd.name, ping_pong_nsd.name]:
            subprocess.call([
                    "sh",
                    "{}/bin/generate_descriptor_pkg.sh".format(os.environ['RIFT_ROOT']),
                    tmpdir,
                    dir_name])

        return (os.path.join(tmpdir, "{}.tar.gz".format(ping_vnfd.name)),
                os.path.join(tmpdir, "{}.tar.gz".format(pong_vnfd.name)),
                os.path.join(tmpdir, "{}.tar.gz".format(ping_pong_nsd.name)))

    def haproxy_descriptors():
        """HAProxy descriptors."""
        files = [
            os.path.join(os.getenv('RIFT_BUILD'), "modules/ext/vnfs/src/ext_vnfs-build/http_client/http_client_vnfd.tar.gz"),
            os.path.join(os.getenv('RIFT_BUILD'), "modules/ext/vnfs/src/ext_vnfs-build/httpd/httpd_vnfd.tar.gz"),
            os.path.join(os.getenv('RIFT_BUILD'), "modules/ext/vnfs/src/ext_vnfs-build/haproxy/haproxy_vnfd.tar.gz"),
            os.path.join(os.getenv('RIFT_BUILD'), "modules/ext/vnfs/src/ext_vnfs-build/waf/waf_vnfd.tar.gz"),
            os.path.join(os.getenv('RIFT_BUILD'), "modules/ext/vnfs/src/ext_vnfs-build/haproxy_waf_httpd_nsd/haproxy_waf_httpd_nsd.tar.gz")
            ]

        return files


    if request.config.option.network_service == "pingpong":
        return pingpong_descriptors()
    elif request.config.option.network_service == "haproxy":
        return haproxy_descriptors()


@pytest.fixture(scope='session')
def descriptor_images(request):
    def haproxy_images():
        """HAProxy images."""
        images = [
            os.path.join(os.getenv('RIFT_ROOT'), "images/haproxy-v03.qcow2"),
            os.path.join(os.getenv('RIFT_ROOT'), "images/web-app-firewall-v02.qcow2"),
            os.path.join(os.getenv('RIFT_ROOT'), "images/web-server-v02.qcow2")
            ]

        return images

    if request.config.option.network_service == "haproxy":
        return haproxy_images()

    return []
