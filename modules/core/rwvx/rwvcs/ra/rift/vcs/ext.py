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
# @file ext.py
# @author Joshua Downer (joshua.downer@riftio.com)
# @date 2015/05/06
#

This modules contains class extensions that are used by VCS classes.

"""


class ClassProperty(object):
    """
    This class defines a data descriptor specifying class attributes that are
    immutable properties of the class, like a static const in C++.
    """

    def __init__(self, value=None):
        """Create an instance with the specified value"""
        self.value = value

    def __get__(self, instance, owner):
        """Return the value associated with the descriptor"""
        return self.value
