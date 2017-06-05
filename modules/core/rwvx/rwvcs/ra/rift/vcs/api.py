# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file ravcs_api.py
# @author Austin Cormier (austin.cormier@riftio.com)
# @date 06/30/2014
# @brief High-level RA.VCS Python library
# @details The goal of this library is to abstract away many of how we
# interface with RW.VCS and provide a more Pythonic interface than is
# possible using the generated Vala interface.
#
# This provides the foundation for the north-bound API of RA.VCS.
#


import logging
import os
import tempfile

import rw_peas
from xml.dom.minidom import parseString

from gi import require_version
require_version('RwvcsApi', '1.0')

logger = logging.getLogger(__name__)

SYSTEM_READY_TIMEOUT_SECS = 120

SLEEP_AFTER_SYSTEM_READY_SECS = 15


class ManifestStartError(Exception):
    pass


class RaVcsApi(object):
    # Initialize the peas plugin and initialize the component
    _ravcs_plugin = rw_peas.PeasPlugin('rwvcs_api-c', 'RwvcsApi-1.0')
    _engine, _info, _extension = _ravcs_plugin()

    def _manifest_as_xml_string(self, manifest, minimized=False):
        '''
        Returns manifest as an XML string.
        If minimized is True, returns a minimized XML string.
        If minimized is False, returns a nicely formatted (prettified) XML string.
        '''

        xml_data = manifest.as_xml(RaVcsApi._extension)
        if minimized is False:
            dom = parseString(xml_data)
            xml_data = dom.toprettyxml(indent="  ")
        return xml_data

    def exec_rwmain(self, xml_file, use_gdb=False, track_memory=False):
        """
        Execute rwmain to load the provided xml_file.

        Arguments:
            xml_file - The manifest xml_file file on disk to load
            use_gdb - Flag indicating whether to launch rwmain under gdb
            track_memory - Flag indicating whether to launch rwmain with memory-tracking enabled
        """
        logger.debug("Loading manifest file into VCS: %s" % xml_file)
        if not os.path.exists(xml_file):
            raise ManifestStartError("Cannot find manifest xml_file argument: %s" % xml_file)

        # The system only runs from within the .install directory.
        os.chdir(os.environ['RIFT_INSTALL'])

        print("track_memory=", track_memory)
        if track_memory:
            if os.environ.get('LD_PRELOAD') is not None:
                os.environ['LD_PRELOAD'] = str(os.path.join(os.environ['RIFT_INSTALL'], "usr/lib/librwmalloc.so")) + ':' + str(os.environ['LD_PRELOAD'])
            else:
                os.environ['LD_PRELOAD'] = os.path.join(os.environ['RIFT_INSTALL'], "usr/lib/librwmalloc.so")

        rwmain_exe = os.path.join(os.environ['RIFT_INSTALL'], "usr/bin/rwmain")
        if not os.path.isfile(rwmain_exe):
            raise ManifestStartError("{} does not exist!  Cannot start system.".format(rwmain_exe))

        exe = rwmain_exe
        exe_name = "rwmain"
        exe_args = ["--manifest", xml_file]

        # If the user requested to use gdb, lets set gdb as the executable
        # and put the existing rwmain exe and args after --args
        if use_gdb:
            logger.info("Running rwmain under GDB")
            exe_args = ["--args", exe] + exe_args
            exe = "/usr/bin/gdb"
            exe_name = "gdb"

        # Set argv[0] to the appropriate executable name
        exe_args.insert(0, exe_name)

        # Execute rwmain with provided manifest, this never returns
        os.execv(exe, exe_args)

    def manifest_generate_xml(self, manifest, output_filepath=None, manifest_prefix=None):
        """
        Compile the RaManifest object into an XML file.

        Arguments:
            output_filepath - If provided, then write the manifest to the file path provided.
                              The existing file will be overwritten.
        """
        xml_data = self._manifest_as_xml_string(manifest).encode('utf-8')

        # user requested to write the manifest to a specific file
        if output_filepath:
            manifest_hdl = os.open(output_filepath, os.O_RDWR|os.O_CREAT|os.O_TRUNC)
        else:
            # Create a temporary file to store the manifest data
            prefix = "manifest_"
            if manifest_prefix:
                prefix=manifest_prefix + "_" + prefix
            (manifest_hdl, output_filepath) = tempfile.mkstemp(".xml", prefix)

        # Write the xml data and close the manifest file handle
        os.write(manifest_hdl, xml_data)
        os.close(manifest_hdl)

        logger.debug("Wrote Manifest XML File: %s" % output_filepath)

        return output_filepath

    def manifest_start(self, manifest):
        """
        Compile the RaManifest object into an XML file, load the file into RW.VCS, and optionally
        start the init component of the manifest.
        """
        manifest_file = self.manifest_generate_xml(manifest)
        self.exec_rwmain(manifest_file)


