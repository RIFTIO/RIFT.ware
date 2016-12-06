#!/usr/bin/env python
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
# @file rw_status.py
# @author Justin Broner(Justin.Bronder@riftio.com)
# @author Austin Cormier(Austin.Cormier@riftio.com)
# @date 1/28/2015
# @brief This module defines Python utilities for dealing with rwstatus codes.

import traceback
import functools
import gi
gi.require_version('RwTypes', '1.0')

from gi.repository import RwTypes

def rwstatus_from_exc_map(exc_map):
    """ Creates an rwstatus decorator from a dictionary mapping exception
    types to rwstatus codes.
    """

    # A decorator that maps a Python exception to a particular return code.
    # Automatically returns RW_SUCCESS when no Python exception was thrown.
    # Prevents us from having to use try: except: handlers around every function call.

    # When attempting to call a wrapped function from Python, you can pass in
    # no_rwstatus=True kwarg to prevent the exception from being converted into
    # an rwstatus return value.
    def rwstatus(arg=None, ret_on_failure=None):
        def decorator(func):
            @functools.wraps(func)
            def wrapper(*args, **kwds):
                no_rwstatus = False
                if "no_rwstatus" in kwds:
                    if kwds["no_rwstatus"]:
                        no_rwstatus = True

                    del kwds["no_rwstatus"]

                try:
                    ret = func(*args, **kwds)
                    if no_rwstatus:
                        return ret

                except Exception as e:
                    # If no_rwstatus was provided as a keyword argument and
                    # set to True, raise the exception.
                    if no_rwstatus:
                        raise(e)

                    ret_code = [status for exc, status in exc_map.items() if isinstance(e, exc)]
                    ret_list = [None] if ret_on_failure is None else list(ret_on_failure)
                    if len(ret_code):
                        ret_list.insert(0, ret_code[0])
                        return tuple(ret_list)

                    # If it was not explicitly mapped, print the full traceback as this
                    # is not an anticipated error.
                    traceback.print_exc()
                    ret_list.insert(0, RwTypes.RwStatus.FAILURE)
                    return tuple(ret_list)

                if ret is not None:
                    ret_list = [RwTypes.RwStatus.SUCCESS]
                    if type(ret) == tuple:
                        ret_list.extend(ret)
                    else:
                        ret_list.append(ret)

                    return tuple(ret_list)
                else:
                    return RwTypes.RwStatus.SUCCESS

            return wrapper

        if isinstance(arg, dict):
            exc_map.update(arg)
            return decorator
        elif ret_on_failure is not None:
            return decorator
        else:
            return decorator(arg)

    return rwstatus


