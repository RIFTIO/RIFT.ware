
# STANDARD_RIFT_IO_COPYRIGHT

# Workaround RIFT-6485 - rpmbuild defaults to python2 for
# anything not in a site-packages directory so we have to
# install the plugin implementation in site-packages and then
# import it from the actual plugin.

import rift.tasklets.pytoytasklet

class Tasklet(rift.tasklets.pytoytasklet.ExampleTasklet):
    pass

# vim: sw=4
