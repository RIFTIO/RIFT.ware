#!/usr/bin/python
#
# Expand the bundled cairo-1.0.gir.in files
# for use in Visual C++ builds of G-I
#
# Author: Fan, Chun-wei
# Date: January 20, 2014
#
# (Adapted from setup.py in
# $(glib_src_root)/build/win32/setup.py written by Shixin Zeng)

import os
import sys
import optparse

from gi_msvc_build_utils import process_in
from gi_msvc_build_utils import parent_dir

def setup_vars_tools(module, func, srcfile, outfile):
    vars = {}

    # Well, we are using the "relocatable" feature on Windows...
    blah = 'this\\\\is\\\\ignored\\\\on\\\\windows'
    vars['datarootdir'] = blah
    vars['libdir'] = blah

    # This doesn't really matter for cmd.exe usage, but
    # let's just set this like this here, in case one
    # wants to use MinGW with the scripts generated here
    vars['PYTHON'] = 'python'

    # The parts that really matter.
    vars['TOOL_MODULE'] = module
    vars['TOOL_FUNCTION'] = func
    process_in(srcfile, outfile, vars, 2)

def main(argv):
    modules = ['scannermain','annotationmain','docmain']
    funcs = ['scanner_main','annotation_main','doc_main']
    tools = ['g-ir-scanner','g-ir-annotation-tool','g-ir-doc-tool']

    srcroot = parent_dir(__file__)
    preset_tools_path = os.path.join(srcroot, 'tools')
    src = os.path.join(preset_tools_path, 'g-ir-tool-template.in')

    for i in range(3):
        dest = os.path.join(preset_tools_path, tools[i])
        setup_vars_tools(modules[i], funcs[i], src, dest)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
