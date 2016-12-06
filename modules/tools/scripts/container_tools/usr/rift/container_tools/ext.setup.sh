

# we really should be deleting ALL ext packages
# on all platforms, but hopefully soon we will be
# using new containers for every build

if [[ ${RIFT_PLATFORM} == 'fc20' ]]; then
    yum erase -y protobuf-c-devel protobuf-c
fi

