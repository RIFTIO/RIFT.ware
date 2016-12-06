
RIFT_INSTALL=/net/gluon/localdisk/speriasa/master/.install

export LD_LIBRARY_PATH=${RIFT_INSTALL}/usr/lib64:${RIFT_INSTALL}/usr/lib:${RIFT_INSTALL}/usr/lib/rift/plugins:${RIFT_INSTALL}/usr/local/confd/lib

# These would be needed to load a plugin
# ?? export PEAS_PLUGIN_LOADERS_DIR=${RIFT_INSTALL}/usr/lib/libpeas-1.0/loaders
# ?? export PLUGINDIR=${RIFT_INSTALL}/usr/lib/rift/plugins

# These would be needed to run Python code interfacing with Riftware
# ?? export GI_TYPELIB_PATH=${RIFT_INSTALL}/usr/lib/girepository-1.0:${RIFT_INSTALL}/usr/lib/rift/girepository-1.0:${RIFT_INSTALL}/usr/lib/rift/plugins

# Unclear what needs these schema files
# ?? export YUMA_MODPATH=${RIFT_INSTALL}/usr/data/yang:${RIFT_INSTALL}/usr/local/pyang-1.4.1/share/yang/modules:${RIFT_INSTALL}/usr/share/yuma/modules/ietf:${RIFT_INSTALL}/usr/local/confd/src/confd/yang

