# Copyright 2009 Shikhar Bhushan
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.



from ncclient.operations import util
from ncclient.operations import rpc
from ncclient.operations import errors
import ncclient.transport as nctransport
import ncclient.xml_ as ncxml

import threading
from lxml import etree
import logging

logger = logging.getLogger('ncclient.operations.subscribe')

NC_NOTIFICATION_NS = "urn:ietf:params:xml:ns:netconf:notification:1.0"

etree.register_namespace('nc-notif', NC_NOTIFICATION_NS)


class Notification(object):
    """This class encapsulates the Netconf Notificaion received from the Netconf
    server.

    It has the following properties
        event_time - a datetime object indicating the notification time
        event_xml  - XML Element holding the Notification data
    """
    def __init__(self, raw):
        self._raw = raw
        notif_xml = ncxml.to_ele(self._raw)

        # The notification xml contains a xsd:sequence of two elements
        #   - eventTime
        #   - Notification Content (anyxml)
        if len(notif_xml) != 2:
            raise errors.OperationError("Invalid notification xml. " +
                        "Found {} elements.".format(len(notif_xml)))

        if notif_xml[0].tag != ncxml.qualify("eventTime", NC_NOTIFICATION_NS):
            raise errors.OperationError("Element eventTime not found")

        self._event_time = util.parse_datetime(notif_xml[0].text)
        self._event_xml  = notif_xml[1]

    def __repr__(self):
        return self._raw

    @property
    def xml(self):
        """Notification XML as sent by the server"""
        return self._raw

    @property
    def event_time(self):
        """datetime object representing the notification time"""
        return self._event_time

    @property
    def event_xml(self):
        """XML Element having the Notification data"""
        return self._event_xml


class CreateSubscription(rpc.RPC):
    """The create-subscription RPC"""

    def request(self, stream=None, filter=None, start_time=None, stop_time=None):
        """Create a subscription to receive notifications on the session

        Arguments:
            stream - Optional string that specifies the stream of interest
            filter - XML subtree or XPath that indicates which subset of all
                     posible events is of interest
            start_time - Used to trigger the replay feature and indicates when
                         the replay should start. Type: datetime
            stop_time  - Indicates the newest notifications of interest. Used
                         with the replay feature and is a datetime type.
        """
        node = etree.Element("{%s}create-subscription" % NC_NOTIFICATION_NS)

        if stream is not None:
            stream_ele = etree.SubElement(node, "{%s}stream" % NC_NOTIFICATION_NS)
            stream_ele.text = stream

        if filter is not None:
            node.append(util.build_filter(filter))

        if start_time is not None:
            st_ele = etree.SubElement(node, "{%s}startTime" % NC_NOTIFICATION_NS)
            start_time = util.datetime_to_string(start_time)
            st_ele.text = start_time

        if stop_time is not None:
            st_ele = etree.SubElement(node, "{%s}endTime" % NC_NOTIFICATION_NS)
            stop_time = util.datetime_to_string(stop_time)
            st_ele.text = stop_time

        notify_listener = NotificationListener(self._session,
                                        self._device_handler)

        return self._request(node)


class NotificationListener(nctransport.SessionListener):
    """Used to capture the messages received on the session.

    Handles only the netconf notification messages and if a callback is
    registered for the session, the notification callback is invoked.
    """
    creation_lock = threading.Lock()

    # one instance per session
    def __new__(cls, session, device_handler):
        """Ensures that there is only one NotificationListerner per session

        Arguments:
            cls            - Class type
            session        - netconf session on which the notifications are 
                             captured
            device_handler - device specific handler object
        """
        with NotificationListener.creation_lock:
            instance = session.get_listener_instance(cls)
            if instance is None:
                instance = object.__new__(cls)
                instance._session = session
                session.add_listener(instance)
                logger.debug('New notification listener added for session %s',
                             session)
            return instance

    def callback(self, root, raw):
        """Handles the notification netconf message and invokes the registered
        notification callback if any

        Arguments:
            root - contains a tuple a top level tag and its attributes of the
                   received message
            raw  - XML raw string received from the Netconf server
        """
        tag, attrs = root
        if tag != ncxml.qualify("notification", NC_NOTIFICATION_NS):
            return

        logger.info('Notification received %s', raw)
        try:
            if self._session.notification_callback is not None:
                notification_event = Notification(raw)
                self._session.notification_callback(notification_event)
        except errors.OperationError as exp:
            logger.error('Invalid Notification Received: %s %s', exp, raw)


    def errback(self, err):
        """Callback that will be invoked on a session error.
        """
        # Not handled
        pass
    

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
