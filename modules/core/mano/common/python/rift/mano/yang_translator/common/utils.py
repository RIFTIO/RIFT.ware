import gettext
import numbers
import os

from six.moves.urllib.parse import urlparse

import yaml

_localedir = os.environ.get('yang-translator'.upper() + '_LOCALEDIR')
_t = gettext.translation('yang-translator', localedir=_localedir,
                         fallback=True)


def _(msg):
    return _t.gettext(msg)


class CompareUtils(object):

    MISMATCH_VALUE1_LABEL = "<Expected>"
    MISMATCH_VALUE2_LABEL = "<Provided>"
    ORDERLESS_LIST_KEYS = ['allowed_values', 'depends_on']

    @staticmethod
    def compare_dicts(dict1, dict2, log):
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


def str_to_num(value):
    """Convert a string representation of a number into a numeric type."""
    if isinstance(value, numbers.Number):
        return value
    try:
        return int(value)
    except ValueError:
        return float(value)


def map_name_to_python(name):
    if name == 'type':
        return 'type_yang'
    return name.replace('-', '_')


def convert_keys_to_python(d):
    '''Change all keys from - to _'''
    if isinstance(d, dict):
        for key in d.keys():
            d[map_name_to_python(key)] = convert_keys_to_python(d.pop(key))
        return d
    elif isinstance(d, list):
        arr = []
        for memb in d:
            arr.append(convert_keys_to_python(memb))
        return arr
    else:
        return d

def map_name_to_yang(name):
    return name.replace('_', '-')


def convert_keys_to_yang(d):
    '''Change all keys from _ to -'''
    if isinstance(d, dict):
        for key in d.keys():
            d[map_name_to_python(key)] = convert_keys_to_yang(d.pop(key))
        return d
    elif isinstance(d, list):
        arr = []
        for memb in d:
            arr.append(convert_keys_to_yang(memb))
        return arr
    else:
        return d


def stringify_dict(d):
    '''Convert all integer, float, etc to str'''
    if isinstance(d, dict):
        for key in d.keys():
                d[key] = stringify_dict(d[key])
        return d
    elif isinstance(d, list):
        arr = []
        for memb in d:
            arr.append(stringify_dict(memb))
        return arr
    else:
        if not isinstance(d, str):
            return str(d)
        return d
