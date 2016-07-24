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
%define AGENT_BUILD       %{_agent_build}

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

Requires: riftware-base >= %{_version}, gdb
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

# test
echo "   Name:  %{name}"
echo "Version:  %{version}.%{_buildnum}"
echo "Release:  %{release}"
echo "Agent_Build:  %{AGENT_BUILD}"

# copy all copied source files
cp -Rp %{RIFT_ROOT}/.install/rpmbuild/SOURCES/%{name}-%{version}/* %{buildroot}/

# setup .artifacts for manifest files
mkdir %{buildroot}/%{DST_RIFT_ROOT}/.artifacts

%if "%{AGENT_BUILD}" != "XML_ONLY"

echo "XML/CONFD: ConfD build"

# confD needs some writeable directories to start up
mkdir -p %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/var/confd/{state,candidate,rollback,log}

# No execute perms on these tailf-provided binaries, to exclude them from the debuginfo/buildid regime.
# chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/ipc_drv/ipc_drv_unix.o
# chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_ncs/_ncs_py2.so
# chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_ncs/_ncs_py3.cpython-33m.so
# chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_confd/_confd_py2.so
# chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_confd/_confd_py3.cpython-33m.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/smidump
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/confd_load
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/confd_cli
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/maapi
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/confd_cmd
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/libconfd.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/bin/erlc
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/bin/escript
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/heart
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/escript
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/confd
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/inet_gethost
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/confd.smp
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/child_setup
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/confdexec
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/webui/yaws/priv/lib/setuid_drv.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/webui/yaws/priv/lib/yaws_sendfile_drv.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/w3cregex_nif.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/syst_nif.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/syslog_nif.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/pregex_nif.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/capi/priv/confd_aaa_bridge
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/capi/priv/utype.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/asn1/priv/lib/asn1_erl_nif.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/cmdwrapper
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/crypt-hash
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/cmdptywrapper
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/ipc_drv_ops.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/ipc_drv.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/crypto/priv/lib/crypto_callback.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/crypto/priv/lib/crypto.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/pam/priv/epam
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/tts/priv/tts_nif.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/xds/priv/otts_nif.so
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/confdc/yanger/priv/yang_parser_nif.so

%else

echo "XML/CONFD: XML build"

%endif

# No execute perms on these ext compiled binaries, to exclude them from the debuginfo/buildid regime.
chmod ugo-x %{buildroot}/%{DST_RIFT_ROOT}/.install/usr/bin/serf

rm -f \
    %{buildroot}/debugfiles.list \
    %{buildroot}/debugsources.list \
    %{buildroot}/debuglinks.list \
    %{buildroot}/elfbins.list

%files

%if "%{AGENT_BUILD}" != "XML_ONLY"

# Undo the chmod -x we did above to avoid find-debuginfo erroring out on this tailf-provided binary
#### attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/ipc_drv/ipc_drv_unix.o
#### attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_ncs/_ncs_py2.so
#### attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_ncs/_ncs_py3.cpython-33m.so
#### attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_confd/_confd_py2.so
#### attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/src/confd/pyapi/_confd/_confd_py3.cpython-33m.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/smidump
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/confd_load
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/confd_cli
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/maapi
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/bin/confd_cmd
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/libconfd.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/bin/erlc
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/bin/escript
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/heart
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/escript
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/confd
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/inet_gethost
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/confd.smp
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/child_setup
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/erts/bin/confdexec
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/webui/yaws/priv/lib/setuid_drv.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/webui/yaws/priv/lib/yaws_sendfile_drv.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/w3cregex_nif.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/syst_nif.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/syslog_nif.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/util/priv/pregex_nif.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/capi/priv/confd_aaa_bridge
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/capi/priv/utype.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/asn1/priv/lib/asn1_erl_nif.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/cmdwrapper
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/crypt-hash
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/cmdptywrapper
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/ipc_drv_ops.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/confd/priv/ipc_drv.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/crypto/priv/lib/crypto_callback.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/crypto/priv/lib/crypto.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/pam/priv/epam
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/tts/priv/tts_nif.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/core/xds/priv/otts_nif.so
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/local/confd/lib/confd/lib/confdc/yanger/priv/yang_parser_nif.so

%endif

# Undo the chmod -x we did above to avoid find-debuginfo erroring out on these RW compiled ext binaries.
%attr(555, -, -) /%{DST_RIFT_ROOT}/.install/usr/bin/serf

/%{DST_RIFT_ROOT}/

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
#rm -rf $RPM_BUILD_ROOT
#rm -rvf $RPM_BUILD_ROOT

# we may need some of this stuff down the road ?!?
# doc AUTHORS ChangeLog NEWS README THANKS TODO
# license COPYING

%changelog
* Thu Nov 12 2015 <Nate.Hudson@riftio.com> 4.0
- Initial version of the package

