
# STANDARD_RIFT_IO_COPYRIGHT

import gi.module
import rift.gi_utils

def wrap_rwmain():
    gi_mod = gi.module.get_introspection_module('RwMain')
    info_cls = gi_mod.Info

    prop_nd_getter = {
        'tasklet_info': gi_mod.Info.get_tasklet_info,
    }
    for prop, getter in prop_nd_getter.items():
        rift.gi_utils.create_property(gi_mod.Info, prop, getter)

    def __str__(self):
        return "RwMain.TaskletInfo=%s" % (self.tasklet_info,)
    gi_mod.Info.__str__ = __str__


wrap_rwmain()

# vim: sw=4
