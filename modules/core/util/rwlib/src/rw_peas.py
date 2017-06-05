# STANDARD_RIFT_IO_COPYRIGHT #

# @file rw_peas.py
# @author Austin Cormier (austin.cormier@riftio.com)
# @date 06/19/2014
# @brief Functions to create and load libpeas plugins

import gi
import importlib
import logging
import os
import warnings
gi.require_version('Peas', '1.0')
gi.require_version('YangModelPlugin', '1.0')

from gi.repository import GObject, Peas, GLib, GIRepository

logger = logging.getLogger(__name__)

DEFAULT_TYPELIBS = 'RwTypes-1.0', 'Rwplugin-1.0'
PLUGIN_LOADER_TYPE = {
    '-c'  : 'c',
    '-py' : 'python3',
    '-python' : 'python3',
    '-plugin' : 'python3',
    '_c'  : 'c',
    '_py' : 'python3',
    '_python' : 'python3',
    '_plugin' : 'python3',
}

def add_typelibs(typelibs):
    '''
    Add typelib names specified in "typelibs" sequence (list, tuple. set) to the default
    GI typelib repository. Every name must be a name of the .typelib file (without the extension),
    i.e. a string that includes a trailing version separated by a dash.

    Example: add_typelibs(('RwvcsApi-1.0', 'RwCal-1.0'))
    '''
    default = GIRepository.Repository.get_default()
    for typelib in typelibs:
        name, version = typelib.rsplit('-', 1)
        GIRepository.Repository.require(default, name, version, 0)

def create_engine(GI_TYPELIB_PATH=True, PLUGINDIR=True):
    """
    Create a libpeas engine with the search paths specified in the GI_TYPELIB_PATH
    and/or PLUGINDIR environment and add the default required typelibs (DEFAULT_TYPELIBS).

    Note this procedure builds a union of paths specified in GI_TYPELIB_PATH and/or
    PLUGINDIR with no guarantee of a particular order of the paths.

    Arguments:

    GI_TYPELIB_PATH - if True, add all search paths from the GI_TYPELIB_PATH environment;
                      if a string or a list of strings, should specify path(s) to be added
                      to the engine's search paths; if False or None, nothing from this
                      environment will be added
    PLUGINDIR       - if True, add all search paths from the PLUGINDIR environment;
                      if a string or a list of strings, should specify path(s) to be added
                      to the engine's search paths; if False or None, nothing from this
                      environment will be added

    Return: the engine object.
    """
    engine = Peas.Engine.get_default()

    paths  = set([])
    for var in 'GI_TYPELIB_PATH', 'PLUGINDIR':
        val = locals()[var]
        if val is True:
            try:
                paths = paths.union(os.environ[var].split(":"))
            except:
                raise Exception('[create_engine] {} is not in the environment'.format(var))
        elif val:
            if isinstance(val, str):
                val = val.split(':')
            if not isinstance(val, list):
                raise Exception('[create_engine] Invalid format of {}'.format(var))
            paths = paths.union(val)

    for path in paths:
        engine.add_search_path(path, path)

    add_typelibs(DEFAULT_TYPELIBS)
    return engine

def load_plugin(engine, plugin_name, typelib):
    """
    This function loads a specified plugin into peas framework
    and returns plugin info. Both 'plugin_name' and 'typelib'
    arguments should be strings.

    Return: the plugin info

    Example:

        info = load_plugin(engine, 'rwtrace', 'Rwtrace-1.0')
    """
    add_typelibs([typelib])
    info = engine.get_plugin_info(plugin_name)
    if info is None:
        raise AttributeError('Plugin "%s" not found' % (plugin_name,))
    engine.load_plugin(info)
    return info

class PeasPlugin(object):
    '''
    Class that initializes a plugin and stores relevant data.

    Data members:

    plugin_name           - string that keeps input argument "plugin_name" (e.g. 'rwvcs_api-c')
    typelib_filename      - string that keeps input argument "typelib_filename" (e.g. 'RwvcsApi-1.0')

    typelib_name          - typelib name without the version (e.g. 'RwvcsApi')
    typelib_version       - typelib version (e.g. '1.0')
    typelib               - typelib module object which has the "Api" and "ApiIface"
                            members (among the others)
    typelib_api           - typelib.Api object or None if the plugin does not have an Api
                            interface

    engine                - the libpeas engine object
    info                  - plugin info or None if the plugin does not have an Api interface
    plugin                - the plugin object initialized via libpease engine
                            or none if the plugin does not have an Api
                            interface

    Example:
    my_plugin = PeasPlugin('rwvcs_api-c', 'RwvcsApi-1.0')
    '''
    def __init__(self, plugin_name, typelib_filename, engine=None):
        '''
        Input arguments:

        plugin_name      - a plugin implementation name (example: "rwvcs_api-c");
                           usually it is a name of a subdirectory (inside /plugins/)
                           where the plugin source files reside (.gschema.xml, .plugin, etc.)
        typelib_filename - a name of the typelib file (without the .typelib file extension)
                           with the Vala API for the plugin (example: "RwvcsApi-1.0")
        engine           - a libpeas engine object (if it already exists); by default a new
                           engine object will be created if not passed in via this argument

        '''
        logger.debug('Initializing plugin "{}", {}'.format(plugin_name, typelib_filename))

        self.plugin_name      = plugin_name
        self.typelib_filename = typelib_filename

        self.engine = create_engine() if engine is None else engine

        # Enable language loader for this plugin
        loader_type = 'python3'
        for suff in PLUGIN_LOADER_TYPE:
            if plugin_name.endswith(suff):
                loader_type = PLUGIN_LOADER_TYPE[suff]
                break

        self.engine.enable_loader(loader_type)

        try:
            self.typelib_name, self.typelib_version = typelib_filename.rsplit('-', 1)
        except:
            raise SystemExit('[rw_peas.PeasPlugin] Typelib filename "{}" is missing version'.format(typelib_filename))

        # Catch any import warnings from PyGI and adjust the stacklevel
        # so it points to application code for easier analysis
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always", gi.PyGIWarning)
            self.typelib = importlib.import_module('.{}'.format(self.typelib_name), 'gi.repository')

        for warning in w:
            warnings.warn(warning.message, warning.category, stacklevel=2)

        # This is nuts but the dynamic module is not actually fully initialized until after getattr() is called on
        # well, basically anything.  We need the module to fully initialize so that an honest attempt to load
        # the plugin can be made.
        _ = getattr(self.typelib, dir(self.typelib)[0])

        try:
            self.info = load_plugin(self.engine, self.plugin_name, self.typelib_filename)
        except TypeError as e:
            self.info = None

        # Not every plugin has an Api interface.  For instance, rwtypes
        try:
            self.typelib_api = self.typelib.Api
            self.plugin = self.engine.create_extension(self.info, self.typelib_api, None)
        except AttributeError:
            self.typelib_api = None
            self.plugin = None

    def __call__(self):
        '''
        This is a shortcut convenience method that returns 3 data members:
            - engine
            - info
            - plugin
        '''
        return self.engine, self.info, self.plugin

    def enable_loader(self, loader_type='python'):
        '''
        Enable engine loader of a specified type: python, c, etc.
        '''
        self.engine.enable_loader(loader_type)

    def get_interface(self, name):
        '''
        Get an interface from the plugin typelib.

        @param name - name of the interface to retreive
        @return     - handle to the specified interface
        '''
        return self.engine.create_extension(self.info, getattr(self.typelib, name), None)

