#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import logging
import os
import ipaddress
import unittest
import uuid
import sys
from gi.repository import RwcalYang

import rift.rwcal.cloudsim.lvm as lvm
import rift.rwcal.cloudsim.lxc as lxc

sys.path.append('../')
import rwcal_cloudsim


logger = logging.getLogger('rwcal-cloudsim')


class CloudsimTest(unittest.TestCase):
    @classmethod
    def cleanUp(cls):
        for container in lxc.containers():
            lxc.stop(container)

        for container in lxc.containers():
            lxc.destroy(container)

        #lvm.destroy("rift")

    @classmethod
    def create_image(cls):
        image = RwcalYang.ImageInfoItem()
        image.name = "rift-lxc-image"
        image.location = "/net/sharedfiles/home1/common/vm/R0.4/rift-mano-devel-latest.qcow2"
        image.disk_format = "qcow2"
        image.id = cls.cal.do_create_image(cls.account, image, no_rwstatus=True)

        cls.image = image

    @classmethod
    def setUpClass(cls):
        cls.cleanUp()

        lvm.create("rift")
        cls.account = RwcalYang.CloudAccount()
        cls.cal = rwcal_cloudsim.CloudSimPlugin()
        cls.create_image()

    def setUp(self):
        pass

    def create_vm(self, image, index):
        vm = RwcalYang.VmInfo()
        vm.vm_name = 'rift-s{}'.format(index + 1)
        vm.image_id = image.id
        vm.user_tags.node_id = str(uuid.uuid4())

        self.cal.do_create_vm(self.account, vm, no_rwstatus=True)

        return vm

    def create_virtual_link(self, index):
        link = RwcalYang.VirtualLinkReqParams()
        link.name = 'link-{}'.format(index + 1)
        link.subnet = '192.168.{}.0/24'.format(index + 1)

        logger.debug("Creating virtual link: %s", link)

        link_id = self.cal.do_create_virtual_link(self.account, link, no_rwstatus=True)
        return link, link_id

    def create_vdu(self, image, index, virtual_link_ids=None):
        vdu_init = RwcalYang.VDUInitParams()
        vdu_init.name = 'rift-vdu{}'.format(index + 1)
        vdu_init.node_id = str(uuid.uuid4())
        vdu_init.image_id = image.id

        if virtual_link_ids is not None:
            for vl_id in virtual_link_ids:
                cp = vdu_init.connection_points.add()
                cp.name = "{}_{}".format(vdu_init.name, vl_id)
                cp.virtual_link_id = vl_id

        vdu_id = self.cal.do_create_vdu(self.account, vdu_init, no_rwstatus=True)

        return vdu_init, vdu_id

    def test_create_vm(self):
        self.create_vm(self.image, 0)

    def test_create_delete_virtual_link(self):
        link, link_id = self.create_virtual_link(0)
        get_link = self.cal.do_get_virtual_link(self.account, link_id, no_rwstatus=True)
        assert get_link.name == link.name
        assert get_link.virtual_link_id == link_id
        assert len(get_link.connection_points) == 0
        assert get_link.state == "active"

        resources = self.cal.do_get_virtual_link_list(self.account, no_rwstatus=True)
        assert len(resources.virtual_link_info_list) == 1
        assert resources.virtual_link_info_list[0] == get_link

        self.cal.do_delete_virtual_link(self.account, link_id, no_rwstatus=True)
        resources = self.cal.do_get_virtual_link_list(self.account, no_rwstatus=True)
        assert len(resources.virtual_link_info_list) == 0

    def test_create_delete_vdu(self):
        vdu, vdu_id = self.create_vdu(self.image, 0)
        get_vdu = self.cal.do_get_vdu(self.account, vdu_id, no_rwstatus=True)

        assert get_vdu.image_id == self.image.id
        assert get_vdu.name == vdu.name
        assert get_vdu.node_id == vdu.node_id

        assert len(get_vdu.connection_points) == 0

        assert get_vdu.vm_flavor.vcpu_count >= 1
        assert get_vdu.vm_flavor.memory_mb >= 8 * 1024
        assert get_vdu.vm_flavor.storage_gb >= 5

        resources = self.cal.do_get_vdu_list(self.account, no_rwstatus=True)
        assert len(resources.vdu_info_list) == 1
        assert resources.vdu_info_list[0] == get_vdu

        resources = self.cal.do_delete_vdu(self.account, vdu_id, no_rwstatus=True)

        resources = self.cal.do_get_vdu_list(self.account, no_rwstatus=True)
        assert len(resources.vdu_info_list) == 0

    def test_create_vdu_single_connection_point(self):
        link, link_id = self.create_virtual_link(0)
        vdu, vdu_id = self.create_vdu(self.image, 0, [link_id])
        get_vdu = self.cal.do_get_vdu(self.account, vdu_id, no_rwstatus=True)
        assert len(get_vdu.connection_points) == 1
        cp = get_vdu.connection_points[0]
        assert (ipaddress.IPv4Address(cp.ip_address) in
                ipaddress.IPv4Network(link.subnet))

        get_link = self.cal.do_get_virtual_link(self.account, link_id, no_rwstatus=True)
        assert len(get_link.connection_points) == 1
        assert get_link.connection_points[0].vdu_id == vdu_id
        assert get_link.connection_points[0].virtual_link_id == link_id

        self.cal.do_delete_vdu(self.account, vdu_id, no_rwstatus=True)
        get_link = self.cal.do_get_virtual_link(self.account, link_id, no_rwstatus=True)
        assert len(get_link.connection_points) == 0

        self.cal.do_delete_virtual_link(self.account, link_id)

    def test_create_vdu_multiple_connection_point(self):
        link1, link1_id = self.create_virtual_link(0)
        link2, link2_id = self.create_virtual_link(1)
        link3, link3_id = self.create_virtual_link(2)
        link_id_map = {link1_id: link1, link2_id: link2, link3_id: link3}

        vdu, vdu_id = self.create_vdu(self.image, 0, link_id_map.keys())
        get_vdu = self.cal.do_get_vdu(self.account, vdu_id, no_rwstatus=True)
        assert len(get_vdu.connection_points) == 3
        for cp in get_vdu.connection_points:
            assert cp.virtual_link_id in link_id_map
            link = link_id_map[cp.virtual_link_id]

            assert (ipaddress.IPv4Address(cp.ip_address) in
                    ipaddress.IPv4Network(link.subnet))

        self.do_delete_vdu(self.account, vdu_id, no_rwstatus=True)

        self.do_delete_virtual_link(self.account, link1_id, no_rwstatus=True)
        self.do_delete_virtual_link(self.account, link2_id, no_rwstatus=True)
        self.do_delete_virtual_link(self.account, link3_id, no_rwstatus=True)

    def test_modify_vdu_add_remove_connection_point(self):
        vdu, vdu_id = self.create_vdu(self.image, 0)
        link, link_id = self.create_virtual_link(0)

        get_vdu = self.cal.do_get_vdu(self.account, vdu_id, no_rwstatus=True)
        assert len(get_vdu.connection_points) == 0

        modify_vdu = RwcalYang.VDUModifyParams()
        modify_vdu.vdu_id = vdu_id
        cp = modify_vdu.connection_points_add.add()
        cp.virtual_link_id = link_id
        cp.name = "link_1"
        self.cal.do_modify_vdu(self.account, modify_vdu, no_rwstatus=True)

        get_vdu = self.cal.do_get_vdu(self.account, vdu_id, no_rwstatus=True)
        assert len(get_vdu.connection_points) == 1

        modify_vdu = RwcalYang.VDUModifyParams()
        modify_vdu.vdu_id = vdu_id
        cp = modify_vdu.connection_points_remove.add()
        cp.connection_point_id = get_vdu.connection_points[0].connection_point_id
        self.cal.do_modify_vdu(self.account, modify_vdu, no_rwstatus=True)

        get_vdu = self.cal.do_get_vdu(self.account, vdu_id, no_rwstatus=True)
        assert len(get_vdu.connection_points) == 0

        self.cal.do_delete_vdu(self.account, vdu_id, no_rwstatus=True)
        self.cal.do_delete_virtual_link(self.account, link_id, no_rwstatus=True)

if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
