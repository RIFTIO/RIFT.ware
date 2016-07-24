
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import gi
gi.require_version('RwTypes', '1.0')
from gi.repository import (
    IetfNetworkYang,
    IetfNetworkTopologyYang,
    IetfL2TopologyYang,
    RwTopologyYang,
    RwTypes
)
import logging
from gi.repository.RwTypes import RwStatus


class NwtopDataStore(object):
    """ Common datastore for discovered and static topologies """
    def __init__(self, log):
        self._networks = {}
        self._log = log

    """ Deep copy utility for topology class """
    def rwtop_copy_object(self, obj):
        dup = obj.__class__()
        dup.copy_from(obj)
        return dup

    """ Utility for updating L2 topology attributes """
    def _update_l2_attr(self, current_elem, new_elem, new_l2_attr, attr_field_name):
        if not getattr(current_elem, attr_field_name):
           self._log.debug ("Creating L2 attributes..%s", l2_attr_field)
           setattr(current_elem, attr_field_name, new_l2_attr)
           return

        for l2_attr_field in new_l2_attr.fields:
             l2_elem_attr_value = getattr(new_l2_attr, l2_attr_field)
             if l2_elem_attr_value:
                 self._log.debug ("Updating L2 attributes..%s", l2_attr_field)
                 setattr(getattr(current_elem, attr_field_name), l2_attr_field, getattr(new_l2_attr, l2_attr_field))

    """ Utility for updating termination point attributes """
    def _update_termination_point(self, current_node, new_node, new_tp):
        current_tp = next((x for x in current_node.termination_point if x.tp_id == new_tp.tp_id), None)
        if current_tp is None:
            self._log.debug("Creating termination point..%s", new_tp)
            # Add tp to current node
            new_tp_dup = self.rwtop_copy_object(new_tp)
            current_node.termination_point.append(new_tp_dup)
            return
        # Update current tp
        for tp_field in new_tp.fields:
            tp_field_value = getattr(new_tp, tp_field)
            if tp_field_value:
                self._log.debug("Updating termination point..%s", tp_field)
                if (tp_field == 'tp_id'):
                    # Don't change key
                    pass
                elif (tp_field == 'l2_termination_point_attributes'):
                    self._update_l2_attr(current_tp, new_tp, tp_field_value, tp_field)
                elif (tp_field == 'supporting_termination_point'):
                    self._log.debug(tp_field)
                else:
                    self._log.info("Updating termination point..Not implemented %s", tp_field)
                    #raise NotImplementedError

    """ Utility for updating link attributes """
    def _update_link(self, current_nw, new_nw, new_link):
        current_link = next((x for x in current_nw.link if x.link_id == new_link.link_id), None)
        if current_link is None:
            # Add link to current nw
            self._log.info("Creating link..%s", new_link )
            new_link_dup = self.rwtop_copy_object(new_link)
            current_nw.link.append(new_link_dup)
            return
        # Update current link
        for link_field in new_link.fields:
            link_field_value = getattr(new_link, link_field)
            if link_field_value:
                self._log.info("Updating link..%s", link_field)
                if (link_field == 'link_id'):
                    # Don't change key
                    pass
                elif (link_field == 'source'):
                    if getattr(link_field_value, 'source_node') is not None:
                       current_link.source.source_node = getattr(link_field_value, 'source_node')
                    if getattr(link_field_value, 'source_tp') is not None:
                       current_link.source.source_tp = getattr(link_field_value, 'source_tp')
                elif (link_field == 'destination'):
                    if getattr(link_field_value, 'dest_node') is not None:
                       current_link.destination.dest_node = link_field_value.dest_node
                    if getattr(link_field_value, 'dest_tp') is not None:
                       current_link.destination.dest_tp = link_field_value.dest_tp
                elif (link_field == 'l2_link_attributes'):
                    self._update_l2_attr(current_link, new_link, link_field_value, link_field)
                elif (link_field == 'supporting_link'):
                    self._log.debug(link_field)
                else:
                    self._log.info("Update link..Not implemented %s", link_field)
                    #raise NotImplementedError


    """ Utility for updating node attributes """
    def _update_node(self, current_nw, new_nw, new_node):
        current_node = next((x for x in current_nw.node if x.node_id == new_node.node_id), None)
        if current_node is None:
            # Add node to current nw
            self._log.debug("Creating node..%s", new_node)
            new_node_dup = self.rwtop_copy_object(new_node)
            current_nw.node.append(new_node_dup)
            return
        # Update current node
        for node_field in new_node.fields:
            node_field_value = getattr(new_node, node_field)
            if node_field_value:
                self._log.debug("Updating node..%s", node_field)
                if (node_field == 'node_id'):
                    # Don't change key
                    pass
                elif (node_field == 'l2_node_attributes'):
                    self._update_l2_attr(current_node, new_node, node_field_value, node_field)
                elif (node_field == 'termination_point'):
                    for tp in new_node.termination_point:
                        self._update_termination_point(current_node, new_node, tp)
                elif (node_field == 'supporting-node'):
                    self._log.debug(node_field)
                else:
                    self._log.info("Update node..Not implemented %s", node_field)
                    #raise NotImplementedError


    """ API for retrieving internal network """
    def get_network(self, network_id):
        if (network_id not in self._networks):
            return None
        return self._networks[network_id]

    """ API for creating internal network """
    def create_network(self, key, nw):
        self._networks[key] = self.rwtop_copy_object(nw)

    """ API for updating internal network """
    def update_network(self, key, new_nw):
        if key not in self._networks:
            self._log.debug("Creating network..New_nw %s", new_nw)
            self._networks[key] = self.rwtop_copy_object(new_nw)
            return
        # Iterating thru changed fields
        for nw_field in new_nw.fields:
            nw_field_value = getattr(new_nw, nw_field)
            self._log.debug("Update nw..nw_field %s", nw_field)
            if nw_field_value:
                if (nw_field == 'node'):
                    for node in new_nw.node:
                        self._update_node(self._networks[key], new_nw, node)
                elif (nw_field == 'network_id'):
                    # Don't change key
                    pass
                elif (nw_field == 'link'):
                    for link in new_nw.link:
                        self._update_link(self._networks[key], new_nw, link)
                elif (nw_field == 'network_types'):
                    self._networks[key].network_types.l2_network = self._networks[key].network_types.l2_network.new()
                elif (nw_field == 'l2_network_attributes'):
                    self._update_l2_attr(self._networks[key], new_nw, nw_field_value, nw_field)
                else:
                    self._log.info("Update nw..Not implemented %s", nw_field)
                    #raise NotImplementedError

        

