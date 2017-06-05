# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Max Beckett
# Creation Date: 8/24/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)


# Workaround RIFT-6485 - rpmbuild defaults to python2 for
# anything not in a site-packages directory so we have to
# install the plugin implementation in site-packages and then
# import it from the actual plugin.

import rift.tasklets.restconf

class Tasklet(rift.tasklets.restconf.RestconfTasklet):
   pass
