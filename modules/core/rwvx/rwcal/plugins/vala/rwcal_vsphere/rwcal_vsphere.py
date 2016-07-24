
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import logging
from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import (
    GObject,
    RwCal,
    RwTypes,
    RwcalYang)

import rw_status
import rwlogger

logger = logging.getLogger('rwcal.vsphere')

class UnknownAccountError(Exception):
    pass


class MissingFileError(Exception):
    pass


class ImageLocationError(Exception):
    pass


rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    UnknownAccountError: RwTypes.RwStatus.NOTFOUND,
    MissingFileError: RwTypes.RwStatus.NOTFOUND,
    })


class RwcalVspherePlugin(GObject.Object, RwCal.Cloud):
    """This class implements the CAL VALA methods for Vsphere.
    """

    def __init__(self):
        GObject.Object.__init__(self)
        
    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(
                rwlogger.RwLogger(
                    category="rw-cal-log",
                    subcategory="vsphere",
                    log_hdl=rwlog_ctx,
                )
            )
            
    @rwstatus(ret_on_failure=[None])
    def do_get_management_network(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_tenant(self, account, name):
        raise NotImplementedError()

    @rwstatus
    def do_delete_tenant(self, account, tenant_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_tenant_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_role(self, account, name):
        raise NotImplementedError()

    @rwstatus
    def do_delete_role(self, account, role_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_role_list(self, account):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_create_image(self, account, image):
        raise NotImplementedError()

    
    @rwstatus
    def do_delete_image(self, account, image_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_image(self, account, image_id):
        raise NotImplementedError()
    
    @rwstatus(ret_on_failure=[[]])
    def do_get_image_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_vm(self, account, vm):
        raise NotImplementedError()

    @rwstatus
    def do_start_vm(self, account, vm_id):
        raise NotImplementedError()

    @rwstatus
    def do_stop_vm(self, account, vm_id):
        raise NotImplementedError()

    @rwstatus
    def do_delete_vm(self, account, vm_id):
        raise NotImplementedError()

    @rwstatus
    def do_reboot_vm(self, account, vm_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_vm_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_flavor(self, account, flavor):
        raise NotImplementedError()
    
    @rwstatus
    def do_delete_flavor(self, account, flavor_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_flavor(self, account, flavor_id):
        raise NotImplementedError()
    
    @rwstatus(ret_on_failure=[[]])
    def do_get_flavor_list(self, account):
        raise NotImplementedError()        
            
    @rwstatus
    def do_add_host(self, account, host):
        raise NotImplementedError()

    @rwstatus
    def do_remove_host(self, account, host_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_host(self, account, host_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_host_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_port(self, account, port):
        raise NotImplementedError()

    @rwstatus
    def do_delete_port(self, account, port_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_port(self, account, port_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_port_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_network(self, account, network):
        raise NotImplementedError()

    @rwstatus
    def do_delete_network(self, account, network_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_network(self, account, network_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_network_list(self, account):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[""])
    def do_create_virtual_link(self, account, link_params):
        raise NotImplementedError()
    
    @rwstatus
    def do_delete_virtual_link(self, account, link_id):
        raise NotImplementedError()        
    
    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link(self, account, link_id):
        raise NotImplementedError()
    
    @rwstatus(ret_on_failure=[""])
    def do_get_virtual_link_list(self, account):
        raise NotImplementedError()
    
    @rwstatus(ret_on_failure=[""])
    def do_create_vdu(self, account, vdu_init):
        raise NotImplementedError()            
    
    @rwstatus
    def do_modify_vdu(self, account, vdu_modify):
        raise NotImplementedError()
    
    @rwstatus
    def do_delete_vdu(self, account, vdu_id):
        raise NotImplementedError()
    
    @rwstatus(ret_on_failure=[None])
    def do_get_vdu(self, account, vdu_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[""])
    def do_get_vdu_list(self, account):
        raise NotImplementedError()        
