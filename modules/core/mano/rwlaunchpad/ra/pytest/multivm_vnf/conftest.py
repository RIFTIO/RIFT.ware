
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import gi
import shlex
import pytest
import os
import subprocess
import tempfile

from gi.repository import (
    RwMcYang,
    NsdYang,
    NsrYang,
    RwNsrYang,
    RwVnfrYang,
    VnfrYang,
    VldYang,
    RwVnfdYang,
    RwLaunchpadYang,
    RwBaseYang
)

gi.require_version('RwMcYang', '1.0')
from gi.repository import RwMcYang


@pytest.fixture(scope='session', autouse=True)
def cloud_account_name(request):
    '''fixture which returns the name used to identify the cloud account'''
    return 'cloud-0'

@pytest.fixture(scope='session')
def launchpad_host(request, confd_host):
    return confd_host

@pytest.fixture(scope='session')
def launchpad_session(mgmt_session):
    '''Fixture containing a rift.auto.session connected to the launchpad

    Arguments:
        mgmt_session         - session connected to the mission control instance
                               (or launchpad in the case of a standalone session)
        mgmt_domain_name     - name of the mgmt_domain being used
        session_type         - Restconf or Netconf
        standalone_launchpad - indicates if the launchpad is running standalone
    '''
    return mgmt_session

@pytest.fixture(scope='session')
def launchpad_proxy(request, launchpad_session):
    return launchpad_session

@pytest.fixture(scope='session')
def vnfd_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwVnfdYang)

@pytest.fixture(scope='session')
def vnfr_proxy(request, launchpad_session):
    return launchpad_session.proxy(VnfrYang)

@pytest.fixture(scope='session')
def rwvnfr_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwVnfrYang)

@pytest.fixture(scope='session')
def vld_proxy(request, launchpad_session):
    return launchpad_session.proxy(VldYang)

@pytest.fixture(scope='session')
def nsd_proxy(request, launchpad_session):
    return launchpad_session.proxy(NsdYang)


@pytest.fixture(scope='session')
def nsr_proxy(request, launchpad_session):
    return launchpad_session.proxy(NsrYang)


@pytest.fixture(scope='session')
def rwnsr_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwNsrYang)


@pytest.fixture(scope='session')
def base_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwBaseYang)


@pytest.fixture(scope='session')
def base_proxy(request, launchpad_session):
    return launchpad_session.proxy(RwBaseYang)

@pytest.fixture(scope='session')
def mvv_descr_dir(request):
    """root-directory of descriptors files used for Multi-VM VNF"""
    return os.path.join(
        os.environ["RIFT_INSTALL"],
        "demos/tests/multivm_vnf"
        )

@pytest.fixture(scope='session')
def package_dir(request):
    return tempfile.mkdtemp(prefix="mvv_")

@pytest.fixture(scope='session')
def trafgen_vnfd_package_file(request, package_gen_script, mvv_descr_dir, package_dir):
    pkg_cmd = "{pkg_scr} --descriptor-type='vnfd' --format='xml' --infile='{infile}' --outdir='{outdir}'".format(
            pkg_scr=package_gen_script,
            outdir=package_dir,
            infile=os.path.join(mvv_descr_dir, 'vnfd/xml/multivm_trafgen_vnfd.xml'))
    pkg_file = os.path.join(package_dir, 'multivm_trafgen_vnfd.tar.gz')
    command = shlex.split(pkg_cmd)
    print("Running the command arguments: %s" % command)
    command = [package_gen_script,
               "--descriptor-type", "vnfd",
               "--format", "xml",
               "--infile", "%s" % os.path.join(mvv_descr_dir, 'vnfd/xml/multivm_trafgen_vnfd.xml'),
               "--outdir", "%s" % package_dir]
    print("Running new command arguments: %s" % command)
    subprocess.check_call(command)
    return pkg_file

@pytest.fixture(scope='session')
def trafsink_vnfd_package_file(request, package_gen_script, mvv_descr_dir, package_dir):
    pkg_cmd = "{pkg_scr} --descriptor-type='vnfd' --format='xml' --infile='{infile}' --outdir='{outdir}'".format(
            pkg_scr=package_gen_script,
            outdir=package_dir,
            infile=os.path.join(mvv_descr_dir, 'vnfd/xml/multivm_trafsink_vnfd.xml'))
    pkg_file = os.path.join(package_dir, 'multivm_trafsink_vnfd.tar.gz')
    command = shlex.split(pkg_cmd)
    print("Running the command arguments: %s" % command)
    command = [package_gen_script,
               "--descriptor-type", "vnfd",
               "--format", "xml",
               "--infile", "%s" % os.path.join(mvv_descr_dir, 'vnfd/xml/multivm_trafsink_vnfd.xml'),
               "--outdir", "%s" % package_dir]
    print("Running new command arguments: %s" % command)
    subprocess.check_call(command)
    return pkg_file

@pytest.fixture(scope='session')
def slb_vnfd_package_file(request, package_gen_script, mvv_descr_dir, package_dir):
    pkg_cmd = "{pkg_scr} --outdir {outdir} --infile {infile} --descriptor-type vnfd --format xml".format(
            pkg_scr=package_gen_script,
            outdir=package_dir,
            infile=os.path.join(mvv_descr_dir, 'vnfd/xml/multivm_slb_vnfd.xml'),
            )
    pkg_file = os.path.join(package_dir, 'multivm_slb_vnfd.tar.gz')
    subprocess.check_call(shlex.split(pkg_cmd))
    return pkg_file
