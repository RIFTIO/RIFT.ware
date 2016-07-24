# Description: Specification for building an RPM package for SEP kernel modules
# Copyright (C) 2013-2014 Intel Corporation.  All Rights Reserved.

# Disclaimer: for flexibility purposes this spec does not take into 
# account all dependencies on build and install stage.

# Building a driver requires defining the following values:
#    * target kernel version
#    * sources directory
#    * arity (smp or up) 
#    * architecture (x32 or x32_64).
# This can be done automatically or manually.

# Automatic Definition of Values:
#---------------------------------
# To define all these values automatically, specify the following:  
# Enter the capability from the "Provides" property, of the kernel-devel RPM package in the following macro. For example:
#     - For latest RedHat / Fedora: use kernel-devel-uname-r
#     - For latest SuSE / OpenSuSE: use "kernel-devel-uname-r", or kernel-default-devel 

%define target_kernel_devel_capability kernel-devel-uname-r
%define target_kernel_devel_package %(rpm -q --whatprovides %{target_kernel_devel_capability} 2>/dev/null | head -n 1)

# Manual Definition of Values:
# ----------------------------
# To define the values manually, set the correct values and uncomment the macros below:
#     define target_kernel_version  - Enter the exact version of target kernel as provided by uname -r
#     define target_kernel_src_dir  - Enter the path to the target kernel build directory
#     define target_kernel_arity    - Enter: smp or up, as defined in target kernel config
#     define _target_cpu            - Enter the target processor type, for example, i586 or i686. 
#                                     To check the type use 'uname -m' on the target

# Building an RPM Package:
# -----------------------------------
# After you have defined the values, you can build the package using the following commands:
# To create the RPM package from the .tar file use these commands:
# cd ../../ # -change directory to the sepdk directory 
# tar czf sepdk.tar.gz sepdk  /#
# rpmbuild -tb sepdk.tar.gz 

# To create the src.rpm package and build the RPM package from it, use these commands:
# cd ../../ # -change directory to the sepdk directory 
# tar czf sepdk.tar.gz sepdk
# rpmbuild -ts sepdk.tar.gz                     # Creates intel-sepdk-x-y.src.rpm package
# rpmbuild --rebuild intel-sepdk-x-y.src.rpm    # RPM package could be built from src.rpm

# A package contains three kernel modules: sep, pax, vtsspp.
# If the target kernel does not have the requirements for the vtsspp kernel module
# you can build the package without it:

# rpmbuild -tb --without=vtsspp sepdk.tar.gz
# or 
# rpmbuild --rebuild intel-sepdk-x-y.src.rpm

%{!?target_kernel_version: %define target_kernel_version %(rpm -q --provides %{target_kernel_devel_package} | grep -E "%{target_kernel_devel_capability} +=" | head -n 1  | cut -d " " -f3)}

# If you know the exact kernel build directory you can specify it in the command as follows:

%{!?target_kernel_src_dir: %define target_kernel_src_dir /usr/src/kernels/%{target_kernel_version}}

# Target kernel arity: SMP or UP - use smp if CONFIG_SMP=yes is specified in the target kernel config
%{!?target_kernel_arity: %define target_kernel_arity smp}

# If you know exact the target kernel architecture (x86_64/i686/i586...) enter  it here:
%{!?_target_cpu: %define _target_cpu %(rpm -q --queryformat "%{ARCH}" %{target_kernel_devel_package})}

# Target platform: x32 for i586/i686/etc or x32_64 for x86_64
%{!?target_kernel_platform: %define target_kernel_platform %(if [ %{_target_cpu} == 'x86_64' ]; then echo x32_64; else echo x32; fi)}

%{!?_with_vtsspp: %{!?_without_vtsspp: %define _with_vtsspp --with-vtsspp}}

Summary: SEP driver for Intel(R) VTune(TM) Amplifier XE for Systems
Name: intel-sepdk
Source: sepdk.tar.gz
Version: 1
Release: 0
VCS:     #vendor/1-0-0-g9c5fb460282e14cb24759d252a0a6779a34304f2
License: GPLv2/LGPL
Group: System/Performance
Vendor: Intel Corporation
URL: http://software.intel.com/en-US/software-tools-for-developers-worldwide
Prefix: /opt/intel/sepdk
BuildRoot: %{_tmppath}/%{name}
%{?target_kernel_devel_capability:BuildRequires: %{target_kernel_devel_capability}}

# on Fedora, it may be helpful to resolve multiple choices for target_kernel_devel_capability
#BuildRequires: %kernel_module_package_buildreqs
BuildRequires: gcc
BuildRequires: coreutils, binutils, grep
#Requires: module-init-tools, coreutils


%define debug_package %{nil}

%description
SEP driver for VTune Amplifier XE for Systems

%prep

%setup -n sepdk

%build
cd src

make KERNEL_VERSION=%{target_kernel_version} \
     KERNEL_SRC_DIR=%{target_kernel_src_dir} \
     PER_USER_MODE=NO \
     PLATFORM=%{target_kernel_platform} \
     ARITY=%{target_kernel_arity} \
     clean default

%install
%define sep_scripts src/insmod-sep3 src/rmmod-sep3 src/boot-script src/README.txt
%define pax_scripts src/pax/rmmod-pax src/pax/insmod-pax src/pax/boot-script src/pax/README.txt
%define vtsspp_scripts src/vtsspp/rmmod-vtsspp src/vtsspp/insmod-vtsspp src/vtsspp/README.txt

%define sep_driver src/sep*%{target_kernel_platform}-%{target_kernel_version}%{target_kernel_arity}.ko
%define pax_driver src/pax/pax*%{target_kernel_platform}-%{target_kernel_version}%{target_kernel_arity}.ko
%define vtsspp_driver src/vtsspp/vtsspp*%{target_kernel_platform}-%{target_kernel_version}%{target_kernel_arity}.ko

mkdir -p %{buildroot}%{prefix}/sepdk/src/vtsspp
mkdir -p %{buildroot}%{prefix}/sepdk/src/pax

#copy drivers

eval cp %{sep_driver} %{buildroot}%{prefix}/sepdk/src
eval cp %{pax_driver}  %{buildroot}%{prefix}/sepdk/src/pax

#vtss driver
%if %{?_with_vtsspp:1}%{!?_with_vtsspp:0}
eval cp %{vtsspp_driver} %{buildroot}%{prefix}/sepdk/src/vtsspp
%endif

#copy scripts
eval cp %{sep_scripts} %{buildroot}%{prefix}/sepdk/src
eval cp %{pax_scripts} %{buildroot}%{prefix}/sepdk/src/pax

#vtss scripts
%if %{?_with_vtsspp:1}%{!?_with_vtsspp:0}
eval cp %{vtsspp_scripts} %{buildroot}%{prefix}/sepdk/src/vtsspp
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{prefix}

%post
%{prefix}/sepdk/src/insmod-sep3 -re

%preun
%{prefix}/sepdk/src/rmmod-sep3
%{prefix}/sepdk/src/pax/rmmod-pax

