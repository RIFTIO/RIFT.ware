include(rift_python)
include(rift_plugin)


rift_python_install_tree(
  FILES
    rift/rwpb/__init__.py  
    rift/rwpb/ir.py
    rift/rwpb/memberdata.py
    rift/rwpb/pathwalker.py
    rift/rwpb/schema.py
    rift/rwpb/util.py
  PYTHON3_ONLY
  COMPONENT
    ${INSTALL_COMPONENT}
)
