
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import functools

#from . import exceptions


def unsupported(f):
    @functools.wraps(f)
    def impl(*args, **kwargs):
        msg = '{} not supported'.format(f.__name__)
        raise exceptions.RWErrorNotSupported(msg)

    return impl


class Topology(object):
    """
    Topoology defines a base class for sdn driver implementations. Note that
    not all drivers will support the complete set of functionality presented
    here.
    """

    @unsupported
    def get_network_list(self, account):
        """
        Returns the discovered network associated with the specified account.

        @param account - a SDN account

        @return a discovered network
        """
        pass

