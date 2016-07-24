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

'Boilerplate ugliness'

from ncclient.xml_ import *
from ncclient.operations.errors import OperationError, MissingCapabilityError

import re
import time
import datetime

__all__ = [
    "one_of",
    "datastore_or_url",
    "build_filter",
    "parse_datetime",
    "datetime_to_string",
    "datetime_now",
]

def one_of(*args):
    "Verifies that only one of the arguments is not None"
    for i, arg in enumerate(args):
        if arg is not None:
            for argh in args[i+1:]:
                if argh is not None:
                    raise OperationError("Too many parameters")
            else:
                return
    raise OperationError("Insufficient parameters")

def datastore_or_url(wha, loc, capcheck=None):
    node = new_ele(wha)
    if "://" in loc: # e.g. http://, file://, ftp://
        if capcheck is not None:
            capcheck(":url") # url schema check at some point!
            sub_ele(node, "url").text = loc
    else:
        #if loc == 'candidate':
        #    capcheck(':candidate')
        #elif loc == 'startup':
        #    capcheck(':startup')
        #elif loc == 'running' and wha == 'target':
        #    capcheck(':writable-running')
        sub_ele(node, loc)
    return node

def build_filter(spec, capcheck=None):
    type = None
    if isinstance(spec, tuple):
        type, criteria = spec
        rep = new_ele("filter", type=type)
        if type == "xpath":
            rep.attrib["select"] = criteria
        elif type == "subtree":
            rep.append(to_ele(criteria))
        else:
            raise OperationError("Invalid filter type")
    else:

        rep = validated_element(spec, ("filter", qualify("filter")))
        # results in XMLError: line 105 ncclient/xml_.py - commented by earies - 5/10/13
        #rep = validated_element(spec, ("filter", qualify("filter")),
        #                                attrs=("type",))
        # TODO set type var here, check if select attr present in case of xpath..
    if type == "xpath" and capcheck is not None:
        capcheck(":xpath")
    return rep

#
# The following methods deals with RFC3339 time also known as Internet time.
# RFC3339 is the defacto time format used in the Netconf RFC. This time format
# is timezone aware and is a subset of ISO8601 Time. 

# Regular expression for Date, Seperator and Time
_date_re_str = r'(\d\d\d\d)-(\d\d)-(\d\d)'
_dt_sep_str  = r'[ tT]'
_time_re_str = r'(\d\d):(\d\d):(\d\d)(\.(\d+))?([zZ]|(([-+])(\d\d):?(\d\d)))?'

# Compiled regular expression for the RFC3339 time representation
_datetime_re = re.compile(r'^\s*' + 
                    _date_re_str + 
                    _dt_sep_str + 
                    _time_re_str +
                    r'\s*$')

def parse_datetime(time_str):
    """Returns a datetime for a RFC3339 time representation

    The returned datetime is timezone aware. This method is required to handle
    the finer aspects of RFC3339 - fraction seconds and the timezone Z
    representation.
    """
    match = _datetime_re.match(time_str)
    if match is None:
        raise OperationError('Invalid Internet Time Format {}'.format(time_str))
    
    yyyy, mm, dd, hh, mins, sec, _, ms, utc_z, _, tz_sign, tz_hh, tz_min = match.groups()
    micro_sec = 0
    if ms:
        micro_sec = int(float('0.' + ms) * 1000000)

    if utc_z in ['z', 'Z']:
        tz = datetime.timezone.utc
    else:
        tz_hh = int(tz_hh) if tz_hh is not None else 0
        tz_min = int(tz_min) if tz_min is not None else 0
        if tz_hh > 24 or tz_min > 60:
            raise OperationError('Invalid Internet Time Offset {}'.format(time_str))

        tz_offset = tz_hh * 60 + tz_min
        if tz_offset == 0:
            tz = datetime.timezone.utc
        else:
            if tz_sign == '-':
                tz_offset = -tz_offset
            tz = datetime.timezone(datetime.timedelta(minutes=tz_offset))

    return datetime.datetime(int(yyyy), int(mm), int(dd), int(hh), int(mins),
                             int(sec), micro_sec, tz)

def datetime_to_string(dt):
    """Returns RFC3339 representation of datetime object."""
    if dt.utcoffset() is not None:
        dt_str = dt.isoformat()
    else:
        dt_str = "{}Z".format(dt.isoformat())
    return dt_str

def datetime_now():
    """Returns timezone aware current datetime.

    The time returned by the datetime.now() is not timezone aware. This method
    computes the offset time from UTC in the current locale and adds to the
    datetime.
    """
    now_ts = time.time()
    tz_offset = datetime.datetime.fromtimestamp(now_ts) - datetime.datetime.utcfromtimestamp(now_ts)
    dt_now = datetime.datetime.now()
    dt_now = dt_now.replace(tzinfo=datetime.timezone(tz_offset))
    return dt_now
