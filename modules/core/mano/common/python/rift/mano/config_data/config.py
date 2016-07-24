############################################################################
# Copyright 2016 RIFT.io Inc                                               #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
############################################################################


import abc
import json
import os
import yaml

from gi.repository import NsdYang
from gi.repository import VnfdYang


class InitialConfigReadError(Exception):
    pass


class InitialConfigMethodError(Exception):
    pass


class InitialConfigPrimitiveReader(object):
    """ Reader for the VNF Initial Config Input Data

    This class interprets the Initial Config Primitive
    Input data and constructs inital config primitive
    protobuf messages.

    The reason for creating a new format is to keep the structure
    as dead-simple as possible for readability.

    The structure (not serialization format) is defined as the
    following.

    [
        {
          "name": <primitive_name>,
          "parameter": {
            "hostname": "pe1"
            "pass": "6windos"
            ...
          }
        }
        ...
    ]

    """
    def __init__(self, primitive_input):
        self._primitives = []

        self._parse_input_data(primitive_input)

    def _parse_input_data(self, input_dict):
        for seq, cfg in enumerate(input_dict):
            if "name" not in cfg:
                raise InitialConfigReadError("Initial config primitive must have a name")

            name = cfg["name"]

            new_primitive = self. get_initial_config_primitive(seq=seq, name=name)
            self._primitives.append(new_primitive)
            if "parameter" in cfg:
                for key, val in cfg["parameter"].items():
                    new_primitive.parameter.add(name=key, value=val)

    @abc.abstractmethod
    def get_initial_config_primitive(self, seq, name):
        '''Override in sub class to provide the correct yang model'''
        raise InitialConfigMethodError(
            "InitialConfigPrimitiveReader Calling abstract class method")

    @property
    def primitives(self):
        """ Returns a copy of the read inital config primitives"""
        return [prim.deep_copy() for prim in self._primitives]

    @classmethod
    def from_yaml_file_hdl(cls, file_hdl):
        """ Create a instance of InitialConfigPrimitiveFileData
        by reading a YAML file handle.

        Arguments:
            file_hdl - A file handle which contains serialized YAML which
                       follows the documented structure.

        Returns:
            A new InitialConfigPrimitiveFileData() instance

        Raises:
            InitialConfigReadError: Input Data was malformed or could not be read
        """
        try:
            input_dict = yaml.safe_load(file_hdl)
        except yaml.YAMLError as e:
            raise InitialConfigReadError(e)

        return cls(input_dict)


class VnfInitialConfigPrimitiveReader(InitialConfigPrimitiveReader):
    '''Class to read the VNF initial config primitives'''

    def __init__(self, primitive_input):
        super(VnfInitialConfigPrimitiveReader, self).__init__(primitive_input)

    def get_initial_config_primitive(self, seq, name):
        return VnfdYang.InitialConfigPrimitive(seq=seq, name=name)


class NsInitialConfigPrimitiveReader(InitialConfigPrimitiveReader):
    '''Class to read the NS initial config primitives'''

    def __init__(self, primitive_input):
        super(NsInitialConfigPrimitiveReader, self).__init__(primitive_input)

    def get_initial_config_primitive(self, seq, name):
        return NsdYang.NsdInitialConfigPrimitive(seq=seq, name=name)


class ConfigPrimitiveConvertor(object):
    PARAMETER = "parameter"
    PARAMETER_GROUP = "parameter_group"
    CONFIG_PRIMITIVE = "config_primitive"
    INITIAL_CONFIG_PRIMITIVE = "initial_config_primitive"

    def _extract_param(self, param, field="default_value"):
        key = param.name
        value = getattr(param, field, None)

        if value is not None:
            setattr(param, field, None)

        return key, value

    def _extract_parameters(self, parameters, input_data, field="default_value"):
        input_data[self.PARAMETER] = {}
        for param in parameters:
            key, value = self._extract_param(param, field)

            if value is None:
                continue

            input_data[self.PARAMETER][key] = value

        if not input_data[self.PARAMETER]:
            del input_data[self.PARAMETER]

    def _extract_parameter_group(self, param_groups, input_data):
        input_data[self.PARAMETER_GROUP] = {}
        for param_group in param_groups:
            input_data[self.PARAMETER_GROUP][param_group.name] = {}
            for param in param_group.parameter:
                key, value = self._extract_param(param)

                if value is None:
                    continue

                input_data[self.PARAMETER_GROUP][param_group.name][key] = value

        if not input_data[self.PARAMETER_GROUP]:
            del input_data[self.PARAMETER_GROUP]

    def extract_config(self,
                       config_primitives=None,
                       initial_configs=None,
                       format="yaml"):
        input_data = {}

        if config_primitives:
            input_data[self.CONFIG_PRIMITIVE] = {}
            for config_primitive in config_primitives:
                input_data[self.CONFIG_PRIMITIVE][config_primitive.name] = {}
                self._extract_parameters(
                    config_primitive.parameter,
                    input_data[self.CONFIG_PRIMITIVE][config_primitive.name])

                try:
                    self._extract_parameter_group(
                        config_primitive.parameter_group,
                        input_data[self.CONFIG_PRIMITIVE][config_primitive.name])
                except AttributeError:
                    pass

                if not input_data[self.CONFIG_PRIMITIVE][config_primitive.name]:
                    del input_data[self.CONFIG_PRIMITIVE][config_primitive.name]

            if not input_data[self.CONFIG_PRIMITIVE]:
                del input_data[self.CONFIG_PRIMITIVE]


        if initial_configs:
            input_data[self.INITIAL_CONFIG_PRIMITIVE] = []
            for in_config_primitive in initial_configs:
                primitive = {}
                self._extract_parameters(
                    in_config_primitive.parameter,
                    primitive,
                    field="value")

                if primitive:
                    input_data[self.INITIAL_CONFIG_PRIMITIVE].append(
                        {
                            "name": in_config_primitive.name,
                            self.PARAMETER: primitive[self.PARAMETER],
                        }
                    )

            if not input_data[self.INITIAL_CONFIG_PRIMITIVE]:
                del input_data[self.INITIAL_CONFIG_PRIMITIVE]

        if len(input_data):
            if format == "json":
                return json.dumps(input_data)
            elif format == "yaml":
                return yaml.dump(input_data, default_flow_style=False)
        else:
            return ''

    def extract_nsd_config(self, nsd, format="yaml"):
        config_prim = None
        try:
            config_prim = nsd.config_primitive
        except AttributeError:
            pass

        initial_conf = None
        try:
            initial_conf = nsd.initial_config_primitive
        except AttributeError:
            pass

        return self.extract_config(
            config_primitives=config_prim,
            initial_configs=initial_conf,
            format=format)

    def extract_vnfd_config(self, vnfd, format="yaml"):
        config_prim = None
        try:
            config_prim = vnfd.mgmt_interface.vnf_configuration.config_primitive
        except AttributeError:
            pass

        initial_conf = None
        try:
            initial_conf = vnfd.mgmt_interface.vnf_configuration.initial_config_primitive
        except AttributeError:
            pass

        return self.extract_config(
            config_primitives=config_prim,
            initial_configs=initial_conf,
            format=format)

    def merge_params(self, parameters, input_config, field="default_value"):
        for param in parameters:
            try:
                setattr(param, field, input_config[param.name])
            except KeyError:
                pass

    def add_nsd_initial_config(self, nsd_init_cfg_prim_msg, input_data):
        """ Add initial config primitives from NS Initial Config Input Data

        Arguments:
            nsd_init_cfg_prim_msg - manotypes:nsd/initial_config_primitive pb msg
            ns_input_data - NsInitialConfigPrimitiveReader documented input data

        Raises:
           InitialConfigReadError: VNF input data was malformed
        """
        if self.INITIAL_CONFIG_PRIMITIVE in input_data:
            ns_input_data = input_data[self.INITIAL_CONFIG_PRIMITIVE]

            reader = NsInitialConfigPrimitiveReader(ns_input_data)
            for prim in reader.primitives:
                nsd_init_cfg_prim_msg.append(prim)

    def merge_nsd_initial_config(self, nsd, input_data):
        try:
            for config_primitive in nsd.initial_config_primitive:
                for cfg in input_data[self.INITIAL_CONFIG_PRIMITIVE]:
                    if cfg['name'] == config_primitive.name:
                        self.merge_params(
                            config_primitive.parameter,
                            cfg[self.PARAMETER],
                            field="value")
                        break

        except AttributeError as e:
            self._log.debug("Did not find initial-config-primitive for NSD {}: {}".
                            format(nsd.name, e))


    def merge_nsd_config(self, nsd, input_data):
        for config_primitive in nsd.config_primitive:
            try:
                cfg = input_data[self.CONFIG_PRIMITIVE][config_primitive.name]
            except KeyError:
                continue

            self.merge_params(
                    config_primitive.parameter,
                    cfg[self.PARAMETER])

            for param_group in config_primitive.parameter_group:
                self.merge_params(
                        param_group.parameter,
                        cfg[self.PARAMETER_GROUP][param_group.name])

    def add_vnfd_initial_config(self, vnfd_init_cfg_prim_msg, input_data):
        """ Add initial config primitives from VNF Initial Config Input Data

        Arguments:
            vnfd_init_cfg_prim_msg - manotypes:vnf-configuration/initial_config_primitive pb msg
            vnf_input_data - VnfInitialConfigPrimitiveReader documented input data

        Raises:
           InitialConfigReadError: VNF input data was malformed
        """
        if self.INITIAL_CONFIG_PRIMITIVE in input_data:
            vnf_input_data = input_data[self.INITIAL_CONFIG_PRIMITIVE]

            reader = VnfInitialConfigPrimitiveReader(vnf_input_data)
            for prim in reader.primitives:
                vnfd_init_cfg_prim_msg.append(prim)

    def merge_vnfd_config(self, vnfd, input_data):
        for config_primitive in vnfd.mgmt_interface.vnf_configuration.config_primitive:
            try:
                cfg = input_data[self.CONFIG_PRIMITIVE][config_primitive.name]
            except KeyError:
                continue

            self.merge_params(
                config_primitive.parameter,
                cfg[self.PARAMETER])


class ConfigStore(object):
    """Convenience class that fetches all the instance related data from the
    $RIFT_ARTIFACTS/launchpad/libs directory.
    """

    def __init__(self, log):
        """
        Args:
            log : Log handle.
        """
        self._log = log
        self.converter = ConfigPrimitiveConvertor()

    def merge_vnfd_config(self, nsd_id, vnfd, member_vnf_index):
        """Merges the vnfd config from the config directory.

        Args:
            nsd_id (str): Id of the NSD object
            vnfd_msg : VNFD pb message containing the VNFD id and
                       the member index ref.
        """
        nsd_archive = os.path.join(
            os.getenv('RIFT_ARTIFACTS'),
            "launchpad/libs",
            nsd_id,
            "config")

        self._log.info("Looking for config from the archive {}".format(nsd_archive))

        if not os.path.exists(nsd_archive):
            return

        config_file = os.path.join(nsd_archive,
                                   "{}__{}.yaml".format(vnfd.id, member_vnf_index))

        if not os.path.exists(config_file):
            self._log.info("Could not find VNF initial config in archive: %s", config_file)
            return

        input_data = self.read_from_file(config_file)
        self._log.info("Loaded VNF config file {}: {}".format(config_file, input_data))

        self.converter.merge_vnfd_config(vnfd, input_data)

        self.converter.add_vnfd_initial_config(
            vnfd.mgmt_interface.vnf_configuration.initial_config_primitive,
            input_data,
        )

    def read_from_file(self, filename):
        with open(filename) as fh:
            input_data = yaml.load(fh)
        return input_data

    def merge_nsd_config(self, nsd):
        nsd_archive = os.path.join(
            os.getenv('RIFT_ARTIFACTS'),
            "launchpad/libs",
            nsd.id,
            "config")

        self._log.info("Looking for config from the archive {}".format(nsd_archive))

        if not os.path.exists(nsd_archive):
            return

        config_file = os.path.join(nsd_archive,
                                   "{}.yaml".format(nsd.id))
        if not os.path.exists(config_file):
            self._log.info("Could not find NS config in archive: %s", config_file)
            return

        input_data = self.read_from_file(config_file)
        self._log.info("Loaded NS config file {}: {}".format(config_file, input_data))

        self.converter.merge_nsd_config(nsd, input_data)

        self.converter.merge_nsd_initial_config(nsd, input_data)
