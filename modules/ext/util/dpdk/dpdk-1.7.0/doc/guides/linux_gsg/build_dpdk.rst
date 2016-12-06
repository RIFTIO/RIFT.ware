..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Compiling the Intel® DPDK Target from Source
============================================

.. note::

    Parts of this process can also be done using the setup script described in Chapter 6 of this document.

Install the Intel® DPDK and Browse Sources
------------------------------------------

First, uncompress the archive and move to the uncompressed Intel® DPDK source directory:

.. code-block:: console

   user@host:~$ unzip DPDK-<version>.zip
   user@host:~$ cd DPDK-<version>
   user@host:~/DPDK-<version>$ ls
   app/   config/   examples/   lib/   LICENSE.GPL   LICENSE.LGPL   Makefile   mk/   scripts/   tools/

The Intel® DPDK is composed of several directories:

*   lib: Source code of Intel® DPDK libraries

*   app: Source code of Intel® DPDK applications (automatic tests)

*   examples: Source code of Intel® DPDK application examples

*   config, tools, scripts, mk: Framework-related makefiles, scripts and configuration

Installation of Intel® DPDK Target Environments
-----------------------------------------------

The format of a Intel® DPDK target is:

    ARCH-MACHINE-EXECENV-TOOLCHAIN

where:

*   ARCH can be:  i686, x86_64

*   MACHINE can be:  native, ivshmem

*   EXECENV can be:  linuxapp,  bsdapp

*   TOOLCHAIN can be:  gcc,  icc

The targets to be installed depend on the 32-bit and/or 64-bit packages and compilers installed on the host.
Available targets can be found in the DPDK/config directory.
The defconfig\_ prefix should not be used.

.. note::

    Configuration files are provided with the RTE_MACHINE optimization level set.
    Within the configuration files, the RTE_MACHINE configuration value is set to native,
    which means that the compiled software is tuned for the platform on which it is built.
    For more information on this setting, and its possible values, see the *Intel® DPDK Programmers Guide*.

When using the Intel® C++ Compiler (icc), one of the following commands should be invoked for 64-bit or 32-bit use respectively.
Notice that the shell scripts update the $PATH variable and therefore should not be performed in the same session.
Also, verify the compiler's installation directory since the path may be different:

.. code-block:: console

    source /opt/intel/bin/iccvars.sh intel64
    source /opt/intel/bin/iccvars.sh ia32

To install and make targets, use the make install T=<target> command in the top-level Intel® DPDK directory.

For example, to compile a 64-bit target using icc, run:

.. code-block:: console

    make install T=x86_64-native-linuxapp-icc

To compile a 32-bit build using gcc, the make command should be:

.. code-block:: console

    make install T=i686-native-linuxapp-gcc

To compile all 64-bit targets using gcc, use:

.. code-block:: console

    make install T=x86_64*gcc

To compile all 64-bit targets using both gcc and icc, use:

.. code-block:: console

    make install T=x86_64-*

.. note::

    The wildcard operator (*) can be used to create multiple targets at the same time.

To prepare a target without building it, for example, if the configuration changes need to be made before compilation,
use the make config T=<target> command:

.. code-block:: console

    make config T=x86_64-native-linuxapp-gcc

.. warning::

    The igb_uio module must be compiled with the same kernel as the one running on the target.
    If the Intel® DPDK is not being built on the target machine,
    the RTE_KERNELDIR environment variable should be used to point the compilation at a copy of the kernel version to be used on the target machine.

Once the target environment is created, the user may move to the target environment directory and continue to make code changes and re-compile.
The user may also make modifications to the compile-time Intel® DPDK configuration by editing the .config file in the build directory.
(This is a build-local copy of the defconfig file from the top- level config directory).

.. code-block:: console

    cd x86_64-native-linuxapp-gcc
    vi .config
    make

In addition, the make clean command can be used to remove any existing compiled files for a subsequent full, clean rebuild of the code.

Browsing the Installed Intel® DPDK Environment Target
-----------------------------------------------------

Once a target is created it contains all libraries and header files for the Intel® DPDK environment that are required to build customer applications.
In addition, the test and testpmd applications are built under the build/app directory, which may be used for testing.
In the case of Linux, a kmod  directory is also present that contains a module to install:

.. code-block:: console

    $ ls x86_64-native-linuxapp-gcc
    app build hostapp include kmod lib Makefile

Loading the Intel® DPDK igb_uio Module
--------------------------------------

To run any Intel® DPDK application, the igb_uio module can be loaded into the running kernel.
The module is found in the kmod sub-directory of the Intel® DPDK target directory.
This module should be loaded using the insmod command as shown below (assuming that the current directory is the Intel® DPDK target directory).
In many cases, the uio support in the Linux* kernel is compiled as a module rather than as part of the kernel,
so it is often necessary to load the uio module first:

.. code-block:: console

    sudo modprobe uio
    sudo insmod kmod/igb_uio.ko

Since Intel® DPDK release 1.7 provides VFIO support, compilation and use of igb_uio module has become optional for platforms that support using VFIO.

Loading VFIO Module
-------------------

To run an Intel® DPDK application and make use of VFIO, the vfio-pci module must be loaded:

.. code-block:: console

    sudo modprobe vfio-pci

Note that in order to use VFIO, your kernel must support it.
VFIO kernel modules have been included in the Linux kernel since version 3.6.0 and are usually present by default,
however please consult your distributions documentation to make sure that is the case.

Also, to use VFIO, both kernel and BIOS must support and be configured to use IO virtualization (such as Intel® VT-d).

For proper operation of VFIO when running Intel® DPDK applications as a non-privileged user, correct permissions should also be set up.
This can be done by using the Intel® DPDK setup script (called setup.sh and located in the tools directory).

Binding and Unbinding Network Ports to/from the igb_uioor VFIO Modules
----------------------------------------------------------------------

As of release 1.4, Intel® DPDK applications no longer automatically unbind all supported network ports from the kernel driver in use.
Instead, all ports that are to be used by an Intel® DPDK application must be bound to the igb_uio or vfio-pci module before the application is run.
Any network ports under Linux* control will be ignored by the Intel® DPDK poll-mode drivers and cannot be used by the application.

.. warning::

    The Intel® DPDK will, by default, no longer automatically unbind network ports from the kernel driver at startup.
    Any ports to be used by an Intel® DPDK application must be unbound from Linux* control and bound to the igb_uio or vfio-pci module before the application is run.

To bind ports to the igb_uio or vfio-pci module for Intel® DPDK use, and then subsequently return ports to Linux* control,
a utility script called dpdk_nic _bind.py is provided in the tools subdirectory.
This utility can be used to provide a view of the current state of the network ports on the system,
and to bind and unbind those ports from the different kernel modules, including igb_uio and vfio-pci.
The following are some examples of how the script can be used.
A full description of the script and its parameters can be obtained by calling the script with the --help or --usage options.

.. warning::

    Due to the way VFIO works, there are certain limitations to which devices can be used with VFIO.
    Mainly it comes down to how IOMMU groups work.
    Any Virtual Function device can be used with VFIO on its own, but physical devices will require either all ports bound to VFIO,
    or some of them bound to VFIO while others not being bound to anything at all.

    If your device is behind a PCI-to-PCI bridge, the bridge will then be part of the IOMMU group in which your device is in.
    Therefore, the bridge driver should also be unbound from the bridge PCI device for VFIO to work with devices behind the bridge.

.. warning::

    While any user can run the dpdk_nic_bind.py script to view the status of the network ports,
    binding or unbinding network ports requires root privileges.

To see the status of all network ports on the system:

.. code-block:: console

    root@host:DPDK# ./tools/dpdk_nic_bind.py --status

    Network devices using IGB_UIO driver
    ====================================
    0000:82:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe
    0000:82:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe

    Network devices using kernel driver
    ===================================
    0000:04:00.0 'I350 Gigabit Network Connection' if=em0 drv=igb unused=igb_uio *Active*
    0000:04:00.1 'I350 Gigabit Network Connection' if=eth1 drv=igb unused=igb_uio
    0000:04:00.2 'I350 Gigabit Network Connection' if=eth2 drv=igb unused=igb_uio
    0000:04:00.3 'I350 Gigabit Network Connection' if=eth3 drv=igb unused=igb_uio

    Other network devices
    =====================
    <none>

To bind device eth1, 04:00.1, to the igb_uio driver:

.. code-block:: console

    root@host:DPDK# ./tools/dpdk_nic_bind.py --bind=igb_uio 04:00.1

or, alternatively,

.. code-block:: console

    root@host:DPDK# ./tools/dpdk_nic_bind.py --bind=igb_uio eth1

To restore device 82:00.0 to its original kernel binding:

.. code-block:: console

    root@host:DPDK# ./tools/dpdk_nic_bind.py --bind=ixgbe 82:00.0
