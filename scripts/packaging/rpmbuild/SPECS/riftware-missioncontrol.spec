# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

%define _topdir           %{_rift_root}/.install/rpmbuild/
%define name              %{_rpmname}
%define release           1%{?dist}
#%%define release           %{_buildnum}%{?dist}
%define version           %{_version}
%define buildroot         %{_topdir}/%{name}-%{version}-root
%define DST_RIFT_ROOT     %{_dst_rift_root}
%define RIFT_ROOT         %{_rift_root}

%define _binaries_in_noarch_packages_terminate_build   0 	# http://winzter143.blogspot.com/2011/11/linux-arch-dependent-binaries-in-noarch.html

# Override this redhat macro.  This definition skips the Python
# byte-compile, which we don't like (I think due to Py2/Py3 confusion
# with Riftware plugins)
#
%define __os_install_post    \
  /usr/lib/rpm/redhat/brp-compress \
  %{!?__debug_package:\
  /usr/lib/rpm/redhat/brp-strip %{__strip} \
  /usr/lib/rpm/redhat/brp-strip-comment-note %{__strip} %{__objdump} \
   } \
   /usr/lib/rpm/redhat/brp-strip-static-archive %{__strip} \
%{nil}

Name:     	%{name}
Version:  	%{version}
Release:  	%{release}
Summary:        RIFT.ware

License:        Apache
URL:            http://www.riftio.com
Source0:        %{name}-%{version}.tar.gz

#BuildArch: noarch

BuildRequires: yum

# turn this off when we have proper deps
AutoReqProv: no 

Requires: riftware-base >= %{_version}, riftware-base-ui = %{_version}
#Requires(post): info
#Requires(preun): info


%description
The %{name} program is a RIFT.ware package.


# This reads the sources and patches in the source directory %_sourcedir. It unpackages the sources to a subdirectory underneath the build directory %_builddir and applies the patches.
%prep
%autosetup -n %{name}-%{version}

# This compiles the files underneath the build directory %_builddir. This is often implemented by running some variation of "./configure && make".
%build
echo "BuildDir: %_builddir"

#%setup


# This reads the files underneath the build directory %_builddir and writes to a directory underneath the build root directory %_buildrootdir. The files that are written are the files that are supposed to be installed when the binary package is installed by an end-user. 
# Beware of the weird terminology: The build root directory is not the same as the build directory. This is often implemented by running "make install".
#
# this is a little wonky now but it works 
#
%install
echo "buildroot: %{buildroot}"
echo "pwd: `pwd`";

# copy in service files to buildroot .install location
mkdir -p %{buildroot}/%{DST_RIFT_ROOT}/.install/var/rift/services/%{name}/
cp -p %{RIFT_ROOT}/scripts/packaging/services/%{name}/rwmc-start.sh  %{buildroot}/%{DST_RIFT_ROOT}/.install/var/rift/services/%{name}/
cp -p %{RIFT_ROOT}/scripts/packaging/services/%{name}/rwmc-stop.sh   %{buildroot}/%{DST_RIFT_ROOT}/.install/var/rift/services/%{name}/
cp -p %{RIFT_ROOT}/scripts/packaging/services/%{name}/rwmc.screenrc  %{buildroot}/%{DST_RIFT_ROOT}/.install/var/rift/services/%{name}/
# and service to final OS location
install -p -D -m 644 %{RIFT_ROOT}/scripts/packaging/services/%{name}/rwmc.service  %{buildroot}%{_unitdir}/rwmc.service

# copy all copied source files
cp -Rp %{RIFT_ROOT}/.install/rpmbuild/SOURCES/%{name}-%{version}/* %{buildroot}/

rm -f \
    %{buildroot}/debugfiles.list \
    %{buildroot}/debugsources.list \
    %{buildroot}/debuglinks.list \
    %{buildroot}/elfbins.list


%files
/%{DST_RIFT_ROOT}/
%{_unitdir}/rwmc.service

%files debuginfo

/debugfiles.list
/debugsources.list
/debuglinks.list
/elfbins.list

# Don't ask...

%exclude /debugfiles.list
%exclude /debugsources.list
%exclude /debuglinks.list
%exclude /elfbins.list


%clean
rm -rf ${buildroot}


%post
# reload systemD and enable service
/bin/systemctl daemon-reload 
/bin/systemctl enable rwmc
test -f /etc/sysconfig/rift-mission-control || echo 'MC_OPTS=""' >/etc/sysconfig/rift-mission-control
# do we want to auto start mission control?
#systemctl start rwmc

# use a glob* here like
# or a directory ???
# like %{DST_RIFT_ROOT}/demos ???
# or use an external list of files
# %files -f xconfig_files.txt

# filelist test
#%files -f %{_filelist}


%changelog
* Thu Nov 12 2015 <Nate.Hudson@riftio.com> 4.0
- Initial version of the package

