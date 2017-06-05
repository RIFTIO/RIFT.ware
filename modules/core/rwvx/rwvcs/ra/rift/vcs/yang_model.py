
# STANDARD_RIFT_IO_COPYRIGHT

import rw_peas
import gi
gi.require_version('YangModelPlugin', '1.0')

yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
assert yang is not None

yang_model_api = yang.get_interface('Model')
assert yang_model_api is not None

yang_model = yang_model_api.alloc()
assert yang_model is not None

base_module = yang_model_api.load_module(yang_model, 'rw-base')
assert base_module is not None

manifest_module = yang_model_api.load_module(yang_model, 'rw-manifest')
assert manifest_module is not None

vcs_module = yang_model_api.load_module(yang_model, 'rw-vcs')
assert vcs_module is not None

