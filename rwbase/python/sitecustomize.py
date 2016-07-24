
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# This is required to make sure that python will pick up path configuration files
# located in the parent directory.  Without this, external projects which use
# path files will fail to work.  For instance, python-protobuf.

import os
import site
import sys

# Deal with rift-shell.
if sys.version_info < (3, 0):
    site.addsitedir(os.path.dirname(__file__))
else:
    # rift-shell is assuming that we're running python2.7, support running
    # python3.x as well.  Someone else can deal with python2.x for x != 7.
    rift_root = os.environ.get('RIFT_ROOT')
    if rift_root != None:
        rift_paths = [p for p in sys.path if rift_root in p]
        [sys.path.remove(p) for p in rift_paths]

        py27_paths = [p for p in sys.path if 'python2.7' in p]
        [sys.path.remove(p) for p in py27_paths]

        rift_paths.reverse()
        for path in rift_paths:
            path = path.replace('python2.7', 'python%d.%d' % sys.version_info[:2])
            sys.path.insert(0, path)

    sitedir = os.path.dirname(__file__)
    sitedir = sitedir.replace('python2.7', 'python%d.%d' % sys.version_info[:2])
    site.addsitedir(sitedir)

# vim: sw=4
