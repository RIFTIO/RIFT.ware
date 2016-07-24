# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab
#
# Copyright (C) 2005-2009 Johan Dahlin <johan@gnome.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
# USA

from __future__ import absolute_import

# support overrides in different directories than our gi module
from pkgutil import extend_path
__path__ = extend_path(__path__, __name__)

import sys
import os
import importlib
import types

_static_binding_error = ('When using gi.repository you must not import static '
                         'modules like "gobject". Please change all occurrences '
                         'of "import gobject" to "from gi.repository import GObject". '
                         'See: https://bugzilla.gnome.org/show_bug.cgi?id=709183')

# we can't have pygobject 2 loaded at the same time we load the internal _gobject
if 'gobject' in sys.modules:
    raise ImportError(_static_binding_error)


from . import _gi
from ._gi import _gobject
from ._gi import _API
from ._gi import Repository
from ._gi import PyGIDeprecationWarning
from ._gi import PyGIWarning

_API = _API  # pyflakes
PyGIDeprecationWarning = PyGIDeprecationWarning
PyGIWarning = PyGIWarning

_versions = {}
_overridesdir = os.path.join(os.path.dirname(__file__), 'overrides')

version_info = _gobject.pygobject_version[:]
__version__ = "{0}.{1}.{2}".format(*version_info)


class _DummyStaticModule(types.ModuleType):
    __path__ = None

    def __getattr__(self, name):
        raise AttributeError(_static_binding_error)


sys.modules['glib'] = _DummyStaticModule('glib', _static_binding_error)
sys.modules['gobject'] = _DummyStaticModule('gobject', _static_binding_error)
sys.modules['gio'] = _DummyStaticModule('gio', _static_binding_error)
sys.modules['gtk'] = _DummyStaticModule('gtk', _static_binding_error)
sys.modules['gtk.gdk'] = _DummyStaticModule('gtk.gdk', _static_binding_error)


def check_version(version):
    if isinstance(version, str):
        version_list = tuple(map(int, version.split(".")))
    else:
        version_list = version

    if version_list > version_info:
        raise ValueError((
            "pygobject's version %s required, and available version "
            "%s is not recent enough") % (version, __version__)
        )


def require_version(namespace, version):
    repository = Repository.get_default()

    if namespace in repository.get_loaded_namespaces():
        loaded_version = repository.get_version(namespace)
        if loaded_version != version:
            raise ValueError('Namespace %s is already loaded with version %s' %
                             (namespace, loaded_version))

    if namespace in _versions and _versions[namespace] != version:
        raise ValueError('Namespace %s already requires version %s' %
                         (namespace, _versions[namespace]))

    available_versions = repository.enumerate_versions(namespace)
    if not available_versions:
        raise ValueError('Namespace %s not available' % namespace)

    if version not in available_versions:
        raise ValueError('Namespace %s not available for version %s' %
                         (namespace, version))

    _versions[namespace] = version


def get_required_version(namespace):
    return _versions.get(namespace, None)


def require_foreign(namespace, symbol=None):
    """Ensure the given foreign marshaling module is available and loaded.

    :param str namespace:
        Introspection namespace of the foreign module (e.g. "cairo")
    :param symbol:
        Optional symbol typename to ensure a converter exists.
    :type symbol: str or None
    :raises: ImportError

    :Example:

    .. code-block:: python

        import gi
        import cairo
        gi.require_foreign('cairo')

    """
    try:
        _gi.require_foreign(namespace, symbol)
    except Exception as e:
        raise ImportError(str(e))
    importlib.import_module('gi.repository', namespace)
