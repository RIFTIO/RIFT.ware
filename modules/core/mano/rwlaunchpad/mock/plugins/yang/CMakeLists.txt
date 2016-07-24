
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


include(rift_yang)
include(rift_plugin)

rift_add_yang_target(
    TARGET
        lpmocklet_yang
    YANG_FILES
        lpmocklet.yang
    COMPONENT
        ${PKG_LONG_NAME}
)

