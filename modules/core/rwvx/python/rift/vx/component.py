"""
#
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
#
# @file component.py
# @author Austin Cormier (austin.cormier@riftio.com)
# @date 09/12/2014
# @brief Component Classes
# @details This file contains classes that are used by Python tasklets
# in order to interact with the Component schema.
#
"""

import collections
import pprint
import logging

import gi
gi.require_version('CF', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwVcsYang', '1.0')
from gi.repository import (
        CF,
        RwBaseYang,
        RwDts,
        RwManifestYang,
        RwTypes,
        RwVcsYang,
        )

class ComponentNotFound(Exception):
    pass

class ComponentInfoDBXactFailed(Exception):
    pass

class ComponentPublishFailed(Exception):
    pass

class ComponentStartFailed(Exception):
    pass


class ComponentInfoDB(object):
    """This class stores and performs queries on ComponentInfo protobuf objects"""
    def __init__(self, log=logging.getLogger(__name__)):
        """Creates a ComponentInfoDB object"""
        self._components = []
        self._log = log

    def add_component_infos(self, components):
        """Add a list of ComponentInfo objects to the instance

        Arguments:
            components - list of ComponentInfo protobuf objects (from rw-base.yang)
        """
        self._components.extend(components)

    def find_component_by_name(self, instance_name):
        """ Finds a component by the component instance name

        Arguments:
            instance_name - The component's instance name

        Returns:
            The ComponentInfo if the instance_name is found else None
        """
        for component in self._components:
            if component.instance_name == instance_name:
                return component

        return None

    def list_components_by_component_name(self, component_name):
        """ List all components with a particular compnent name """
        return [c for c in self._components if c.component_name == component_name]

    def get_component_instance_index(self, instance_name):
        """ Gets a index number for a particular component instance

        In applications where there are multiple instanes of the same
        component, it is useful to get a index for a particular instance.

        For example, if there are three foo_tasklet components with instance
        id's of 100, 153, 189, then when this function is called with the
        foo_tasklet-100 instance name, then 0 is returned, foo_tasklet-153=1,
        foo_tasklet-189=2.

        Arguments:
            instance_name - The component's instance name

        Returns:
            The instance's component index starting from 0.

        Raises:
            ComponentNotFound - If the compnent is not found.
        """
        component = self.find_component_by_name(instance_name)
        if component is None:
            raise ComponentNotFound(
                    "Could not find component using name: {}".format(instance_name)
                    )

        instance_list = self.list_components_by_component_name(component.component_name)

        # Sort the components by instance id
        instance_list.sort(key=lambda c: c.instance_id)

        for i, c in enumerate(instance_list):
            if c.instance_name == instance_name:
                return i

        raise ComponentNotFound(
                "Could not find component {} in instance list: {}".format(
                    instance_name, instance_list
                ))


    def find_parent_component(self, child_instance_name):
        """ Finds a parent component by the child's instance name

        Arguments:
            child_instance_name - A child component's instance name

        Returns:
            The parent ComponentInfo if the child instance has a parent, else None

        Raises:
            ComponentNotFound - If the child component can not be found by name
        """
        child_component = self.find_component_by_name(child_instance_name)
        if child_component is None:
            raise ComponentNotFound("Could not find child component using name: %s",
                    child_instance_name)

        if not child_component.has_field("rwcomponent_parent"):
            self._log.debug("Child does not have a parent: %s", child_instance_name)
            return None

        parent_instance_name = child_component.rwcomponent_parent

        #TODO: Check if the child actually have a set, but None, rwcomponent_parent field?
        if parent_instance_name is None:
            self._log.debug("Child's parent instance is None: %s", parent_instance_name)
            return None

        parent_component = self.find_component_by_name(parent_instance_name)

        return parent_component

    def list_child_components_by_type(self, instance_name, component_type):
        """ Recursively find all child components of a certain component_type

        Arguments:
            instance_name - The parent instance_name to start decending down
            component_type - The component type string (e.g. RWTASKLET) defined
                             in rw-manifest.yang.

        Returns:
            A list of child ComponentInfo objects that match component_type

        Raises:
            ComponentNotFound - If the component can not be found by name
        """
        components = []

        component = self.find_component_by_name(instance_name)
        if component is None:
            raise ComponentNotFound(
                    "Could not find parent component using name: {}".format(instance_name)
                    )

        for child_instance_name in component.rwcomponent_children:
            child = self.find_component_by_name(child_instance_name)
            if child.component_type == component_type:
                components.append(child)
            child_components = self.list_child_tasklets(child.instance_name)
            components.extend(child_components)

        return components

    def list_child_tasklets(self, instance_name):
        """ Recursively find all child tasklet components

        Arguments:
            instance_name - The parent instance_name to start decending down

        Returns:
            A list of child tasklet ComponentInfo objects

        Raises:
            ComponentNotFound - If the component can not be found by name
        """

        return self.list_child_components_by_type(instance_name, "RWTASKLET")

    def find_parent_component_by_type(self, instance_name, parent_component_type):
        """ Recursively find parent component with a certain component_type

        Arguments:
            instance_name - The child instance_name to start parent componentt search

        Returns:
            The parent ComponentInfo object if parent of component_type is found
            None otherwise

        Raises:
            ComponentNotFound - If the child component can not be found by name
        """
        component = self.find_component_by_name(instance_name)
        if component is None:
            raise ComponentNotFound(
                    "Could not find child component using name: {}".format(instance_name)
                    )

        while component.component_type != parent_component_type:
            component = self.find_parent_component(component.instance_name)

        if component.component_type != parent_component_type:
            self._log.debug("Could not find parent component of type(%s) for instance: %s",
                    parent_component_type, instance_name)
            return None

        return component

    def find_parent_vm_component(self, instance_name):
        """ Recursively find parent VM component

        Arguments:
            instance_name - The child instance_name to start parent component search

        Returns:
            The parent VM ComponentInfo object if parent of component_type is found
            None otherwise

        Raises:
            ComponentNotFound - If the child component can not be found by name
        """
        return self.find_parent_component_by_type(instance_name, "RWVM")

    def __len__(self):
        """ Returns the number of ComponentInfo objects stored"""
        return len(self._components)

    def __str__(self):
        """ Return a formatted string of all components (not ordered)"""
        return "Components: {}".format(pprint.pformat(self._components))


class ComponentInfoContext(collections.namedtuple(
        "ComponentInfoContext", ["comp_db", "block"])):
    """ This class encapsulates a ComponentInfoDbCreator transaction context"""
    pass


class ComponentInfoDBCreator(object):
    """ This class is responsible for querying DTS to construct a ComponentInfoDB """

    def __init__(self, tasklet_info, dts_api_h, log):
        """ Creates a ComponentInfoDBCreator object

        Arguments:
            tasklet_info - The tasklets VcsInfo object
            dts_api_h - A DTS handle created when registed with DTS
            log       - A logging.Logger instance
        """
        self._tasklet_info = tasklet_info
        self._dts_api_h = dts_api_h
        self._log = log

        self._comp_db = None
        self._result_cb_entries = []

    def _on_tasklet_info_lookup(self, xact, event, xact_context):
        """ DTS query callback called on any query xact state changes """

        self._log.debug("Got Xact callback for parent VM lookup: %s", locals())

        xact_status = xact.get_status()
        self._log.debug("Got Transaction status: %s", xact_status.status)

        if xact_status.status == RwDts.XactMainState.FAILURE:
            self._log.error("Transaction to get Tasklet Info failed")
            return

        elif xact_status.status == RwDts.XactMainState.ABORTED:
            self._log.warning("Transaction to get Tasklet Info was aborted")
            return

        query_result = xact_context.block.query_result(0)
        while query_result is not None:
            tasklet_info = RwBaseYang.VcsInfo.from_pbcm(query_result.protobuf)
            #self._log.debug("Got TaskletInfo query result: %s", tasklet_info)

            xact_context.comp_db.add_component_infos(tasklet_info.components.component_info)

            query_result = xact_context.block.query_result(0)

        # If this is the last result for the transactions then lets call the client
        # back with a (Exception or None, ComponentInfoDB or None) tuple
        if not xact_context.block.get_query_more_results(0):
            self._log.debug("ComponentInfo query finished (%s components)",
                    len(xact_context.comp_db))

            # Store the ComponentDB and callback anyone waiting on the result
            self._comp_db = xact_context.comp_db
            for (result_cb, result_ctx) in self._result_cb_entries:
                self._create_result_oneshot_cb(result_cb, result_ctx)
            self._result_cb_entries = []

    def _on_timer_oneshot(self, timer, result_cb_ctx):
        cb, ctx = result_cb_ctx
        cb(self._comp_db, ctx)

    def _create_result_oneshot_cb(self, result_cb, result_ctx):
        self._tasklet_info.rwsched_tasklet.CFRunLoopAddTimer(
            self._tasklet_info.rwsched_tasklet.CFRunLoopGetCurrent(),
            self._tasklet_info.rwsched_tasklet.CFRunLoopTimer(
                    CF.CFAbsoluteTimeGetCurrent(),
                    0,
                    self._on_timer_oneshot,
                    (result_cb, result_ctx),
            ),
            self._tasklet_info.rwsched_instance.CFRunLoopGetMainMode(),
        )

    def start(self, xact):
        """ Starts a new DTS transaction to create a ComponentInfoDB

        Arguments:
            xact          - A transaction that this query is associated with
        """

        #RIFT-6474: Create a seperate transaction until subtransactions are working
        xact = self._dts_api_h.xact_create(0, None, None)

        block = xact.block_create()
        status = block.add_query(
                   '/rw-base:vcs/rw-base:info',
                   RwDts.QueryAction.READ,
                   RwDts.XactFlag.MERGE,
                   0,
                )
        if status != RwTypes.RwStatus.SUCCESS:
            raise ComponentStartFailed("Failed to add a query to the block")

        status = block.execute_immediate(
                   0,
                   self._on_tasklet_info_lookup,
                   ComponentInfoContext(
                       comp_db=ComponentInfoDB(),
                       block=block,
                   ),
                )
        if status != RwTypes.RwStatus.SUCCESS:
            raise ComponentStartFailed("Failed to execute the block")

        self._log.debug("Started executing Tasklet Info query block(%s) on xact(%s)",
                block, xact)

        #RIFT-6474: Commit seperate transaction until subtransactions are working
        xact.commit()

    def add_on_result_cb(self, result_cb, result_ctx):
        """Arguments:
            result_cb     - A callback function which is called with a
                            ComponentInfoDB instance
            result_ctx    - Context to pass into the result_cb along with comp_db
        """
        # If the ComponentInfoDB has already been cached, then just
        # callback immediately.  (Consider doing this asynchronously?)
        if self._comp_db is not None:
            self._create_result_oneshot_cb(result_cb, result_ctx)
            return

        self._result_cb_entries.append(
                (result_cb, result_ctx)
                )


class NativeProcPublisherCtx(object):
    def __init__(self, reg_ready_cb, reg_ready_ctx):
        self.reg_ready_cb = reg_ready_cb
        self.reg_ready_ctx = reg_ready_ctx


class NativeProcPublisher(object):
    """
    This class is reponsible for publishing a single NativeProc component.
    """
    def __init__(self,
            dts_api_h,
            log,
            component_name,
            exe_path,
            args=None,
            run_as=None,
            netns_name=None
            ):
        """ Creates an instance of NativeProcPublisher

        Arguments:
            dts_api_h       - A DTS handle created when registed with DTS
            log             - A logging.Logger instance
            component_name  - A globally unique component name
            exe_path        - The file path that should be executed
            args            - The argument sting to pass into the executable
            run_as          - The user that the executable should be run as
            netns_name      - The network namespace to execute process under
        """
        self._dts_api_h = dts_api_h
        self._log = log

        self._component_name = component_name
        self._exe_path = exe_path
        self._args = args
        self._run_as = run_as
        self._netns_name = netns_name

    def _on_reg_ready(self, reg_handle, reg_status, context):
        """ DTS callback when the component has been published """

        self._log.debug("Registration ready for native_proc component: %s",
                self._component_name)

        # Call the client back letting them know that the component has
        # been published (ready to call Vstart)
        context.reg_ready_cb(context.reg_ready_ctx)

    def _on_prepare(self, xact_info, action, keyspec, msg, context):
        """ DTS callback when a subscriber has requested the component info"""

        self._log.debug("Got a prepare callback for native_proc component: %s",
                self._component_name)

        #vcs_comp = RwManifestYang.VcsComponent()
        vcs_comp = RwManifestYang.Inventory_Component()
        vcs_comp.component_name = self._component_name
        vcs_comp.component_type = "PROC"
        vcs_comp.native_proc.exe_path = self._exe_path
        vcs_comp.native_proc.run_as = "root"
        if self._args is not None:
            vcs_comp.native_proc.args = self._args
        if self._run_as is not None:
            vcs_comp.native_proc.run_as = self._run_as
        if self._netns_name is not None:
            vcs_comp.native_proc.network_namespace = self._netns_name

        self._log.debug("Publishing xpath (%s) with component: %s", self.xpath, vcs_comp)

        status = xact_info.respond_xpath(
                RwDts.XactRspCode.ACK,
                self.xpath,
                vcs_comp.to_pbcm()
                )
        if status != RwTypes.RwStatus.SUCCESS:
            raise ComponentPublishFailed("Respond_path failed with status: {}".format(status))

        return RwDts.MemberRspCode.ACTION_ASYNC

    @property
    def xpath(self):
        xpath = (
                "C,/rw-manifest:manifest/rw-manifest:inventory"
                "/rw-manifest:component[rw-manifest:component-name='{}']").format(
                        self._component_name
                        )
        return xpath

    def publish(self, xact, on_publish_cb, on_publish_ctx):
        """ Publish the native process via DTS

        Arguments:
            xact - An existing transaction handle to associate the registration with
            reg_ready_cb - A callback that takes no arguments when the component
                           has been published.
            reg_ready_ctx - Context to pass as an argument into the reg_ready_cb
        """
        #RIFT-6474: Do not publish under existing transaction until subtransactions are working
        xact = None

        self._log.debug("Registering to publish component at xpath: %s", self.xpath)
        context = NativeProcPublisherCtx(on_publish_cb, on_publish_ctx)
        status, reg_handle = self._dts_api_h.member_register_xpath(
                self.xpath,              #Xpath
                xact,                    #Xact Handle
                RwDts.Flag.PUBLISHER,    #Xact Flags
                RwDts.ShardFlavor.NULL,  # Flavor
                0,
                0,
                -1,
                0,
                0,                       #flavor params
                self._on_reg_ready,      #Reg Ready Callback
                self._on_prepare,        #Prepare Callback
                None,                    #Precommit Callback
                None,                    #Commit Callback
                None,                    #Abort Callback
                context,                 #User Data
                )
        if status != RwTypes.RwStatus.SUCCESS:
            raise ComponentPublishFailed("member_register_xpath failed with status: {}".format(status))


class ComponentStartContext(object):
    """ This class encapsulates transaction specific context for ComponentStarter"""
    def __init__(self, component_name, parent_instance, block, started_cb, started_ctx):
        """ Creates a ComponentStartContext object

        Arguments:
            component_name  - The component name we're starting
            block           - The block added to original transaction
            started_cb      - The client callback to invoke when the
                              component instance is started
            started_ctx     - The client context to pass into started_cb
        """
        self.component_name = component_name
        self.parent_instance = parent_instance
        self.block = block
        self.started_cb = started_cb
        self.started_ctx = started_ctx

        self.instance_name = None


class ComponentStarter(object):
    """
    This class is responsible for creating RPC queries which start components
    that have been already published.
    """
    def __init__(self, tasklet_info, dts_api_h, log):
        """ Creates a ComponentStarter object

        Arguments:
            dts_api_h - A DTS handle created when registed with DTS
            log       - A logging.Logger instance
        """
        self._tasklet_info = tasklet_info
        self._dts_api_h = dts_api_h
        self._log = log

    def _on_instance_started(self, xact, event, start_context):
        """ DTS callback which is called in response to the vstart RPC query """

        self._log.debug("Got instance_started callback for component: %s (xact: %s, event: %s)",
                start_context.component_name, xact, event)

        xact_status = xact.get_status()
        if xact_status.status == RwDts.XactMainState.FAILURE:
            self._log.error("Transaction to start component failed: %s",
                    start_context.component_name)
            return

        elif xact_status.status == RwDts.XactMainState.ABORTED:
            self._log.warning("Transaction to start component was aborted: %s",
                    start_context.component_name)
            return

        if start_context.instance_name:
                if xact_status.status != RwDts.XactMainState.COMMITTED:
                        self._log.warning("Only expected once transact is complete: %s",
                                start_context.component_name)
                if start_context.block.get_query_more_results(0):
                        self._log.warning("No more responses to be processed: %s",
                                start_context.component_name)
                return

        query_result = start_context.block.query_result()
        self._log.debug("Got block %s query result: %s", start_context.block, query_result)

        while query_result is not None:
            tasklet_info = RwVcsYang.VStartOutput.from_pbcm(query_result.protobuf)
            self._log.debug("Got component(%s) Vstart RPC response: %s",
                    start_context.component_name, tasklet_info)

            # If the component did not start correctly, abort the transaction
            if tasklet_info.rw_status != 0:
                self._log.error("Component (%s) start failed (rw_status=%s)",
                        start_context.component_name, tasklet_info.rw_status)
                xact.abort()
                return

            start_context.instance_name = tasklet_info.instance_name

            query_result = xact.query_result()


        # If this is the last result for the transactions then lets finish
        # the parent VM lookup operation.
        if not start_context.block.get_query_more_results(0):
            self._log.info("Started new component (%s) with instance name: %s",
                    start_context.component_name, start_context.instance_name)

            # If the instance name is None or empty, abort transaction
            if not start_context.instance_name:
                self._log.error("Did not get a instance name, aborting transaction")
                xact.abort()
                return

            start_context.started_cb(
                    tasklet_info.instance_name,
                    start_context.started_ctx,
                    )

    def start(self,
            xact,
            component_name,
            parent_instance,
            started_cb,
            started_ctx,
            ):
        """ Creates a query to start a already published component

        Arguments:
            xact - An existing transaction to add the block/query under
            component_name - The published component name to start
            parent_instance - The parent instance name to start the component
            started_cb - A callback which takes a single string parameter
                         representing the component instance_name.
            started_ctx - Context which is passed into the started_cb
        """

        #RIFT-6474: Create a seperate transaction until subtransactions are working
        xact = self._dts_api_h.xact_create(0, None, None)

        vstart = RwVcsYang.VStartInput()
        vstart.component_name = component_name
        vstart.parent_instance = parent_instance

        block = xact.block_create()
        status = block.add_query(
                   "/rw-vcs:vstart",        #Query Xpath
                   RwDts.QueryAction.RPC,   #Query Action
                   0,                       #Query Flags
                   0,                       #Query Correlation ID
                   vstart.to_pbcm(),
                )
        if status != RwTypes.RwStatus.SUCCESS:
            raise ComponentInfoDBXactFailed("Failed to add a query to the block")

        status = block.execute_immediate(
                   0,                           #Block flags
                   self._on_instance_started,   #Block callback
                   ComponentStartContext(
                       component_name,
                       parent_instance,
                       block,
                       started_cb,
                       started_ctx,
                       ),
                )
        if status != RwTypes.RwStatus.SUCCESS:
            raise ComponentInfoDBXactFailed("Failed to execute the block")

        self._log.debug("Started block on xact(%s) to create a ComponentInfoDB object: %s",
                xact, block)

        #RIFT-6474: Commit seperate transaction until subtransactions are working
        xact.commit()

