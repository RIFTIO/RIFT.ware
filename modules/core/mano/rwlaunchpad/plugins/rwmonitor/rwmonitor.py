
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# Workaround RIFT-6485 - rpmbuild defaults to python2 for
# anything not in a site-packages directory so we have to
# install the plugin implementation in site-packages and then
# import it from the actual plugin.

import rift.tasklets.rwmonitor

class Tasklet(rift.tasklets.rwmonitor.MonitorTasklet):
    pass

# vim: sw=4
