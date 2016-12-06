#!/bin/bash

echo "1a) checking pip six"
cmd "pip list | grep -i six"
echo "1b) checking pip3 six"
cmd "$PIP3_INSTALLER list | grep -i six"

echo "upgrading pip six"
cmd "pip install --upgrade six"

echo "2a) checking pip six"
cmd "pip list | grep -i six"
echo "2b) checking pip3 six"
cmd "$PIP3_INSTALLER list | grep -i six"

# somehow this library os getting installed on fc20, I think from our external repos
# adding it here for ub16 so it doesn't break that
if [ $RIFT_PLATFORM == ub16 ]; then
    pip3_install protobuf
fi

# Install the correct kernel packages

if [[ ${RIFT_PLATFORM} == 'fc20' ]]; then

    # The kernel version for fc20 is frozen (by default) at 3.12.9.  This is the
    # version in the RIFT misc repo, and that is expected by the build system.
    KERNEL_REV=${KERNEL_REV:-3.12.9-301.fc20.x86_64}
    if $CONTAINER; then
        yum_install kernel-devel-${KERNEL_REV}
    else
        yum_install kernel-${KERNEL_REV} kernel-devel-${KERNEL_REV} kernel-modules-extra-${KERNEL_REV}
    fi
    if [ ! -e /lib/modules/${KERNEL_REV}/build ]; then
	mkdir -p /lib/modules/${KERNEL_REV}
	ln -s /usr/src/kernels/${KERNEL_REV} /lib/modules/${KERNEL_REV}/build
    fi
else
    if [[ $DISTRO == fedora ]]; then
        yum_install kernel-devel
    else
	:
    fi
fi


$PIP3_INSTALLER install lxml
