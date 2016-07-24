#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#
#    Copyright 2016 RIFT.io Inc


import gettext
import json
import logging
import math
import numbers
import os
import re
import requests
from six.moves.urllib.parse import urlparse
import yaml

from hashlib import md5
from hashlib import sha256

import toscaparser.utils.yamlparser

_localedir = os.environ.get('tosca-translator'.upper() + '_LOCALEDIR')
_t = gettext.translation('tosca-translator', localedir=_localedir,
                         fallback=True)


def _(msg):
    return _t.gettext(msg)


YAML_ORDER_PARSER = toscaparser.utils.yamlparser.simple_ordered_parse
log = logging.getLogger('tosca-translator')

# Required environment variables to create openstackclient object.
ENV_VARIABLES = ['OS_AUTH_URL', 'OS_PASSWORD', 'OS_USERNAME', 'OS_TENANT_NAME']


class MemoryUnit(object):

    UNIT_SIZE_DEFAULT = 'B'
    UNIT_SIZE_DICT = {'B': 1, 'kB': 1000, 'KiB': 1024, 'MB': 1000000,
                      'MiB': 1048576, 'GB': 1000000000,
                      'GiB': 1073741824, 'TB': 1000000000000,
                      'TiB': 1099511627776}

    @staticmethod
    def convert_unit_size_to_num(size, unit=None):
        """Convert given size to a number representing given unit.

        If unit is None, convert to a number representing UNIT_SIZE_DEFAULT
        :param size: unit size e.g. 1 TB
        :param unit: unit to be converted to e.g GB
        :return: converted number e.g. 1000 for 1 TB size and unit GB
        """
        if unit:
            unit = MemoryUnit.validate_unit(unit)
        else:
            unit = MemoryUnit.UNIT_SIZE_DEFAULT
            log.info(_('A memory unit is not provided for size; using the '
                       'default unit %(default)s.') % {'default': 'B'})
        regex = re.compile('(\d*)\s*(\w*)')
        result = regex.match(str(size)).groups()
        if result[1]:
            unit_size = MemoryUnit.validate_unit(result[1])
            converted = int(str_to_num(result[0])
                            * MemoryUnit.UNIT_SIZE_DICT[unit_size]
                            * math.pow(MemoryUnit.UNIT_SIZE_DICT
                                       [unit], -1))
            log.info(_('Given size %(size)s is converted to %(num)s '
                       '%(unit)s.') % {'size': size,
                     'num': converted, 'unit': unit})
        else:
            converted = (str_to_num(result[0]))
        return converted

    @staticmethod
    def validate_unit(unit):
        if unit in MemoryUnit.UNIT_SIZE_DICT.keys():
            return unit
        else:
            for key in MemoryUnit.UNIT_SIZE_DICT.keys():
                if key.upper() == unit.upper():
                    return key

            msg = _('Provided unit "{0}" is not valid. The valid units are'
                    ' {1}').format(unit, MemoryUnit.UNIT_SIZE_DICT.keys())
            log.error(msg)
            raise ValueError(msg)


class CompareUtils(object):

    MISMATCH_VALUE1_LABEL = "<Expected>"
    MISMATCH_VALUE2_LABEL = "<Provided>"
    ORDERLESS_LIST_KEYS = ['allowed_values', 'depends_on']

    @staticmethod
    def compare_dicts(dict1, dict2):
        """Return False if not equal, True if both are equal."""

        if dict1 is None and dict2 is None:
            return True
        if dict1 is None or dict2 is None:
            return False

        both_equal = True
        for dict1_item, dict2_item in zip(dict1.items(), dict2.items()):
            if dict1_item != dict2_item:
                msg = (_("%(label1)s: %(item1)s \n is not equal to \n:"
                         "%(label2)s: %(item2)s")
                       % {'label1': CompareUtils.MISMATCH_VALUE2_LABEL,
                          'item1': dict1_item,
                          'label2': CompareUtils.MISMATCH_VALUE1_LABEL,
                          'item2': dict2_item})
                log.warning(msg)
                both_equal = False
                break
        return both_equal

    @staticmethod
    def compare_mano_yamls(generated_yaml, expected_yaml):
        mano_translated_dict = YAML_ORDER_PARSER(generated_yaml)
        mano_expected_dict = YAML_ORDER_PARSER(expected_yaml)
        return CompareUtils.compare_dicts(mano_translated_dict,
                                          mano_expected_dict)

    @staticmethod
    def reorder(dic):
        '''Canonicalize list items in the dictionary for ease of comparison.

        For properties whose value is a list in which the order does not
        matter, some pre-processing is required to bring those lists into a
        canonical format. We use sorting just to make sure such differences
        in ordering would not cause to a mismatch.
        '''

        if type(dic) is not dict:
            return None

        reordered = {}
        for key in dic.keys():
            value = dic[key]
            if type(value) is dict:
                reordered[key] = CompareUtils.reorder(value)
            elif type(value) is list \
                and key in CompareUtils.ORDERLESS_LIST_KEYS:
                reordered[key] = sorted(value)
            else:
                reordered[key] = value
        return reordered

    @staticmethod
    def diff_dicts(dict1, dict2, reorder=True):
        '''Compares two dictionaries and returns their differences.

        Returns a dictionary of mismatches between the two dictionaries.
        An empty dictionary is returned if two dictionaries are equivalent.
        The reorder parameter indicates whether reordering is required
        before comparison or not.
        '''

        if reorder:
            dict1 = CompareUtils.reorder(dict1)
            dict2 = CompareUtils.reorder(dict2)

        if dict1 is None and dict2 is None:
            return {}
        if dict1 is None or dict2 is None:
            return {CompareUtils.MISMATCH_VALUE1_LABEL: dict1,
                    CompareUtils.MISMATCH_VALUE2_LABEL: dict2}

        diff = {}
        keys1 = set(dict1.keys())
        keys2 = set(dict2.keys())
        for key in keys1.union(keys2):
            if key in keys1 and key not in keys2:
                diff[key] = {CompareUtils.MISMATCH_VALUE1_LABEL: dict1[key],
                             CompareUtils.MISMATCH_VALUE2_LABEL: None}
            elif key not in keys1 and key in keys2:
                diff[key] = {CompareUtils.MISMATCH_VALUE1_LABEL: None,
                             CompareUtils.MISMATCH_VALUE2_LABEL: dict2[key]}
            else:
                val1 = dict1[key]
                val2 = dict2[key]
                if val1 != val2:
                    if type(val1) is dict and type(val2) is dict:
                        diff[key] = CompareUtils.diff_dicts(val1, val2, False)
                    else:
                        diff[key] = {CompareUtils.MISMATCH_VALUE1_LABEL: val1,
                                     CompareUtils.MISMATCH_VALUE2_LABEL: val2}
        return diff


class YamlUtils(object):

    @staticmethod
    def get_dict(yaml_file):
        '''Returns the dictionary representation of the given YAML spec.'''
        try:
            return yaml.load(open(yaml_file))
        except IOError:
            return None

    @staticmethod
    def compare_yamls(yaml1_file, yaml2_file):
        '''Returns true if two dictionaries are equivalent, false otherwise.'''
        dict1 = YamlUtils.get_dict(yaml1_file)
        dict2 = YamlUtils.get_dict(yaml2_file)
        return CompareUtils.compare_dicts(dict1, dict2)

    @staticmethod
    def compare_yaml_dict(yaml_file, dic):
        '''Returns true if yaml matches the dictionary, false otherwise.'''
        return CompareUtils.compare_dicts(YamlUtils.get_dict(yaml_file), dic)


class TranslationUtils(object):

    @staticmethod
    def compare_tosca_translation_with_mano(tosca_file, mano_file, params):
        '''Verify tosca translation against the given mano specification.

        inputs:
        tosca_file: relative local path or URL to the tosca input file
        mano_file: relative path to expected mano output
        params: dictionary of parameter name value pairs

        Returns as a dictionary the difference between the MANO translation
        of the given tosca_file and the given mano_file.
        '''

        from toscaparser.tosca_template import ToscaTemplate
        from tosca_translator.mano.tosca_translator import TOSCATranslator

        tosca_tpl = os.path.normpath(os.path.join(
            os.path.dirname(os.path.abspath(__file__)), tosca_file))
        a_file = os.path.isfile(tosca_tpl)
        if not a_file:
            tosca_tpl = tosca_file

        expected_mano_tpl = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), mano_file)

        tosca = ToscaTemplate(tosca_tpl, params, a_file)
        translate = TOSCATranslator(tosca, params)

        output = translate.translate()
        output_dict = toscaparser.utils.yamlparser.simple_parse(output)
        expected_output_dict = YamlUtils.get_dict(expected_mano_tpl)
        return CompareUtils.diff_dicts(output_dict, expected_output_dict)


class UrlUtils(object):

    @staticmethod
    def validate_url(path):
        """Validates whether the given path is a URL or not.

        If the given path includes a scheme (http, https, ftp, ...) and a net
        location (a domain name such as www.github.com) it is validated as a
        URL.
        """
        parsed = urlparse(path)
        return bool(parsed.scheme) and bool(parsed.netloc)


class ChecksumUtils(object):

    @staticmethod
    def get_md5(input_file_name, log=None):
        chunk_size = 1048576  # 1024 B * 1024 B = 1048576 B = 1 MB
        file_md5_checksum = md5()
        try:
            with open(input_file_name, "rb") as f:
                byte = f.read(chunk_size)
                # previous_byte = byte
                byte_size = len(byte)
                file_read_iterations = 1
                while byte:
                    file_md5_checksum.update(byte)
                    # previous_byte = byte
                    byte = f.read(chunk_size)
                    byte_size += len(byte)
                    file_read_iterations += 1

            cksum = file_md5_checksum.hexdigest()
            if log:
                log.debug(_("MD5 for {0} with size {1} (iter:{2}): {3}").
                          format(input_file_name, byte_size,
                                 file_read_iterations, cksum))
            return cksum
        except IOError:
            if log:
                log.error(_('File could not be opened: {0}').
                          format(input_file_name))
                return
            else:
                raise
        except Exception as e:
            raise e

    @staticmethod
    def get_sha256(input_file_name, log=None):
        chunk_size = 1048576  # 1024 B * 1024 B = 1048576 B = 1 MB
        file_sha256_checksum = sha256()
        try:
            with open(input_file_name, "rb") as f:
                byte = f.read(chunk_size)
                # previous_byte = byte
                byte_size = len(byte)
                file_read_iterations = 1
                while byte:
                    file_sha256_checksum.update(byte)
                    # previous_byte = byte
                    byte = f.read(chunk_size)
                    byte_size += len(byte)
                    file_read_iterations += 1

            cksum = file_sha256_checksum.hexdigest()
            if log:
                log.debug(_("SHA256 for {0} with size {1} (iter:{2}): {3}").
                          format(input_file_name, byte_size,
                                 file_read_iterations, cksum))
            return cksum
        except IOError:
            if log:
                log.error(_('File could not be opened: {0}').
                          format(input_file_name))
                return
            else:
                raise
        except Exception as e:
            raise e


def str_to_num(value):
    """Convert a string representation of a number into a numeric type."""
    if isinstance(value, numbers.Number):
        return value
    try:
        return int(value)
    except ValueError:
        return float(value)


def check_for_env_variables():
    return set(ENV_VARIABLES) < set(os.environ.keys())


def get_ks_access_dict():
    tenant_name = os.getenv('OS_TENANT_NAME')
    username = os.getenv('OS_USERNAME')
    password = os.getenv('OS_PASSWORD')
    auth_url = os.getenv('OS_AUTH_URL')

    auth_dict = {
        "auth": {
            "tenantName": tenant_name,
            "passwordCredentials": {
                "username": username,
                "password": password
            }
        }
    }
    headers = {'Content-Type': 'application/json'}
    try:
        keystone_response = requests.post(auth_url + '/tokens',
                                          data=json.dumps(auth_dict),
                                          headers=headers)
        if keystone_response.status_code != 200:
            return None
        return json.loads(keystone_response.content)
    except Exception:
        return None


def get_url_for(access_dict, service_type):
    if access_dict is None:
        return None
    service_catalog = access_dict['access']['serviceCatalog']
    service_url = ''
    for service in service_catalog:
        if service['type'] == service_type:
            service_url = service['endpoints'][0]['publicURL']
            break
    return service_url


def get_token_id(access_dict):
    if access_dict is None:
        return None
    return access_dict['access']['token']['id']


def map_name_to_python(name):
    if name == 'type':
        return 'type_yang'
    return name.replace('-', '_')

def convert_keys_to_python(d):
    '''Change all keys from - to _'''
    if isinstance(d, dict):
        dic = {}
        for key in d.keys():
            dic[map_name_to_python(key)] = convert_keys_to_python(d[key])
        return dic
    elif isinstance(d, list):
        arr = []
        for memb in d:
            arr.append(convert_keys_to_python(memb))
        return arr
    else:
        return d

def map_name_to_yang (name):
    return name.replace('_', '-')

def convert_keys_to_yang(d):
    '''Change all keys from _ to -'''
    if isinstance(d, dict):
        dic = {}
        for key in d.keys():
            dic[map_name_to_python(key)] = convert_keys_to_yang(d[key])
        return dic
    elif isinstance(d, list):
        arr = []
        for memb in d:
            arr.append(convert_keys_to_yang(memb))
        return arr
    else:
        return d


def dict_convert_values_to_str(d):
    '''Convert all leaf values to str'''
    if isinstance(d, dict):
        for key in d.keys():
            d[key] = dict_convert_values_to_str(d[key])
        return d
    elif isinstance(d, list):
        arr = []
        for memb in d:
            arr.append(dict_convert_values_to_str(memb))
        return arr
    else:
        return str(d)
