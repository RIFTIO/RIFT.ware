
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import mock

import gi
gi.require_version('RwcalYang', '1.0')
from gi.repository import RwcalYang

from . import core

import logging

logger = logging.getLogger('rwsdn.mock')

class Mock(core.Topology):
    """This class implements the abstract methods in the Topology class.
    Mock is used for unit testing."""

    def __init__(self):
        super(Mock, self).__init__()

        m = mock.MagicMock()

        create_default_topology()

    def get_network_list(self, account):
        """
        Returns the discovered network

        @param account - a SDN account

        """
        logger.debug("Not yet implemented")
        return None

