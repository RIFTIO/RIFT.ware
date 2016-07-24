"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file cal_test.py
@author Varun Prasad (varun.prasad@riftio.com)
@date 22-Jan-2016

"""

import logging
import os
import time
import uuid

import pytest

from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import RwcalYang
from gi.repository.RwTypes import RwStatus
import rw_peas
import rwlogger

logger = logging.getLogger('rwcal')
logging.basicConfig(level=logging.INFO)


class CloudConfig(object):
    def __init__(self, cal, account):
        self.cal = cal
        self.account = account

    def check_state(self, object_id, object_api, expected_state, state_attr_name="state"):
        """For a given object (Vm, port etc) checks if the object has
        reached the expected state.
        """
        get_object = getattr(self.cal, object_api)
        for i in range(100):  # 100 poll iterations...
            rc, rs = get_object(self.account, object_id)

            curr_state = getattr(rs, state_attr_name)
            print (curr_state)
            if curr_state == expected_state:
                break
            else:
                time.sleep(2)

        rc, rs = get_object(self.account, object_id)
        assert rc == RwStatus.SUCCESS
        assert getattr(rs, state_attr_name) == expected_state


class Aws(CloudConfig):
    def __init__(self, option):
        """
        Args:
            option (OptionParser): OptionParser instance.
        """
        self.image_id = 'ami-7070231a'
        self.virtual_link_id = None
        self.flavor_id = None
        self.vdu_id = None

        super().__init__(self._cal(), self._account(option))

    def _cal(self):
        """
        Loads rw.cal plugin via libpeas
        """
        plugin = rw_peas.PeasPlugin('rwcal_aws', 'RwCal-1.0')

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
            logger.info("AWS Cal plugin successfully instantiated")
        return cal

    def _account(self, option):
        """
        Args:
            option (OptionParser): OptionParser instance.

        Return:
            CloudAccount details
        """
        account = RwcalYang.CloudAccount.from_dict({
                "account_type": "aws",
                "aws": {
                    "key": option.aws_user,
                    "secret": option.aws_password,
                    "region": option.aws_region,
                    "availability_zone": option.aws_zone,
                    "ssh_key": option.aws_ssh_key
                }
            })

        return account

    def flavor(self):
        """
        Returns:
            FlavorInfoItem
        """
        flavor = RwcalYang.FlavorInfoItem.from_dict({
                    "name": str(uuid.uuid4()),
                    "vm_flavor": {
                        "memory_mb": 1024,
                        "vcpu_count": 1,
                        "storage_gb": 0
                    }
            })

        return flavor

    def vdu(self):
        """Provide AWS specific VDU config.

        Returns:
            VDUInitParams
        """
        vdu = RwcalYang.VDUInitParams.from_dict({
                "name": str(uuid.uuid4()),
                "node_id": "123456789012345",
                "image_id": self.image_id,
                "flavor_id": "t2.micro"
            })

        c1 = vdu.connection_points.add()
        c1.name = str(uuid.uuid4())
        c1.virtual_link_id = self.virtual_link_id

        return vdu

    def image(self):
        raise NotImplementedError("Image create APIs are not implemented for AWS")

    def virtual_link(self):
        """Provide Vlink config

        Returns:
            VirtualLinkReqParams
        """
        vlink = RwcalYang.VirtualLinkReqParams.from_dict({
                    "name": str(uuid.uuid4()),
                    "subnet": '172.31.64.0/20',
            })

        return vlink


class Openstack(CloudConfig):
    def __init__(self, option):
        """
        Args:
            option (OptionParser)
        """
        self.image_id = None
        self.virtual_link_id = None
        self.flavor_id = None
        self.vdu_id = None

        super().__init__(self._cal(), self._account(option))

    def _cal(self):
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

    def _account(self, option):
        """Cloud account information for Account

        Returns:
            CloudAccount
        """
        acct = RwcalYang.CloudAccount.from_dict({
            "account_type": "openstack",
            "openstack": {
                    "key": option.os_user,
                    "secret": option.os_password,
                    "auth_url": 'http://{}:5000/v3/'.format(option.os_host),
                    "tenant": option.os_tenant,
                    "mgmt_network": option.os_network
                }
            })

        return acct

    def image(self):
        """Provides Image config for openstack.

        Returns:
            ImageInfoItem
        """
        image = RwcalYang.ImageInfoItem.from_dict({
                "name": str(uuid.uuid4()),
                "location": os.path.join(os.getenv("RIFT_ROOT"), "images/rift-root-latest.qcow2"),
                "disk_format": "qcow2",
                "container_format": "bare",
                "checksum": "12312313123131313131"
            })
        return image

    def flavor(self):
        """Flavor config for openstack

        Returns:
            FlavorInfoItem
        """
        flavor = RwcalYang.FlavorInfoItem.from_dict({
                "name": str(uuid.uuid4()),
                "vm_flavor": {
                        "memory_mb": 16392,
                        "vcpu_count": 4,
                        "storage_gb": 40
                },
                "guest_epa": {
                        "cpu_pinning_policy": "DEDICATED",
                        "cpu_thread_pinning_policy": "SEPARATE",
                }})

        numa_node_count = 2
        flavor.guest_epa.numa_node_policy.node_cnt = numa_node_count
        for i in range(numa_node_count):
            node = flavor.guest_epa.numa_node_policy.node.add()
            node.id = i
            if i == 0:
                node.vcpu = [0, 1]
            elif i == 1:
                node.vcpu = [2, 3]
            node.memory_mb = 8196

        dev = flavor.guest_epa.pcie_device.add()
        dev.device_id = "PCI_10G_ALIAS"
        dev.count = 1

        return flavor

    def vdu(self):
        """Returns VDU config

        Returns:
            VDUInitParams
        """
        vdu = RwcalYang.VDUInitParams.from_dict({
                "name": str(uuid.uuid4()),
                "node_id": "123456789012345",
                "image_id": self.image_id,
                "flavor_id": self.flavor_id,
            })

        c1 = vdu.connection_points.add()
        c1.name = str(uuid.uuid4())
        c1.virtual_link_id = self.virtual_link_id

        return vdu

    def virtual_link(self):
        """vlink config for Openstack

        Returns:
            VirtualLinkReqParams
        """
        vlink = RwcalYang.VirtualLinkReqParams.from_dict({
                    "name": str(uuid.uuid4()),
                    "subnet": '192.168.1.0/24',
            })

        return vlink


@pytest.fixture(scope="module", params=[Openstack], ids=lambda val: val.__name__)
def cloud_config(request):
    return request.param(request.config.option)


@pytest.mark.incremental
class TestCalSetup:
    def test_flavor_apis(self, cloud_config):
        """
        Asserts:
            1. If the new flavor is created and available via read APIs
            2. Verifies the READ APIs
        """
        account = cloud_config.account
        cal = cloud_config.cal

        status, new_flavor_id = cal.create_flavor(account, cloud_config.flavor())
        cloud_config.flavor_id = new_flavor_id
        assert status == RwStatus.SUCCESS

        status, flavors = cal.get_flavor_list(account)
        assert status == RwStatus.SUCCESS

        ids = []
        for flavor in flavors.flavorinfo_list:
            status, flavor_single = cal.get_flavor(account, flavor.id)
            assert status == RwStatus.SUCCESS
            assert flavor.id == flavor_single.id
            ids.append(flavor.id)

        assert new_flavor_id in ids

    def test_image_apis(self, cloud_config):
        """
        Asserts:
            1. If the new image is created and available via read APIs
            2. Verifies the READ APIs
        """
        account = cloud_config.account
        cal = cloud_config.cal

        if type(cloud_config) is Openstack:
            status, new_image_id = cal.create_image(account, cloud_config.image())
            cloud_config.image_id = new_image_id
            assert status == RwStatus.SUCCESS
            cloud_config.check_state(new_image_id, "get_image", "active")
        else:
            # Hack!
            new_image_id = "ami-7070231a"

        status, images = cal.get_image_list(account)

        ids = []
        for image in images.imageinfo_list:
            status, image_single = cal.get_image(account, image.id)
            assert status == RwStatus.SUCCESS
            assert image_single.id == image.id
            ids.append(image.id)

        assert new_image_id in ids

    def test_virtual_link_create(self, cloud_config):
        """
        Asserts:
            1. If the new Vlink is created and available via read APIs
            2. Verifies the READ APIs
        """
        account = cloud_config.account
        cal = cloud_config.cal

        status, new_vlink_id = cal.create_virtual_link(account, cloud_config.virtual_link())
        cloud_config.virtual_link_id = new_vlink_id
        assert status == RwStatus.SUCCESS
        cloud_config.check_state(new_vlink_id, "get_virtual_link", "active")

        status, vlinks = cal.get_virtual_link_list(account)
        assert status == RwStatus.SUCCESS

        ids = []
        for vlink in vlinks.virtual_link_info_list:
            status, vlink_single = cal.get_virtual_link(account, vlink.virtual_link_id)
            assert status == RwStatus.SUCCESS
            assert vlink_single.virtual_link_id == vlink.virtual_link_id
            ids.append(vlink.virtual_link_id)

        assert new_vlink_id in ids

    def test_vdu_apis(self, cloud_config):
        """
        Asserts:
            1. If the new VDU is created and available via read APIs
            2. Verifies the READ APIs
        """
        account = cloud_config.account
        cal = cloud_config.cal

        status, new_vdu_id = cal.create_vdu(account, cloud_config.vdu())
        cloud_config.vdu_id = new_vdu_id
        assert status == RwStatus.SUCCESS
        cloud_config.check_state(new_vdu_id, "get_vdu", "active")

        status, vdus = cal.get_vdu_list(account)
        assert status == RwStatus.SUCCESS

        ids = []
        for vdu in vdus.vdu_info_list:
            status, vdu_single = cal.get_vdu(account, vdu.vdu_id)
            assert status == RwStatus.SUCCESS
            assert vdu_single.vdu_id == vdu.vdu_id
            ids.append(vdu.vdu_id)

        assert new_vdu_id in ids

    def test_modify_vdu_api(self, cloud_config):
        account = cloud_config.account
        cal = cloud_config.cal

        vdu_modify = RwcalYang.VDUModifyParams()
        vdu_modify.vdu_id = cloud_config.vdu_id
        c1 = vdu_modify.connection_points_add.add()
        c1.name = "c_modify1"
        # Set the new vlink
        c1.virtual_link_id = cloud_config.virtual_link_id

        status = cal.modify_vdu(account, vdu_modify)
        assert status == RwStatus.SUCCESS

@pytest.mark.incremental
class TestCalTeardown:
    def test_flavor_delete(self, cloud_config):
        """
        Asserts:
            1. If flavor is deleted
        """
        account = cloud_config.account
        cal = cloud_config.cal

        if type(cloud_config) != Aws:
            status = cal.delete_flavor(account, cloud_config.flavor_id)
            assert status == RwStatus.SUCCESS

    def test_image_delete(self, cloud_config):
        """
        Asserts:
            1. If image is deleted
        """
        account = cloud_config.account
        cal = cloud_config.cal

        if type(cloud_config) != Aws:
            status = cal.delete_image(account, cloud_config.image_id)
            assert status == RwStatus.SUCCESS

    def test_virtual_link_delete(self, cloud_config):
        """
        Asserts:
            1. If VLink is deleted
        """
        account = cloud_config.account
        cal = cloud_config.cal

        status = cal.delete_virtual_link(account, cloud_config.virtual_link_id)
        assert status == RwStatus.SUCCESS

    def test_delete_vdu(self, cloud_config):
        """
        Asserts:
            1. If VDU is deleted
        """
        account = cloud_config.account
        cal = cloud_config.cal

        status = cal.delete_vdu(account, cloud_config.vdu_id)
        assert status == RwStatus.SUCCESS
