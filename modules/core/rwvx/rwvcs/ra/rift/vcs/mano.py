
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

import uuid

from . import core

class NSD(core.Colony):
    """ This class wraps a Colony object and is meant to act as a short-term
    solution for providing NSD attributes without changing any code paths.
    """
    def __init__(self, collection, name, id=str(uuid.uuid4()), provider="RIFT.io", version="1.0"):
        super(NSD, self).__init__(
                clusters=collection.clusters,
                uid=collection.uid,
                name=collection.name,
                )

        self._collection = collection
        self._nsd_name = name
        self._id = id
        self._provider = provider
        self._version = version

    @property
    def nsd_name(self):
        return self._nsd_name

    @property
    def id(self):
        return self._id

    @property
    def provider(self):
        return self._provider

    @property
    def version(self):
        return self._version


class VNFD(core.Cluster):
    """ This class wraps a Cluster object and is meant to act as a short-term
    solution for providing NSD attributes without changing any code paths.
    """
    def __init__(self, collection, vnfd_type=None, vnfd_id=None, provider="RIFT.io", version="1.0"):
        super(VNFD, self).__init__(
                virtual_machines=collection.virtual_machines,
                uid=collection.uid,
                name=collection.name,
                )

        self._collection = collection
        self._vnfd_type = vnfd_type
        self._vnfd_id = vnfd_id
        self._provider = provider
        self._version = version

    @property
    def version(self):
        return self._version

    @property
    def vnfd_id(self):
        return str(self._vnfd_id) if self._vnfd_id is not None else str(self.uid)

    @property
    def vnfd_type(self):
        return self._vnfd_type if self._vnfd_type is not None else self.name

    @property
    def provider(self):
        return self._provider



class VDU(core.VirtualMachine):
    """ This class wraps a VirtualMachine object and is meant to act as a short-term
    solution for providing NSD attributes without changing any code paths.
    """
    def __init__(self, vm, name=None, id=None):
        super(VDU, self).__init__(
                vm.uid,
                vm.ip,
                vm.name,
                vm.tasklets,
                vm.procs,
                vm.leader
                )

        self._vm = vm
        self._vdu_name = name
        self._id = id

    @property
    def id(self):
        return self._id if self._id is not None else self.uid

    @property
    def vdu_name(self):
        return self._vdu_name if self._vdu_name is not None else self.name

    @property
    def vm(self):
        return self._vm
