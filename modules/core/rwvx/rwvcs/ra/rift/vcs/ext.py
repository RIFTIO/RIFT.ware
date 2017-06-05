"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
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
