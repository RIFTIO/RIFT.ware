# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import uuid
import collections
import asyncio
import concurrent.futures

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwYang', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwcalYang', '1.0')
from gi.repository import (
    RwDts as rwdts,
    RwYang,
    RwResourceMgrYang,
    RwLaunchpadYang,
    RwcalYang,
)

from gi.repository.RwTypes import RwStatus

class ResMgrCALNotPresent(Exception):
    pass

class ResMgrCloudAccountNotFound(Exception):
    pass

class ResMgrCloudAccountExists(Exception):
    pass

class ResMgrCloudAccountInUse(Exception):
    pass

class ResMgrDuplicatePool(Exception):
    pass

class ResMgrPoolNotAvailable(Exception):
    pass

class ResMgrPoolOperationFailed(Exception):
    pass

class ResMgrDuplicateEventId(Exception):
    pass

class ResMgrUnknownEventId(Exception):
    pass

class ResMgrUnknownResourceId(Exception):
    pass

class ResMgrResourceIdBusy(Exception):
    pass

class ResMgrResourceIdNotAllocated(Exception):
    pass

class ResMgrNoResourcesAvailable(Exception):
    pass

class ResMgrResourcesInitFailed(Exception):
    pass

class ResMgrCALOperationFailure(Exception):
    pass



class ResourceMgrCALHandler(object):
    def __init__(self, loop, executor, log, log_hdl, account):
        self._log = log
        self._loop = loop
        self._executor = executor
        self._account = account.cal_account_msg
        self._rwcal = account.cal
        if account.account_type == 'aws':
            self._subnets = ["172.31.97.0/24", "172.31.98.0/24", "172.31.99.0/24", "172.31.100.0/24", "172.31.101.0/24"]
        else:
            self._subnets = ["11.0.0.0/24",
                             "12.0.0.0/24",
                             "13.0.0.0/24",
                             "14.0.0.0/24",
                             "15.0.0.0/24",
                             "16.0.0.0/24",
                             "17.0.0.0/24",
                             "18.0.0.0/24",
                             "19.0.0.0/24",
                             "20.0.0.0/24",
                             "21.0.0.0/24",
                             "22.0.0.0/24",]
        self._subnet_ptr = 0

    def _select_link_subnet(self):
        subnet = self._subnets[self._subnet_ptr]
        self._subnet_ptr += 1
        if self._subnet_ptr == len(self._subnets):
            self._subnet_ptr = 0
        return subnet

    @asyncio.coroutine
    def create_virtual_network(self, req_params):
        #rc, rsp = self._rwcal.get_virtual_link_list(self._account)
        self._log.debug("Calling get_virtual_link_list API")
        rc, rsp = yield from self._loop.run_in_executor(self._executor,
                                                        self._rwcal.get_virtual_link_list,
                                                        self._account)
            
        assert rc == RwStatus.SUCCESS

        links = [vlink for vlink in rsp.virtual_link_info_list if vlink.name == req_params.name]
        if links:
            self._log.debug("Found existing virtual-network with matching name in cloud. Reusing the virtual-network with id: %s" %(links[0].virtual_link_id))
            return links[0].virtual_link_id

        params = RwcalYang.VirtualLinkReqParams()
        params.from_dict(req_params.as_dict())
        params.subnet = self._select_link_subnet()
        #rc, rs = self._rwcal.create_virtual_link(self._account, params)
        self._log.debug("Calling create_virtual_link API with params: %s" %(str(req_params)))
        rs, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.create_virtual_link,
                                                       self._account,
                                                       params)
        if rc != RwStatus.SUCCESS:
            self._log.error("Virtual-network-allocate operation failed for cloud account: %s", self._account.name)
            raise ResMgrCALOperationFailure("Virtual-network allocate operationa failed for cloud account: %s" %(self._account.name))
        return rs

    @asyncio.coroutine
    def delete_virtual_network(self, network_id):
        #rc = self._rwcal.delete_virtual_link(self._account, network_id)
        self._log.debug("Calling delete_virtual_link API with id: %s" %(network_id))
        rc = yield from self._loop.run_in_executor(self._executor,
                                                   self._rwcal.delete_virtual_link,
                                                   self._account,
                                                   network_id)
        if rc != RwStatus.SUCCESS:
            self._log.error("Virtual-network-release operation failed for cloud account: %s. ResourceID: %s",
                            self._account.name,
                            network_id)
            raise ResMgrCALOperationFailure("Virtual-network release operation failed for cloud account: %s. ResourceId: %s" %(self._account.name, network_id))

    @asyncio.coroutine        
    def get_virtual_network_info(self, network_id):
        #rc, rs = self._rwcal.get_virtual_link(self._account, network_id)
        self._log.debug("Calling get_virtual_link_info API with id: %s" %(network_id))
        rc, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.get_virtual_link,
                                                       self._account,
                                                       network_id)
        if rc != RwStatus.SUCCESS:
            self._log.error("Virtual-network-info operation failed for cloud account: %s. ResourceID: %s",
                            self._account.name,
                            network_id)
            raise ResMgrCALOperationFailure("Virtual-network-info operation failed for cloud account: %s. ResourceID: %s" %(self._account.name, network_id))
        return rs

    @asyncio.coroutine
    def create_virtual_compute(self, req_params):
        #rc, rsp = self._rwcal.get_vdu_list(self._account)
        self._log.debug("Calling get_vdu_list API")
        rc, rsp = yield from self._loop.run_in_executor(self._executor,
                                                        self._rwcal.get_vdu_list,
                                                        self._account)
        assert rc == RwStatus.SUCCESS
        vdus = [vm for vm in rsp.vdu_info_list if vm.name == req_params.name]
        if vdus:
            self._log.debug("Found existing virtual-compute with matching name in cloud. Reusing the virtual-compute element with id: %s" %(vdus[0].vdu_id))
            return vdus[0].vdu_id

        params = RwcalYang.VDUInitParams()
        params.from_dict(req_params.as_dict())

        image_checksum = req_params.image_checksum if req_params.has_field("image_checksum") else None
        params.image_id = yield from self.get_image_id_from_image_info(req_params.image_name, image_checksum)

        #rc, rs = self._rwcal.create_vdu(self._account, params)
        self._log.debug("Calling create_vdu API with params %s" %(str(params)))
        rc, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.create_vdu,
                                                       self._account,
                                                       params)
        if rc != RwStatus.SUCCESS:
            self._log.error("Virtual-compute-create operation failed for cloud account: %s", self._account.name)
            raise ResMgrCALOperationFailure("Virtual-compute-create operation failed for cloud account: %s" %(self._account.name))
        return rs

    @asyncio.coroutine
    def modify_virtual_compute(self, req_params):
        #rc = self._rwcal.modify_vdu(self._account, req_params)
        self._log.debug("Calling modify_vdu API with params: %s" %(str(req_params)))
        rc = yield from self._loop.run_in_executor(self._executor,
                                                   self._rwcal.modify_vdu,
                                                   self._account,
                                                   req_params)
        if rc != RwStatus.SUCCESS:
            self._log.error("Virtual-compute-modify operation failed for cloud account: %s", self._account.name)
            raise ResMgrCALOperationFailure("Virtual-compute-modify operation failed for cloud account: %s" %(self._account.name))

    @asyncio.coroutine        
    def delete_virtual_compute(self, compute_id):
        #rc = self._rwcal.delete_vdu(self._account, compute_id)
        self._log.debug("Calling delete_vdu API with id: %s" %(compute_id))
        rc = yield from self._loop.run_in_executor(self._executor,
                                                   self._rwcal.delete_vdu,
                                                   self._account,
                                                   compute_id)
        if rc != RwStatus.SUCCESS:
            self._log.error("Virtual-compute-release operation failed for cloud account: %s. ResourceID: %s",
                            self._account.name,
                            compute_id)
            raise ResMgrCALOperationFailure("Virtual-compute-release operation failed for cloud account: %s. ResourceID: %s" %(self._account.name, compute_id))

    @asyncio.coroutine        
    def get_virtual_compute_info(self, compute_id):
        #rc, rs = self._rwcal.get_vdu(self._account, compute_id)
        self._log.debug("Calling get_vdu API with id: %s" %(compute_id))
        rc, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.get_vdu,
                                                       self._account,
                                                       compute_id)
        if rc != RwStatus.SUCCESS:
            self._log.error("Virtual-compute-info operation failed for cloud account: %s. ResourceID: %s",
                            self._account.name,
                            compute_id)
            raise ResMgrCALOperationFailure("Virtual-compute-info operation failed for cloud account: %s. ResourceID: %s" %(self._account.name, compute_id))
        return rs

    @asyncio.coroutine
    def get_compute_flavor_info_list(self):
        #rc, rs = self._rwcal.get_flavor_list(self._account)
        self._log.debug("Calling get_flavor_list API")
        rc, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.get_flavor_list,
                                                       self._account)
        if rc != RwStatus.SUCCESS:
            self._log.error("Get-flavor-info-list operation failed for cloud account: %s",
                            self._account.name)
            raise ResMgrCALOperationFailure("Get-flavor-info-list operation failed for cloud account: %s" %(self._account.name))
        return rs.flavorinfo_list

    @asyncio.coroutine
    def create_compute_flavor(self, request):
        flavor = RwcalYang.FlavorInfoItem()
        flavor.name = str(uuid.uuid4())
        epa_types = ['vm_flavor', 'guest_epa', 'host_epa', 'host_aggregate']
        epa_dict = {k: v for k, v in request.as_dict().items() if k in epa_types}
        flavor.from_dict(epa_dict)

        self._log.info("Creating flavor: %s", flavor)
        #rc, rs = self._rwcal.create_flavor(self._account, flavor)
        self._log.debug("Calling create_flavor API")
        rc, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.create_flavor,
                                                       self._account,
                                                       flavor)
        if rc != RwStatus.SUCCESS:
            self._log.error("Create-flavor operation failed for cloud account: %s",
                            self._account.name)
            raise ResMgrCALOperationFailure("Create-flavor operation failed for cloud account: %s" %(self._account.name))
        return rs

    @asyncio.coroutine
    def get_image_info_list(self):
        #rc, rs = self._rwcal.get_image_list(self._account)
        self._log.debug("Calling get_image_list API")
        rc, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.get_image_list,
                                                       self._account)
        if rc != RwStatus.SUCCESS:
            self._log.error("Get-image-info-list operation failed for cloud account: %s",
                            self._account.name)
            raise ResMgrCALOperationFailure("Get-image-info-list operation failed for cloud account: %s" %(self._account.name))
        return rs.imageinfo_list

    @asyncio.coroutine
    def get_image_id_from_image_info(self, image_name, image_checksum=None):
        self._log.debug("Looking up image id for image name %s and checksum %s on cloud account: %s",
                image_name, image_checksum, self._account.name
                )
        image_list = yield from self.get_image_info_list()
        matching_images = [i for i in image_list if i.name == image_name]

        # If the image checksum was filled in then further filter the images by the checksum
        if image_checksum is not None:
            matching_images = [i for i in matching_images if i.checksum == image_checksum]
        else:
            self._log.warning("Image checksum not provided.  Lookup using image name (%s) only.",
                              image_name)

        if len(matching_images) == 0:
            raise ResMgrCALOperationFailure("Could not find image name {} (using checksum: {}) for cloud account: {}".format(
                image_name, image_checksum, self._account.name
                ))

        elif len(matching_images) > 1:
            unique_checksums = {i.checksum for i in matching_images}
            if len(unique_checksums) > 1:
                msg = ("Too many images with different checksums matched "
                       "image name of %s for cloud account: %s" % (image_name, self._account.name))
                raise ResMgrCALOperationFailure(msg)

        return matching_images[0].id

    @asyncio.coroutine
    def get_image_info(self, image_id):
        #rc, rs = self._rwcal.get_image(self._account, image_id)
        self._log.debug("Calling get_image API for id: %s" %(image_id))
        rc, rs = yield from self._loop.run_in_executor(self._executor,
                                                       self._rwcal.get_image,
                                                       self._account,
                                                       image_id)
        if rc != RwStatus.SUCCESS:
            self._log.error("Get-image-info-list operation failed for cloud account: %s",
                            self._account.name)
            raise ResMgrCALOperationFailure("Get-image-info operation failed for cloud account: %s" %(self._account.name))
        return rs.imageinfo_list

    def dynamic_flavor_supported(self):
        return getattr(self._account, self._account.account_type).dynamic_flavor_support


class Resource(object):
    def __init__(self, resource_id, resource_type):
        self._id = resource_id
        self._type = resource_type

    @property
    def resource_id(self):
        return self._id

    @property
    def resource_type(self):
        return self._type

    def cleanup(self):
        pass


class ComputeResource(Resource):
    def __init__(self, resource_id, resource_type):
        super(ComputeResource, self).__init__(resource_id, resource_type)


class NetworkResource(Resource):
    def __init__(self, resource_id, resource_type):
        super(NetworkResource, self).__init__(resource_id, resource_type)


class ResourcePoolInfo(object):
    def __init__(self, name, pool_type, resource_type, max_size):
        self.name = name
        self.pool_type = pool_type
        self.resource_type = resource_type
        self.max_size = max_size

    @classmethod
    def from_dict(cls, pool_dict):
        return cls(
                pool_dict["name"],
                pool_dict["pool_type"],
                pool_dict["resource_type"],
                pool_dict["max_size"],
                )


class ResourcePool(object):
    def __init__(self, log, loop, pool_info, resource_class, cal):
        self._log = log
        self._loop = loop
        self._name = pool_info.name
        self._pool_type = pool_info.pool_type
        self._resource_type = pool_info.resource_type
        self._cal = cal
        self._resource_class = resource_class

        self._max_size = pool_info.max_size

        self._status = 'unlocked'
        ### A Dictionary of all the resources in this pool, keyed by CAL resource-id
        self._all_resources = {}
        ### A List of free resources in this pool
        self._free_resources = []
        ### A Dictionary of all the allocated resources in this pool, keyed by CAL resource-id
        self._allocated_resources = {}

    @property
    def name(self):
        return self._name

    @property
    def cal(self):
        """ This instance's ResourceMgrCALHandler """
        return self._cal

    @property
    def pool_type(self):
        return self._pool_type

    @property
    def resource_type(self):
        return self._resource_type

    @property
    def max_size(self):
        return self._max_size

    @property
    def status(self):
        return self._status

    def in_use(self):
        if len(self._allocated_resources) != 0:
            return True
        else:
            return False

    def update_cal_handler(self, cal):
        if self.in_use():
            raise ResMgrPoolOperationFailed(
                    "Cannot update CAL plugin for in use pool"
                    )

        self._cal = cal

    def lock_pool(self):
        self._log.info("Locking the pool :%s", self.name)
        self._status = 'locked'

    def unlock_pool(self):
        self._log.info("Unlocking the pool :%s", self.name)
        self._status = 'unlocked'

    def add_resource(self, resource_info):
        self._log.info("Adding static resource to Pool: %s, Resource-id: %s Resource-Type: %s",
                       self.name,
                       resource_info.resource_id,
                       self.resource_type)

        ### Add static resources to pool
        resource = self._resource_class(resource_info.resource_id, 'static')
        assert resource.resource_id == resource_info.resource_id
        self._all_resources[resource.resource_id] = resource
        self._free_resources.append(resource)

    def delete_resource(self, resource_id):
        if resource_id not in self._all_resources:
            self._log.error("Resource Id: %s not present in pool: %s. Delete operation failed", resource_id, self.name)
            raise ResMgrUnknownResourceId("Resource Id: %s requested for release is not found" %(resource_id))

        if resource_id in self._allocated_resources:
            self._log.error("Resource Id: %s in use. Delete operation failed", resource_id)
            raise ResMgrResourceIdBusy("Resource Id: %s requested for release is in use" %(resource_id))

        self._log.info("Deleting resource: %s from pool: %s, Resource-Type",
                       resource_id,
                       self.name,
                       self.resource_type)

        resource = self._all_resources.pop(resource_id)
        self._free_resources.remove(resource)
        resource.cleanup()
        del resource

    @asyncio.coroutine
    def read_resource_info(self, resource_id):
        if resource_id not in self._all_resources:
            self._log.error("Resource Id: %s not present in pool: %s. Read operation failed", resource_id, self.name)
            raise ResMgrUnknownResourceId("Resource Id: %s requested for read is not found" %(resource_id))

        if resource_id not in self._allocated_resources:
            self._log.error("Resource Id: %s not in use. Read operation failed", resource_id)
            raise ResMgrResourceIdNotAllocated("Resource Id: %s not in use. Read operation failed" %(resource_id))

        resource = self._allocated_resources[resource_id]
        resource_info = yield from self.get_resource_info(resource)
        return resource_info

    def get_pool_info(self):
        info = RwResourceMgrYang.ResourceRecordInfo()
        self._log.info("Providing info for pool: %s", self.name)
        info.name = self.name
        if self.pool_type:
            info.pool_type = self.pool_type
        if self.resource_type:
            info.resource_type = self.resource_type
        if self.status:
            info.pool_status = self.status

        info.total_resources = len(self._all_resources)
        info.free_resources = len(self._free_resources)
        info.allocated_resources = len(self._allocated_resources)
        return info

    def cleanup(self):
        for _, v in self._all_resources.items():
            v.cleanup()

    @asyncio.coroutine
    def _allocate_static_resource(self, request, resource_type):
        unit_type = {'compute': 'VDU', 'network':'VirtualLink'}
        match_found = False
        resource = None
        self._log.info("Doing resource match from pool :%s", self._free_resources)
        for resource in self._free_resources:
            resource_info = yield from self.get_resource_info(resource)
            self._log.info("Attempting to match %s-requirements for %s: %s with resource-id :%s",
                           resource_type, unit_type[resource_type],request.name, resource.resource_id)
            if self.match_epa_params(resource_info, request):
                if self.match_image_params(resource_info, request):
                    match_found = True
                    self._log.info("%s-requirements matched for %s: %s with resource-id :%s",
                                   resource_type, unit_type[resource_type],request.name, resource.resource_id)
                    yield from self.initialize_resource_in_cal(resource, request)
            break

        if not match_found:
            self._log.error("No match found for %s-requirements for %s: %s in pool: %s. %s instantiation failed",
                            resource_type,
                            unit_type[resource_type],
                            request.name,
                            self.name,
                            unit_type[resource_type])
            return None
        else:
            ### Move resource from free-list into allocated-list
            self._log.info("Allocating the static resource with resource-id: %s for %s: %s",
                           resource.resource_id,
                           unit_type[resource_type],request.name)
            self._free_resources.remove(resource)
            self._allocated_resources[resource.resource_id] = resource

        return resource

    @asyncio.coroutine
    def allocate_resource(self, request):
        resource = yield from self.allocate_resource_in_cal(request)
        resource_info =  yield from self.get_resource_info(resource)
        return resource.resource_id, resource_info

    @asyncio.coroutine
    def release_resource(self, resource_id):
        self._log.debug("Releasing resource_id %s in pool %s", resource_id, self.name)
        if resource_id not in self._allocated_resources:
            self._log.error("Failed to release a resource with resource-id: %s in pool: %s. Resource not known",
                            resource_id,
                            self.name)
            raise ResMgrUnknownResourceId("Failed to release resource with resource-id: %s. Unknown resource-id" %(resource_id))

        ### Get resource object
        resource = self._allocated_resources.pop(resource_id)
        yield from self.uninitialize_resource_in_cal(resource)
        yield from self.release_cal_resource(resource)


class NetworkPool(ResourcePool):
    def __init__(self, log, loop, pool_info, cal):
        super(NetworkPool, self).__init__(log, loop, pool_info, NetworkResource, cal)

    @asyncio.coroutine
    def allocate_resource_in_cal(self, request):
        resource = None
        if self.pool_type == 'static':
            self._log.info("Attempting network resource allocation from static pool: %s", self.name)
            ### Attempt resource allocation from static pool
            resource = yield from self._allocate_static_resource(request, 'network')
        elif self.pool_type == 'dynamic':
            ### Attempt resource allocation from dynamic pool
            self._log.info("Attempting network resource allocation from dynamic pool: %s", self.name)
            if len(self._free_resources) != 0:
                self._log.info("Dynamic pool: %s has %d static resources, Attempting resource allocation from static resources",
                               self.name, len(self._free_resources))
                resource =  yield from self._allocate_static_resource(request, 'network')
            if resource is None:
                self._log.info("Could not resource from static resources. Going for dynamic resource allocation")
                ## Not static resource available. Attempt dynamic resource from pool
                resource = yield from self.allocate_dynamic_resource(request)
        if resource is None:
            raise ResMgrNoResourcesAvailable("No matching resource available for allocation from pool: %s" %(self.name))
        return resource

    @asyncio.coroutine
    def allocate_dynamic_resource(self, request):
        resource_id = yield from self._cal.create_virtual_network(request)
        resource = self._resource_class(resource_id, 'dynamic')
        self._all_resources[resource_id] = resource
        self._allocated_resources[resource_id] = resource
        self._log.info("Successfully allocated virtual-network resource from CAL with resource-id: %s", resource_id)
        return resource

    @asyncio.coroutine
    def release_cal_resource(self, resource):
        if resource.resource_type == 'dynamic':
            self._log.debug("Deleting virtual network with network_id: %s", resource.resource_id)
            yield from self._cal.delete_virtual_network(resource.resource_id)
            self._all_resources.pop(resource.resource_id)
            self._log.info("Successfully released virtual-network resource in CAL with resource-id: %s", resource.resource_id)
        else:
            self._log.info("Successfully released virtual-network resource with resource-id: %s into available-list", resource.resource_id)
            self._free_resources.append(resource)

    @asyncio.coroutine
    def get_resource_info(self, resource):
        info = yield from self._cal.get_virtual_network_info(resource.resource_id)
        self._log.info("Successfully retrieved virtual-network information from CAL with resource-id: %s. Info: %s",
                       resource.resource_id, str(info))
        response = RwResourceMgrYang.VirtualLinkEventData_ResourceInfo()
        response.from_dict(info.as_dict())
        response.pool_name = self.name
        response.resource_state = 'active'
        return response

    @asyncio.coroutine
    def get_info_by_id(self, resource_id):
        info = yield from self._cal.get_virtual_network_info(resource_id)
        self._log.info("Successfully retrieved virtual-network information from CAL with resource-id: %s. Info: %s",
                       resource_id, str(info))
        return info

    def match_image_params(self, resource_info, request_params):
        return True

    def match_epa_params(self, resource_info, request_params):
        if not hasattr(request_params, 'provider_network'):
            ### Its a match if nothing is requested
            return True
        else:
            required = getattr(request_params, 'provider_network')

        if not hasattr(resource_info, 'provider_network'):
            ### Its no match
            return False
        else:
            available = getattr(resource_info, 'provider_network')

        self._log.debug("Matching Network EPA params. Required: %s, Available: %s", required, available)

        if required.has_field('name') and required.name!= available.name:
            self._log.debug("Provider Network mismatch. Required: %s, Available: %s",
                            required.name,
                            available.name)
            return False

        self._log.debug("Matching EPA params physical network name")

        if required.has_field('physical_network') and required.physical_network != available.physical_network:
            self._log.debug("Physical Network mismatch. Required: %s, Available: %s",
                            required.physical_network,
                            available.physical_network)
            return False

        self._log.debug("Matching EPA params overlay type")
        if required.has_field('overlay_type') and required.overlay_type != available.overlay_type:
            self._log.debug("Overlay type mismatch. Required: %s, Available: %s",
                            required.overlay_type,
                            available.overlay_type)
            return False

        self._log.debug("Matching EPA params SegmentationID")
        if required.has_field('segmentation_id') and required.segmentation_id != available.segmentation_id:
            self._log.debug("Segmentation-Id mismatch. Required: %s, Available: %s",
                            required.segmentation_id,
                            available.segmentation_id)
            return False
        return True

    @asyncio.coroutine
    def initialize_resource_in_cal(self, resource, request):
        pass

    @asyncio.coroutine
    def uninitialize_resource_in_cal(self, resource):
        pass


class ComputePool(ResourcePool):
    def __init__(self, log, loop, pool_info, cal):
        super(ComputePool, self).__init__(log, loop, pool_info, ComputeResource, cal)

    @asyncio.coroutine
    def allocate_resource_in_cal(self, request):
        resource = None
        if self.pool_type == 'static':
            self._log.info("Attempting compute resource allocation from static pool: %s", self.name)
            ### Attempt resource allocation from static pool
            resource = yield from self._allocate_static_resource(request, 'compute')
        elif self.pool_type == 'dynamic':
            ### Attempt resource allocation from dynamic pool
            self._log.info("Attempting compute resource allocation from dynamic pool: %s", self.name)
            if len(self._free_resources) != 0:
                self._log.info("Dynamic pool: %s has %d static resources, Attempting resource allocation from static resources",
                               len(self._free_resources),
                               self.name)
                resource = yield from self._allocate_static_resource(request, 'compute')
            if resource is None:
                self._log.info("Attempting for dynamic resource allocation")
                resource = yield from self.allocate_dynamic_resource(request)
        if resource is None:
            raise ResMgrNoResourcesAvailable("No matching resource available for allocation from pool: %s" %(self.name))

        requested_params = RwcalYang.VDUInitParams()
        requested_params.from_dict(request.as_dict())
        resource.requested_params = requested_params
        return resource

    @asyncio.coroutine
    def allocate_dynamic_resource(self, request):
        request.flavor_id = yield from self.select_resource_flavor(request)
        resource_id = yield from self._cal.create_virtual_compute(request)
        resource = self._resource_class(resource_id, 'dynamic')
        self._all_resources[resource_id] = resource
        self._allocated_resources[resource_id] = resource
        self._log.info("Successfully allocated virtual-compute resource from CAL with resource-id: %s", resource_id)
        return resource

    @asyncio.coroutine
    def release_cal_resource(self, resource):
        if hasattr(resource, 'requested_params'):
            delattr(resource, 'requested_params')
        if resource.resource_type == 'dynamic':
            yield from self._cal.delete_virtual_compute(resource.resource_id)
            self._all_resources.pop(resource.resource_id)
            self._log.info("Successfully released virtual-compute resource in CAL with resource-id: %s", resource.resource_id)
        else:
            self._log.info("Successfully released virtual-compute resource with resource-id: %s into available-list", resource.resource_id)
            self._free_resources.append(resource)

    @asyncio.coroutine
    def get_resource_info(self, resource):
        info = yield from self._cal.get_virtual_compute_info(resource.resource_id)
        self._log.info("Successfully retrieved virtual-compute information from CAL with resource-id: %s. Info: %s",
                       resource.resource_id, str(info))
        response = RwResourceMgrYang.VDUEventData_ResourceInfo()
        response.from_dict(info.as_dict())
        response.pool_name = self.name
        response.resource_state = self._get_resource_state(info, resource.requested_params)
        return response

    @asyncio.coroutine
    def get_info_by_id(self, resource_id):
        info = yield from self._cal.get_virtual_compute_info(resource_id)
        self._log.info("Successfully retrieved virtual-compute information from CAL with resource-id: %s. Info: %s",
                       resource_id, str(info))
        return info 

    def _get_resource_state(self, resource_info, requested_params):
        if resource_info.state == 'failed':
            self._log.error("<Compute-Resource: %s> Reached failed state.",
                            resource_info.name)
            return 'failed'

        if resource_info.state != 'active':
            self._log.info("<Compute-Resource: %s> Not reached active state.",
                           resource_info.name)
            return 'pending'

        if not resource_info.has_field('management_ip') or resource_info.management_ip == '':
            self._log.info("<Compute-Resource: %s> Management IP not assigned.",
                           resource_info.name)
            return 'pending'

        if (requested_params.has_field('allocate_public_address')) and (requested_params.allocate_public_address == True):
            if not resource_info.has_field('public_ip'):
                self._log.warning("<Compute-Resource: %s> Management IP not assigned- waiting for public ip, %s",
                                  resource_info.name, requested_params)
                return 'pending'

        if(len(requested_params.connection_points) != 
           len(resource_info.connection_points)):
            self._log.warning("<Compute-Resource: %s> Waiting for requested number of ports to be assigned to virtual-compute, requested: %d, assigned: %d",
                              resource_info.name,
                              len(requested_params.connection_points),
                              len(resource_info.connection_points))
            return 'pending'

        #not_active = [c for c in resource_info.connection_points
        #              if c.state != 'active']

        #if not_active:
        #    self._log.warning("<Compute-Resource: %s> Management IP not assigned- waiting for connection_points , %s",
        #                      resource_info.name, resource_info)
        #    return 'pending'

        ## Find the connection_points which are in active state but does not have IP address
        no_address = [c for c in resource_info.connection_points
                      if (c.state == 'active') and (not c.has_field('ip_address'))]

        if no_address:
            self._log.warning("<Compute-Resource: %s> Management IP not assigned- waiting for connection_points , %s",
                              resource_info.name, resource_info)
            return 'pending'

        return 'active'

    @asyncio.coroutine
    def select_resource_flavor(self, request):
        flavors = yield from self._cal.get_compute_flavor_info_list()
        self._log.debug("Received %d flavor information from RW.CAL", len(flavors))
        flavor_id = None
        match_found = False
        for flv in flavors:
            self._log.info("Attempting to match compute requirement for VDU: %s with flavor %s",
                           request.name, flv)
            if self.match_epa_params(flv, request):
                self._log.info("Flavor match found for compute requirements for VDU: %s with flavor name: %s, flavor-id: %s",
                               request.name, flv.name, flv.id)
                match_found = True
                flavor_id = flv.id
                break

        if not match_found:
            ### Check if CAL account allows dynamic flavor creation
            if self._cal.dynamic_flavor_supported():
                self._log.info("Attempting to create a new flavor for required compute-requirement for VDU: %s", request.name)
                flavor_id = yield from self._cal.create_compute_flavor(request)
            else:
                ### No match with existing flavors and CAL does not support dynamic flavor creation
                self._log.error("Unable to create flavor for compute requirement for VDU: %s. VDU instantiation failed", request.name)
                raise ResMgrNoResourcesAvailable("No resource available with matching EPA attributes")
        else:
            ### Found flavor
            self._log.info("Found flavor with id: %s for compute requirement for VDU: %s",
                           flavor_id, request.name)
        return flavor_id

    def _match_vm_flavor(self, required, available):
        self._log.info("Matching VM Flavor attributes")
        if available.vcpu_count != required.vcpu_count:
            self._log.debug("VCPU requirement mismatch. Required: %d, Available: %d",
                            required.vcpu_count,
                            available.vcpu_count)
            return False
        if available.memory_mb != required.memory_mb:
            self._log.debug("Memory requirement mismatch. Required: %d MB, Available: %d MB",
                            required.memory_mb,
                            available.memory_mb)
            return False
        if available.storage_gb != required.storage_gb:
            self._log.debug("Storage requirement mismatch. Required: %d GB, Available: %d GB",
                            required.storage_gb,
                            available.storage_gb)
            return False
        self._log.debug("VM Flavor match found")
        return True

    def _match_guest_epa(self, required, available):
        self._log.info("Matching Guest EPA attributes")
        if required.has_field('pcie_device'):
            self._log.debug("Matching pcie_device")
            if available.has_field('pcie_device') == False:
                self._log.debug("Matching pcie_device failed. Not available in flavor")
                return False
            else:
                for dev in required.pcie_device:
                    if not [ d for d in available.pcie_device
                             if ((d.device_id == dev.device_id) and (d.count == dev.count)) ]:
                        self._log.debug("Matching pcie_device failed. Required: %s, Available: %s", required.pcie_device, available.pcie_device)
                        return False
        elif available.has_field('pcie_device'):
            self._log.debug("Rejecting available flavor because pcie_device not required but available")
            return False
                        
                    
        if required.has_field('mempage_size'):
            self._log.debug("Matching mempage_size")
            if available.has_field('mempage_size') == False:
                self._log.debug("Matching mempage_size failed. Not available in flavor")
                return False
            else:
                if required.mempage_size != available.mempage_size:
                    self._log.debug("Matching mempage_size failed. Required: %s, Available: %s", required.mempage_size, available.mempage_size)
                    return False
        elif available.has_field('mempage_size'):
            self._log.debug("Rejecting available flavor because mempage_size not required but available")
            return False
        
        if required.has_field('cpu_pinning_policy'):
            self._log.debug("Matching cpu_pinning_policy")
            if required.cpu_pinning_policy != 'ANY':
                if available.has_field('cpu_pinning_policy') == False:
                    self._log.debug("Matching cpu_pinning_policy failed. Not available in flavor")
                    return False
                else:
                    if required.cpu_pinning_policy != available.cpu_pinning_policy:
                        self._log.debug("Matching cpu_pinning_policy failed. Required: %s, Available: %s", required.cpu_pinning_policy, available.cpu_pinning_policy)
                        return False
        elif available.has_field('cpu_pinning_policy'):
            self._log.debug("Rejecting available flavor because cpu_pinning_policy not required but available")
            return False
        
        if required.has_field('cpu_thread_pinning_policy'):
            self._log.debug("Matching cpu_thread_pinning_policy")
            if available.has_field('cpu_thread_pinning_policy') == False:
                self._log.debug("Matching cpu_thread_pinning_policy failed. Not available in flavor")
                return False
            else:
                if required.cpu_thread_pinning_policy != available.cpu_thread_pinning_policy:
                    self._log.debug("Matching cpu_thread_pinning_policy failed. Required: %s, Available: %s", required.cpu_thread_pinning_policy, available.cpu_thread_pinning_policy)
                    return False
        elif available.has_field('cpu_thread_pinning_policy'):
            self._log.debug("Rejecting available flavor because cpu_thread_pinning_policy not required but available")
            return False

        if required.has_field('trusted_execution'):
            self._log.debug("Matching trusted_execution")
            if required.trusted_execution == True:
                if available.has_field('trusted_execution') == False:
                    self._log.debug("Matching trusted_execution failed. Not available in flavor")
                    return False
                else:
                    if required.trusted_execution != available.trusted_execution:
                        self._log.debug("Matching trusted_execution failed. Required: %s, Available: %s", required.trusted_execution, available.trusted_execution)
                        return False
        elif available.has_field('trusted_execution'):
            self._log.debug("Rejecting available flavor because trusted_execution not required but available")
            return False
        
        if required.has_field('numa_node_policy'):
            self._log.debug("Matching numa_node_policy")
            if available.has_field('numa_node_policy') == False:
                self._log.debug("Matching numa_node_policy failed. Not available in flavor")
                return False
            else:
                if required.numa_node_policy.has_field('node_cnt'):
                    self._log.debug("Matching numa_node_policy node_cnt")
                    if available.numa_node_policy.has_field('node_cnt') == False:
                        self._log.debug("Matching numa_node_policy node_cnt failed. Not available in flavor")
                        return False
                    else:
                        if required.numa_node_policy.node_cnt != available.numa_node_policy.node_cnt:
                            self._log.debug("Matching numa_node_policy node_cnt failed. Required: %s, Available: %s",required.numa_node_policy.node_cnt, available.numa_node_policy.node_cnt)
                            return False
                elif available.numa_node_policy.has_field('node_cnt'):
                    self._log.debug("Rejecting available flavor because numa node count not required but available")
                    return False
                
                if required.numa_node_policy.has_field('mem_policy'):
                    self._log.debug("Matching numa_node_policy mem_policy")
                    if available.numa_node_policy.has_field('mem_policy') == False:
                        self._log.debug("Matching numa_node_policy mem_policy failed. Not available in flavor")
                        return False
                    else:
                        if required.numa_node_policy.mem_policy != available.numa_node_policy.mem_policy:
                            self._log.debug("Matching numa_node_policy mem_policy failed. Required: %s, Available: %s", required.numa_node_policy.mem_policy, available.numa_node_policy.mem_policy)
                            return False
                elif available.numa_node_policy.has_field('mem_policy'):
                    self._log.debug("Rejecting available flavor because num node mem_policy not required but available")
                    return False

                if required.numa_node_policy.has_field('node'):
                    self._log.debug("Matching numa_node_policy nodes configuration")
                    if available.numa_node_policy.has_field('node') == False:
                        self._log.debug("Matching numa_node_policy nodes configuration failed. Not available in flavor")
                        return False
                    for required_node in required.numa_node_policy.node:
                        self._log.debug("Matching numa_node_policy nodes configuration for node %s", required_node)
                        numa_match = False
                        for available_node in available.numa_node_policy.node:
                            if required_node.id != available_node.id:
                                self._log.debug("Matching numa_node_policy nodes configuration failed. Required: %s, Available: %s", required_node, available_node)
                                continue
                            if required_node.vcpu != available_node.vcpu:
                                self._log.debug("Matching numa_node_policy nodes configuration failed. Required: %s, Available: %s", required_node, available_node)
                                continue
                            if required_node.memory_mb != available_node.memory_mb:
                                self._log.debug("Matching numa_node_policy nodes configuration failed. Required: %s, Available: %s", required_node, available_node)
                                continue
                            numa_match = True
                        if numa_match == False:
                            return False
                elif available.numa_node_policy.has_field('node'):
                    self._log.debug("Rejecting available flavor because numa nodes not required but available")
                    return False
        elif available.has_field('numa_node_policy'):
            self._log.debug("Rejecting available flavor because numa_node_policy not required but available")
            return False
        self._log.info("Successful match for Guest EPA attributes")
        return True

    def _match_vswitch_epa(self, required, available):
        self._log.debug("VSwitch EPA match found")
        return True

    def _match_hypervisor_epa(self, required, available):
        self._log.debug("Hypervisor EPA match found")
        return True

    def _match_host_epa(self, required, available):
        self._log.info("Matching Host EPA attributes")
        if required.has_field('cpu_model'):
            self._log.debug("Matching CPU model")
            if available.has_field('cpu_model') == False:
                self._log.debug("Matching CPU model failed. Not available in flavor")
                return False
            else:
                #### Convert all PREFER to REQUIRE since flavor will only have REQUIRE attributes
                if required.cpu_model.replace('PREFER', 'REQUIRE') != available.cpu_model:
                    self._log.debug("Matching CPU model failed. Required: %s, Available: %s", required.cpu_model, available.cpu_model)
                    return False
        elif available.has_field('cpu_model'):
            self._log.debug("Rejecting available flavor because cpu_model not required but available")
            return False
        
        if required.has_field('cpu_arch'):
            self._log.debug("Matching CPU architecture")
            if available.has_field('cpu_arch') == False:
                self._log.debug("Matching CPU architecture failed. Not available in flavor")
                return False
            else:
                #### Convert all PREFER to REQUIRE since flavor will only have REQUIRE attributes
                if required.cpu_arch.replace('PREFER', 'REQUIRE') != available.cpu_arch:
                    self._log.debug("Matching CPU architecture failed. Required: %s, Available: %s", required.cpu_arch, available.cpu_arch)
                    return False
        elif available.has_field('cpu_arch'):
            self._log.debug("Rejecting available flavor because cpu_arch not required but available")
            return False
        
        if required.has_field('cpu_vendor'):
            self._log.debug("Matching CPU vendor")
            if available.has_field('cpu_vendor') == False:
                self._log.debug("Matching CPU vendor failed. Not available in flavor")
                return False
            else:
                #### Convert all PREFER to REQUIRE since flavor will only have REQUIRE attributes
                if required.cpu_vendor.replace('PREFER', 'REQUIRE') != available.cpu_vendor:
                    self._log.debug("Matching CPU vendor failed. Required: %s, Available: %s", required.cpu_vendor, available.cpu_vendor)
                    return False
        elif available.has_field('cpu_vendor'):
            self._log.debug("Rejecting available flavor because cpu_vendor not required but available")
            return False

        if required.has_field('cpu_socket_count'):
            self._log.debug("Matching CPU socket count")
            if available.has_field('cpu_socket_count') == False:
                self._log.debug("Matching CPU socket count failed. Not available in flavor")
                return False
            else:
                if required.cpu_socket_count != available.cpu_socket_count:
                    self._log.debug("Matching CPU socket count failed. Required: %s, Available: %s", required.cpu_socket_count, available.cpu_socket_count)
                    return False
        elif available.has_field('cpu_socket_count'):
            self._log.debug("Rejecting available flavor because cpu_socket_count not required but available")
            return False
        
        if required.has_field('cpu_core_count'):
            self._log.debug("Matching CPU core count")
            if available.has_field('cpu_core_count') == False:
                self._log.debug("Matching CPU core count failed. Not available in flavor")
                return False
            else:
                if required.cpu_core_count != available.cpu_core_count:
                    self._log.debug("Matching CPU core count failed. Required: %s, Available: %s", required.cpu_core_count, available.cpu_core_count)
                    return False
        elif available.has_field('cpu_core_count'):
            self._log.debug("Rejecting available flavor because cpu_core_count not required but available")
            return False
        
        if required.has_field('cpu_core_thread_count'):
            self._log.debug("Matching CPU core thread count")
            if available.has_field('cpu_core_thread_count') == False:
                self._log.debug("Matching CPU core thread count failed. Not available in flavor")
                return False
            else:
                if required.cpu_core_thread_count != available.cpu_core_thread_count:
                    self._log.debug("Matching CPU core thread count failed. Required: %s, Available: %s", required.cpu_core_thread_count, available.cpu_core_thread_count)
                    return False
        elif available.has_field('cpu_core_thread_count'):
            self._log.debug("Rejecting available flavor because cpu_core_thread_count not required but available")
            return False
    
        if required.has_field('cpu_feature'):
            self._log.debug("Matching CPU feature list")
            if available.has_field('cpu_feature') == False:
                self._log.debug("Matching CPU feature list failed. Not available in flavor")
                return False
            else:
                for feature in required.cpu_feature:
                    if feature not in available.cpu_feature:
                        self._log.debug("Matching CPU feature list failed. Required feature: %s is not present. Available features: %s", feature, available.cpu_feature)
                        return False
        elif available.has_field('cpu_feature'):
            self._log.debug("Rejecting available flavor because cpu_feature not required but available")
            return False
        self._log.info("Successful match for Host EPA attributes")            
        return True


    def _match_placement_group_inputs(self, required, available):
        self._log.info("Matching Host aggregate attributes")
        
        if not required and not available:
            # Host aggregate not required and not available => success
            self._log.info("Successful match for Host Aggregate attributes")
            return True
        if required and available:
            # Host aggregate requested and available => Do a match and decide
            xx = [ x.as_dict() for x in required ]
            yy = [ y.as_dict() for y in available ]
            for i in xx:
                if i not in yy:
                    self._log.debug("Rejecting available flavor because host Aggregate mismatch. Required: %s, Available: %s ", required, available)
                    return False
            self._log.info("Successful match for Host Aggregate attributes")
            return True
        else:
            # Either of following conditions => Failure
            #  - Host aggregate required but not available
            #  - Host aggregate not required but available
            self._log.debug("Rejecting available flavor because host Aggregate mismatch. Required: %s, Available: %s ", required, available)
            return False
                    
    
    def match_image_params(self, resource_info, request_params):
        return True

    def match_epa_params(self, resource_info, request_params):
        result = self._match_vm_flavor(getattr(request_params, 'vm_flavor'),
                                       getattr(resource_info, 'vm_flavor'))
        if result == False:
            self._log.debug("VM Flavor mismatched")
            return False

        result = self._match_guest_epa(getattr(request_params, 'guest_epa'),
                                       getattr(resource_info, 'guest_epa'))
        if result == False:
            self._log.debug("Guest EPA mismatched")
            return False

        result = self._match_vswitch_epa(getattr(request_params, 'vswitch_epa'),
                                         getattr(resource_info, 'vswitch_epa'))
        if result == False:
            self._log.debug("Vswitch EPA mismatched")
            return False

        result = self._match_hypervisor_epa(getattr(request_params, 'hypervisor_epa'),
                                            getattr(resource_info, 'hypervisor_epa'))
        if result == False:
            self._log.debug("Hypervisor EPA mismatched")
            return False

        result = self._match_host_epa(getattr(request_params, 'host_epa'),
                                      getattr(resource_info, 'host_epa'))
        if result == False:
            self._log.debug("Host EPA mismatched")
            return False

        result = self._match_placement_group_inputs(getattr(request_params, 'host_aggregate'),
                                                    getattr(resource_info, 'host_aggregate'))

        if result == False:
            self._log.debug("Host Aggregate mismatched")
            return False
        
        return True

    @asyncio.coroutine
    def initialize_resource_in_cal(self, resource, request):
        self._log.info("Initializing the compute-resource with id: %s in RW.CAL", resource.resource_id)
        modify_params = RwcalYang.VDUModifyParams()
        modify_params.vdu_id = resource.resource_id
        modify_params.image_id = request.image_id

        for c_point in request.connection_points:
            self._log.debug("Adding connection point for VDU: %s to virtual-compute with id: %s  Connection point Name: %s",
                            request.name,resource.resource_id,c_point.name)
            point = modify_params.connection_points_add.add()
            point.name = c_point.name
            point.virtual_link_id = c_point.virtual_link_id
        yield from self._cal.modify_virtual_compute(modify_params)

    @asyncio.coroutine        
    def uninitialize_resource_in_cal(self, resource):
        self._log.info("Un-initializing the compute-resource with id: %s in RW.CAL", resource.resource_id)
        modify_params = RwcalYang.VDUModifyParams()
        modify_params.vdu_id = resource.resource_id
        resource_info =  yield from self.get_resource_info(resource)
        for c_point in resource_info.connection_points:
            self._log.debug("Removing connection point: %s from VDU: %s ",
                            c_point.name,resource_info.name)
            point = modify_params.connection_points_remove.add()
            point.connection_point_id = c_point.connection_point_id
        yield from self._cal.modify_virtual_compute(modify_params)


class ResourceMgrCore(object):
    def __init__(self, dts, log, log_hdl, loop, parent):
        self._log = log
        self._log_hdl = log_hdl
        self._dts = dts
        self._loop = loop
        self._executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self._parent = parent
        self._cloud_cals = {}
        # Dictionary of pool objects keyed by name
        self._cloud_pool_table = {}
        # Dictionary of tuples (resource_id, cloud_account_name, pool_name) keyed by event_id
        self._resource_table = {}
        self._pool_class = {'compute': ComputePool,
                            'network': NetworkPool}

    def _get_cloud_pool_table(self, cloud_account_name):
        if cloud_account_name not in self._cloud_pool_table:
            msg = "Cloud account %s not found" % cloud_account_name
            self._log.error(msg)
            raise ResMgrCloudAccountNotFound(msg)

        return self._cloud_pool_table[cloud_account_name]

    def _get_cloud_cal_plugin(self, cloud_account_name):
        if cloud_account_name not in self._cloud_cals:
            msg = "Cloud account %s not found" % cloud_account_name
            self._log.error(msg)
            raise ResMgrCloudAccountNotFound(msg)

        return self._cloud_cals[cloud_account_name]

    def _add_default_cloud_pools(self, cloud_account_name):
        self._log.debug("Adding default compute and network pools for cloud account %s",
                        cloud_account_name)
        default_pools = [
                    {
                        'name': '____default_compute_pool',
                        'resource_type': 'compute',
                        'pool_type': 'dynamic',
                        'max_size': 128,
                    },
                    {
                        'name': '____default_network_pool',
                        'resource_type': 'network',
                        'pool_type': 'dynamic',
                        'max_size': 128,
                    },
                ]

        for pool_dict in default_pools:
            pool_info = ResourcePoolInfo.from_dict(pool_dict)
            self._log.info("Applying configuration for cloud account %s pool: %s",
                           cloud_account_name, pool_info.name)

            self.add_resource_pool(cloud_account_name, pool_info)
            self.unlock_resource_pool(cloud_account_name, pool_info.name)

    def get_cloud_account_names(self):
        """ Returns a list of configured cloud account names """
        return self._cloud_cals.keys()

    def add_cloud_account(self, account):
        self._log.debug("Received CAL account. Account Name: %s, Account Type: %s",
                        account.name, account.account_type)

        ### Add cal handler to all the pools
        if account.name in self._cloud_cals:
            raise ResMgrCloudAccountExists("Cloud account already exists in res mgr: %s",
                                           account.name)

        self._cloud_pool_table[account.name] = {}

        cal = ResourceMgrCALHandler(self._loop, self._executor, self._log, self._log_hdl, account)
        self._cloud_cals[account.name] = cal

        self._add_default_cloud_pools(account.name)

    def update_cloud_account(self, account):
        raise NotImplementedError("Update cloud account not implemented")

    def delete_cloud_account(self, account_name, dry_run=False):
        cloud_pool_table = self._get_cloud_pool_table(account_name)
        for pool in cloud_pool_table.values():
            if pool.in_use():
                raise ResMgrCloudAccountInUse("Cannot delete cloud which is currently in use")

        # If dry_run is specified, do not actually delete the cloud account
        if dry_run:
            return

        for pool in list(cloud_pool_table):
            self.delete_resource_pool(account_name, pool)

        del self._cloud_pool_table[account_name]
        del self._cloud_cals[account_name]

    def add_resource_pool(self, cloud_account_name, pool_info):
        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        if pool_info.name in cloud_pool_table:
            raise ResMgrDuplicatePool("Pool with name: %s already exists", pool_info.name)

        cloud_cal = self._get_cloud_cal_plugin(cloud_account_name)
        pool = self._pool_class[pool_info.resource_type](self._log, self._loop, pool_info, cloud_cal)

        cloud_pool_table[pool_info.name] = pool

    def delete_resource_pool(self, cloud_account_name, pool_name):
        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        if pool_name not in cloud_pool_table:
            self._log.error("Pool: %s not found for deletion", pool_name)
            return
        pool = cloud_pool_table[pool_name]

        if pool.in_use():
            # Can't delete a pool in use
            self._log.error("Pool: %s in use. Can not delete in-use pool", pool.name)
            return

        pool.cleanup()
        del cloud_pool_table[pool_name]
        self._log.info("Resource Pool: %s successfully deleted", pool_name)

    def modify_resource_pool(self, cloud_account_name, pool):
        pass

    def lock_resource_pool(self, cloud_account_name, pool_name):
        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        if pool_name not in cloud_pool_table:
            self._log.info("Pool: %s is not available for lock operation")
            return

        pool = cloud_pool_table[pool_name]
        pool.lock_pool()

    def unlock_resource_pool(self, cloud_account_name, pool_name):
        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        if pool_name not in cloud_pool_table:
            self._log.info("Pool: %s is not available for unlock operation")
            return

        pool = cloud_pool_table[pool_name]
        pool.unlock_pool()

    def get_resource_pool_info(self, cloud_account_name, pool_name):
        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        if pool_name in cloud_pool_table:
            pool = cloud_pool_table[pool_name]
            return pool.get_pool_info()
        else:
            return None

    def get_resource_pool_list(self, cloud_account_name):
        return [v for _, v in self._get_cloud_pool_table(cloud_account_name).items()]

    def _select_resource_pools(self, cloud_account_name, resource_type):
        pools = [pool for pool in self.get_resource_pool_list(cloud_account_name) if pool.resource_type == resource_type and pool.status == 'unlocked']
        if not pools:
            raise ResMgrPoolNotAvailable("No %s pool found for resource allocation", resource_type)

        return pools[0]

    @asyncio.coroutine
    def allocate_virtual_resource(self, event_id, cloud_account_name, request, resource_type):
        ### Check if event_id is unique or already in use
        if event_id in self._resource_table:
            r_id, cloud_account_name, pool_name = self._resource_table[event_id]
            self._log.warning("Requested event-id :%s for resource-allocation already active with pool: %s",
                              event_id, pool_name)
            # If resource-type matches then return the same resource
            cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
            pool = cloud_pool_table[pool_name]
            if pool.resource_type == resource_type:

                info = yield from pool.read_resource_info(r_id)
                return info
            else:
                self._log.error("Event-id conflict. Duplicate event-id: %s", event_id)
                raise ResMgrDuplicateEventId("Requested event-id :%s already active with pool: %s" %(event_id, pool_name))

        ### All-OK, lets go ahead with resource allocation
        pool = self._select_resource_pools(cloud_account_name, resource_type)
        self._log.info("Selected pool %s for resource allocation", pool.name)

        r_id, r_info = yield from pool.allocate_resource(request)

        self._resource_table[event_id] = (r_id, cloud_account_name, pool.name)
        return r_info

    @asyncio.coroutine
    def reallocate_virtual_resource(self, event_id, cloud_account_name, request, resource_type, resource):
        ### Check if event_id is unique or already in use
        if event_id in self._resource_table:
            r_id, cloud_account_name, pool_name = self._resource_table[event_id]
            self._log.warning("Requested event-id :%s for resource-allocation already active with pool: %s",
                              event_id, pool_name)
            # If resource-type matches then return the same resource
            cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
            pool = cloud_pool_table[pool_name]
            if pool.resource_type == resource_type:
                info = yield from pool.read_resource_info(r_id)
                return info
            else:
                self._log.error("Event-id conflict. Duplicate event-id: %s", event_id)
                raise ResMgrDuplicateEventId("Requested event-id :%s already active with pool: %s" %(event_id, pool_name))

        r_info = None
        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        pool = cloud_pool_table[resource.pool_name]
        if pool.resource_type == resource_type:
            if resource_type == 'network':
              r_id = resource.virtual_link_id
              r_info = yield from pool.get_info_by_id(resource.virtual_link_id)
            elif resource_type == 'compute':
              r_id = resource.vdu_id
              r_info = yield from pool.get_info_by_id(resource.vdu_id)

        if r_info is None:
            r_id, r_info = yield from pool.allocate_resource(request)
            self._resource_table[event_id] = (r_id, cloud_account_name, resource.pool_name)
            return r_info

        self._resource_table[event_id] = (r_id, cloud_account_name, resource.pool_name)
        new_resource = pool._resource_class(r_id, 'dynamic')
        if resource_type == 'compute':
            requested_params = RwcalYang.VDUInitParams()
            requested_params.from_dict(request.as_dict())
            new_resource.requested_params = requested_params
        pool._all_resources[r_id] = new_resource
        pool._allocated_resources[r_id] = new_resource
        return r_info

    @asyncio.coroutine
    def release_virtual_resource(self, event_id, resource_type):
        ### Check if event_id exists
        if event_id not in self._resource_table:
            self._log.error("Received resource-release-request with unknown Event-id :%s", event_id)
            raise ResMgrUnknownEventId("Received resource-release-request with unknown Event-id :%s" %(event_id))

        ## All-OK, lets proceed with resource release
        r_id, cloud_account_name, pool_name = self._resource_table.pop(event_id)
        self._log.debug("Attempting to release virtual resource id %s from pool %s",
                        r_id, pool_name)

        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        pool = cloud_pool_table[pool_name]
        yield from pool.release_resource(r_id)

    @asyncio.coroutine
    def read_virtual_resource(self, event_id, resource_type):
        ### Check if event_id exists
        if event_id not in self._resource_table:
            self._log.error("Received resource-read-request with unknown Event-id :%s", event_id)
            raise ResMgrUnknownEventId("Received resource-read-request with unknown Event-id :%s" %(event_id))

        ## All-OK, lets proceed
        r_id, cloud_account_name, pool_name = self._resource_table[event_id]
        cloud_pool_table = self._get_cloud_pool_table(cloud_account_name)
        pool = cloud_pool_table[pool_name]
        info = yield from pool.read_resource_info(r_id)
        return info
