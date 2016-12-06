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
# @file pool.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 17/09/2014
#
"""

# Standard libraries
import abc

# 3rd Party libraries
import ndl


class NoAvailableAddresses(Exception):
    def __init__(self, msg):
        super(NoAvailableAddresses, self).__init__(msg)


class UnrecognizedAddress(Exception):
    def __init__(self, msg):
        super(UnrecognizedAddress, self).__init__(msg)


class AddressPool(object):
    """
    This class defines the interface that 'pool' objects are required to
    provide.
    """

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def acquire(self):
        """Returns and removes an IP address from the pool"""
        raise NotImplementedError()

    @abc.abstractmethod
    def release(self, address):
        """Returns an IP address back to the pool"""
        raise NotImplementedError()


class StaticAddressPool(AddressPool):
    """
    This class provides a fixed pool of addresses, i.e. the number of addresses
    associated with the pool is static.
    """

    def __init__(self, addresses):
        """Creates a StaticAddressPool object.

        Arguments:
            addresses - a list of strings representing IP addresses

        """
        self._addresses = addresses
        self._released = []

    def acquire(self):
        """Returns and removes an IP address from the pool

        Raises:
            A NoAvailableAddresses exception will be raised if there are no more
            IP addresses available.

        Returns:
            A string representing an IP address

        """
        if not self._addresses:
            raise NoAvailableAddresses("No addresses available")

        address = self._addresses.pop()
        self._released.append(address)
        return address

    def release(self, address):
        """Returns an IP address back to the pool

        An address that is returned to the pool can be used again via the
        'acquire' function.

        Arguments:
            address - the address to release

        Raises:
            An UnrecognizedAddress exception will be raised if the provided
            address did not originate from this pool.

        """
        if address not in self._released:
            raise UnrecognizedAddress("trying to release an address that was not acquired")

        self._released.remove(address)
        self._addresses.append(address)


class ReservedAddressPool(StaticAddressPool):
    """
    This class provides a pool of addresses that are acquired via the
    reservation system. Apart from the initial acquisition of the addresses,
    this class behaves exactly like the StaticAddressPool class.
    """

    def __init__(self, username, num_addresses):
        """Creates a ReservedAddressPool object.

        A fixed number of addresses are acquired from the reservation system and
        made available through this pool.

        Arguments:
            username      - the name of the user who will acquire the addresses.
            num_addresses - the number of addresses to acquire from the
                            reservation system.

        """
        addresses = ndl.get_vms(count=num_addresses, user=username)
        super(ReservedAddressPool, self).__init__(addresses)


class FakeAddressPool(AddressPool):
    """
    This class generates an unlimited number of IP addresses. There is no
    assumption that these IP addresses are real or exist.
    """

    def __init__(self, prefix='10.0.0.'):
        """Creates a FakeAddressPool object

        Arguments:
            prefix - the string prepended to a counter that is used to generate
                     the fake addresses

        """
        self._prefix = prefix
        self._count = 0
        self._addresses = []
        self._released = []

    def acquire(self):
        """Returns and removes an IP address from the pool

        Any addresses that have been returned to the pool are first provided via
        the 'acquire' function until they have been exhausted. Once the address
        list is exhausted, new address will be automatically generated.

        Returns:
            a string representing an IP address

        """
        if self._addresses:
            address = self._addresses.pop()
            self._released.append(address)
        else:
            self._count += 1
            address = '{}{}'.format(self._prefix, self._count)
            self._released.append(address)

        return address

    def release(self, address):
        """Returns an IP address back to the pool

        An address that is returned to the pool can be used again via the
        'acquire' function.

        Arguments:
            address - the address to release

        Raises:
            An UnrecognizedAddress exception will be raised if the provided
            address did not originate from this pool.

        """
        if address not in self._released:
            raise UnrecognizedAddress("trying to release an address that was not acquired")

        self._released.remove(address)
        self._addresses.append(address)
