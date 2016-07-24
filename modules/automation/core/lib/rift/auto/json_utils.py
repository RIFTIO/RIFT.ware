# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Karun Ganesharatnam
# Creation Date: 01/05/2016
# 
#
# JASON functions
import difflib
import json
import xmltodict
import xmltodict

from functools import cmp_to_key
from lxml import etree

def pretty_json(json_data):
    """Creates a more human readable json string by using
    new lines and indentation where appropriate.

    Arguments:
        json_data - json string to be pretty formated.
    """
    toeval = str(json_data)

    result = json.dumps(eval(toeval),
                          sort_keys=True,
                          indent=4,
                          separators=(',', ': '))

    # data type translations from python format to json format
    return result


def sorted_list(obj):
    """
    This is to perform similar operation of sorted in python 2.x which uses
    the cmp() function which is removed in python 3.x. This does not
    provide complete functionality provided in python 2.x sorted()
    method, but it does what is needed in order to compare json
    ignoring the order of elements

    Arguments:
        obj - object to be sorted
    """
    def compare(a, b):
        x = repr(a)
        y = repr(b)
        return (x > y) - (x < y)
    try:
        return sorted(obj)
    except TypeError:
        return sorted(obj, key=cmp_to_key(compare))

def re_order_json(obj):
    """This function will traverse through json data recursively and sort
    items of any dictionary or list based on the key and values

    Arguments:
        obj - json object to be ordered.

    Return:
        ordered json object
    """
    if isinstance(obj, dict):
        return sorted_list((k, re_order_json(v)) for k, v in obj.items())
    if isinstance(obj, list):
        try:
            return sorted_list(re_order_json(x) for x in obj)
        except Exception:
            print(pretty_json(obj))
            print("=" * 100)
            raise
    else:
        return obj

def ordered_json(json_data, from_file=False):
    """To re-arrange jaso data in sorted format

    Arguments:
        json_data - json string, object or file contains the json data
        from_file - Flag denotes whether the json content is in the file
                    provided as argument "json_content"

    Return:
        ordered json object
    """
    json_str = json_data
    if from_file:
        with open(json_content) as js:
            json_str = js.read()
            js.close()
    elif type(json_data) is not str:
        json_str = json.dumps(json_data)

    jsondata = json.loads(json_str)
    return re_order_json(jsondata)

def compare_json(json_1, json_2, ignore_order=True):
    """This function will generate a string showing the differences
       between two json data (similar to the Linux 'diff' command output)

    Arguments:
       ignore_order - Flag denotes whether to ignore the order of
                 dictionary keys and list items

    Arguments:
        json_1, json_2 - json contents (strings or json objects) to be compared

    Return:
        Boolean indicating where the contents of the json data are matched
    """
    if ignore_order:
        json_1 = ordered_json(json_1)
        json_2 = ordered_json(json_2)

    return json_1 == json_2

def json_diff(json_1, json_2, ignore_order=True):
    """This function will generate string showing the differences
       between two json data (similar to the Linux 'diff' command output)
       ignore_order - Flag denotes whether to ignore the
                   order of dictionary keys and list items

    Arguments:
        json_1, json_2 - json contents (strings or json objects) to be compared

    Return:
        String show the differences
    """
    if ignore_order:
        json_1 = ordered_json(json_1)
        json_2 = ordered_json(json_2)

    pp1 = pretty_json(json_1)
    pp2 = pretty_json(json_2)

    pp_lst1 = pp1.split('\n')
    pp_lst2 = pp2.split('\n')

    diff_lines = [line for line in difflib.context_diff(pp_lst1, pp_lst2)]
    return '\n'.join(diff_lines)
