
# STANDARD_RIFT_IO_COPYRIGHT

import gi.module
import rift.gi_utils

def wrap_rwtasklet():
    gi_mod = gi.module.get_introspection_module('RwTasklet')
    info_cls = gi_mod.Info

    prop_nd_getter = {
        'instance_name': gi_mod.Info.get_instance_name,
        'parent_vm_instance_name': gi_mod.Info.get_parent_vm_instance_name,
        'rwsched_instance': gi_mod.Info.get_rwsched_instance,
        'rwsched_tasklet': gi_mod.Info.get_rwsched_tasklet,
    }
    for prop, getter in prop_nd_getter.items():
        rift.gi_utils.create_property(gi_mod.Info, prop, getter)

    def __str__(self):
        return "TaskletInfo(instance_name=%s)" % (self.instance_name,)
    gi_mod.Info.__str__ = __str__


wrap_rwtasklet()

# vim: sw=4
