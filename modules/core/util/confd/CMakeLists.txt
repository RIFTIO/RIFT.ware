# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 03/26/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_externalproject)

if(RIFT_AGENT_BUILD STREQUAL "CONFD_FULL")
  set(CONFD_PKG_NAME confd)
elseif(RIFT_AGENT_BUILD STREQUAL "CONFD_BASIC")
  set(CONFD_PKG_NAME confd-basic)
endif()

if(CONFD_PKG_NAME)

  if (NOT CONFD_VERSION)
    ##################################################################
    ##################################################################
    ##  When upgrading confd inside RIFT.io, be sure to populate
    ##  CONFD_INSTALLER and LIBCONFD_PACKAGE into the private repo,
    ##  at http://repos.riftio.com/private/packages/
    ##################################################################
    ##################################################################
    set(CONFD_VERSION 6.3)
  endif()

  set(CONFD_INSTALLER ${CONFD_PKG_NAME}-${CONFD_VERSION}.linux.x86_64.installer.bin)
  set(LIBCONFD_PACKAGE ${CONFD_PKG_NAME}-${CONFD_VERSION}.libconfd.tar.gz)
  set(CONFD_DIR ${CMAKE_CURRENT_BINARY_DIR}/confd-build)

  add_custom_command(
    OUTPUT
       confd-build/libconfd/include/confd_lib.h
       confd-build/libconfd/src/libconfd.so
       confd-build/libconfd/crypto/crypto.so
       confd-build/bin/confdc
    COMMAND rm -fr ${CONFD_DIR} ${CONFD_INSTALLER} ${LIBCONFD_PACKAGE}
    COMMAND
      ${CMAKE_SOURCE_DIR}/../../toolchain/scripts/get-file
        ${CMAKE_CURRENT_BINARY_DIR}/${CONFD_INSTALLER}
        file://${CMAKE_CURRENT_SOURCE_DIR}/${CONFD_INSTALLER}
        http://repos.riftio.com/private/packages/${CONFD_INSTALLER}
    COMMAND
      ${CMAKE_SOURCE_DIR}/../../toolchain/scripts/get-file
        ${CMAKE_CURRENT_BINARY_DIR}/${LIBCONFD_PACKAGE}
        file://${CMAKE_CURRENT_SOURCE_DIR}/${LIBCONFD_PACKAGE}
        http://repos.riftio.com/private/packages/${LIBCONFD_PACKAGE}
    COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/${CONFD_INSTALLER}
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${CONFD_INSTALLER} ${CONFD_DIR}
    COMMAND tar -xvf ${LIBCONFD_PACKAGE} --transform 's%^${CONFD_PKG_NAME}-${CONFD_VERSION}/%confd-build/%'

    # Make the unix domain socket driver
    COMMAND make -C ${CONFD_DIR}/src/confd/ipc_drv
    COMMAND make -C ${CONFD_DIR}/libconfd CONFD_DIR=${CMAKE_INSTALL_PREFIX}/usr/local/confd
    COMMAND make -C ${CONFD_DIR}/libconfd crypto PREFIX=${CMAKE_INSTALL_PREFIX}
  )

  add_custom_target(build_confd
    DEPENDS
      confd-build/libconfd/include/confd_lib.h
      confd-build/libconfd/src/libconfd.so
      confd-build/libconfd/crypto/crypto.so
      confd-build/bin/confdc
  )

  ##################################################################
  #
  # Confd install files may change from release to release!
  #
  ##################################################################

  install(
    FILES
      ${CONFD_DIR}/include/confd_cdb.h
      ${CONFD_DIR}/include/confd_dp.h
      ${CONFD_DIR}/include/confd_errcode.h
      ${CONFD_DIR}/include/confd_events.h
      ${CONFD_DIR}/include/confd.h
      ${CONFD_DIR}/include/confd_ha.h
      ${CONFD_DIR}/include/confd_lib.h
      ${CONFD_DIR}/include/confd_logsyms.h
      ${CONFD_DIR}/include/confd_maapi.h
      ${CONFD_DIR}/include/confd_netconf_proto.h
    DESTINATION usr/include
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/bin/confd
      ${CONFD_DIR}/bin/confd_cli
      ${CONFD_DIR}/bin/confd_cmd
      ${CONFD_DIR}/bin/confd_load
      ${CONFD_DIR}/bin/confdc
      ${CONFD_DIR}/bin/cs2yang
      ${CONFD_DIR}/bin/erlc
      ${CONFD_DIR}/bin/maapi
      ${CONFD_DIR}/bin/netconf-console
      ${CONFD_DIR}/bin/netconf-console-tcp
      ${CONFD_DIR}/bin/pyang
      ${CONFD_DIR}/bin/yanger
    DESTINATION usr/local/confd/bin
    COMPONENT ${INSTALL_COMPONENT}
  )

  if(RIFT_AGENT_BUILD STREQUAL "CONFD_FULL")
    install(
      PROGRAMS
        ${CONFD_DIR}/bin/smidump
      DESTINATION usr/local/confd/bin
      COMPONENT ${INSTALL_COMPONENT}
    )
  endif()

  install(
    FILES
      ${CONFD_DIR}/etc/confd/confd.ccl
      ${CONFD_DIR}/etc/confd/confd.conf
      ${CONFD_DIR}/etc/confd/confd.conf.example
      ${CONFD_DIR}/etc/confd/confd.conf.full
      ${CONFD_DIR}/etc/confd/iana-crypt-hash.fxs
      ${CONFD_DIR}/etc/confd/ietf-inet-types.fxs
      ${CONFD_DIR}/etc/confd/ietf-netconf-acm.fxs
      ${CONFD_DIR}/etc/confd/ietf-netconf-monitoring.fxs
      ${CONFD_DIR}/etc/confd/ietf-netconf-notifications.fxs
      ${CONFD_DIR}/etc/confd/ietf-yang-library.fxs
      ${CONFD_DIR}/etc/confd/ietf-yang-types.fxs
      ${CONFD_DIR}/etc/confd/java.properties
      ${CONFD_DIR}/etc/confd/tailf-aaa.fxs
      ${CONFD_DIR}/etc/confd/tailf-acm.fxs
      ${CONFD_DIR}/etc/confd/tailf-common-monitoring.fxs
      ${CONFD_DIR}/etc/confd/tailf-confd-monitoring.fxs
      ${CONFD_DIR}/etc/confd/tailf-kicker.fxs
      ${CONFD_DIR}/etc/confd/tailf-netconf-monitoring.fxs
      ${CONFD_DIR}/etc/confd/tailf-rollback.fxs
    DESTINATION usr/local/confd/etc/confd
    COMPONENT ${INSTALL_COMPONENT}
  )

  if(RIFT_AGENT_BUILD STREQUAL "CONFD_FULL")
    install(
      FILES
        ${CONFD_DIR}/etc/confd/smi.conf
        ${CONFD_DIR}/etc/confd/tailf-webui.fxs
      DESTINATION usr/local/confd/etc/confd
      COMPONENT ${INSTALL_COMPONENT}
    )

    install(
      FILES
        ${CONFD_DIR}/etc/confd/snmp/IPV6-TC.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMP-COMMUNITY-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMP-FRAMEWORK-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMP-MPD-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMP-NOTIFICATION-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMP-TARGET-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMP-USER-BASED-SM-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMP-VIEW-BASED-ACM-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMPv2-MIB.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMPv2-SMI.fxs
        ${CONFD_DIR}/etc/confd/snmp/SNMPv2-TC.fxs
        ${CONFD_DIR}/etc/confd/snmp/TRANSPORT-ADDRESS-MIB.fxs
      DESTINATION usr/local/confd/etc/confd/snmp
      COMPONENT ${INSTALL_COMPONENT}
    )
  endif()

  install(
    PROGRAMS
      ${CONFD_DIR}/libconfd/src/libconfd.so
    DESTINATION usr/local/confd/lib
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/etc/confd/ssh/ssh_host_rsa_key
      ${CONFD_DIR}/etc/confd/ssh/ssh_host_rsa_key.pub
    DESTINATION usr/local/confd/etc/confd/ssh
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/bin/confd.boot
      ${CONFD_DIR}/lib/confd/bin/confdc.boot
      ${CONFD_DIR}/lib/confd/bin/start_clean.boot
    DESTINATION usr/local/confd/lib/confd/bin
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/bin/erl
      ${CONFD_DIR}/lib/confd/bin/erlc
      ${CONFD_DIR}/lib/confd/bin/escript
    DESTINATION usr/local/confd/lib/confd/bin
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/erts/bin/child_setup
      ${CONFD_DIR}/lib/confd/erts/bin/confd
      ${CONFD_DIR}/lib/confd/erts/bin/confd.smp
      ${CONFD_DIR}/lib/confd/erts/bin/confdexec
      ${CONFD_DIR}/lib/confd/erts/bin/escript
      ${CONFD_DIR}/lib/confd/erts/bin/heart
      ${CONFD_DIR}/lib/confd/erts/bin/inet_gethost
    DESTINATION usr/local/confd/lib/confd/erts/bin
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/cli/cli.ez
    DESTINATION usr/local/confd/lib/confd/lib/cli
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/cli/cli/priv/cli-builtin.fxs
    DESTINATION usr/local/confd/lib/confd/lib/cli/cli/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/confdc/confdc.ez
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger.ez
    DESTINATION usr/local/confd/lib/confd/lib/confdc
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/confdc/confdc/priv/clispec.fxs
      ${CONFD_DIR}/lib/confd/lib/confdc/confdc/priv/index.html.src
      ${CONFD_DIR}/lib/confd/lib/confdc/confdc/priv/miba.fxs
    DESTINATION usr/local/confd/lib/confd/lib/confdc/confdc/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/bin/yanger
    DESTINATION usr/local/confd/lib/confd/lib/confdc/yanger/bin
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yang_parser_nif.so
    DESTINATION usr/local/confd/lib/confd/lib/confdc/yanger/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_expand.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_fxs.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_lsa.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_ncs.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_smiv2.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_sn.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_tailf.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_tree.beam
      ${CONFD_DIR}/lib/confd/lib/confdc/yanger/priv/yanger_yang.beam
    DESTINATION usr/local/confd/lib/confd/lib/confdc/yanger/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/core/asn1.ez
      ${CONFD_DIR}/lib/confd/lib/core/capi.ez
      ${CONFD_DIR}/lib/confd/lib/core/cdb.ez
      ${CONFD_DIR}/lib/confd/lib/core/compiler.ez
      ${CONFD_DIR}/lib/confd/lib/core/confd.ez
      ${CONFD_DIR}/lib/confd/lib/core/crypto.ez
      ${CONFD_DIR}/lib/confd/lib/core/econfd.ez
      ${CONFD_DIR}/lib/confd/lib/core/inets.ez
      ${CONFD_DIR}/lib/confd/lib/core/kernel.ez
      ${CONFD_DIR}/lib/confd/lib/core/msc.ez
      ${CONFD_DIR}/lib/confd/lib/core/pam.ez
      ${CONFD_DIR}/lib/confd/lib/core/ssh.ez
      ${CONFD_DIR}/lib/confd/lib/core/stdlib.ez
      ${CONFD_DIR}/lib/confd/lib/core/tts.ez
      ${CONFD_DIR}/lib/confd/lib/core/util.ez
      ${CONFD_DIR}/lib/confd/lib/core/xds.ez
      ${CONFD_DIR}/lib/confd/lib/core/xpath.ez
    DESTINATION usr/local/confd/lib/confd/lib/core
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/asn1/priv/lib/asn1_erl_nif.so
    DESTINATION usr/local/confd/lib/confd/lib/core/asn1/priv/lib
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/capi/priv/confd_aaa_bridge
      ${CONFD_DIR}/lib/confd/lib/core/capi/priv/utype.so
    DESTINATION usr/local/confd/lib/confd/lib/core/capi/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/cmdptywrapper
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/cmdwrapper
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/crypt-hash
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/ipc_drv.so
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/ipc_drv_ops.so
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/mmap_schema
    DESTINATION usr/local/confd/lib/confd/lib/core/confd/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/confd.fxs
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/confd_cfg.fxs
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/config.fxs
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/ietf-inet-types.fxs
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/ietf-yang-types.fxs
      ${CONFD_DIR}/lib/confd/lib/core/confd/priv/netconf.fxs
    DESTINATION usr/local/confd/lib/confd/lib/core/confd/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/libconfd/crypto/crypto.so
    DESTINATION usr/local/confd/lib/confd/lib/core/crypto/priv/lib
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/crypto/priv/lib/crypto_callback.so
    DESTINATION usr/local/confd/lib/confd/lib/core/crypto/priv/lib
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/core/kernel/include/dist.hrl
      ${CONFD_DIR}/lib/confd/lib/core/kernel/include/dist_util.hrl
      ${CONFD_DIR}/lib/confd/lib/core/kernel/include/file.hrl
      ${CONFD_DIR}/lib/confd/lib/core/kernel/include/inet.hrl
      ${CONFD_DIR}/lib/confd/lib/core/kernel/include/inet_sctp.hrl
      ${CONFD_DIR}/lib/confd/lib/core/kernel/include/net_address.hrl
    DESTINATION usr/local/confd/lib/confd/lib/core/kernel/include
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/pam/priv/epam
    DESTINATION usr/local/confd/lib/confd/lib/core/pam/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/core/stdlib/include/erl_bits.hrl
      ${CONFD_DIR}/lib/confd/lib/core/stdlib/include/erl_compile.hrl
      ${CONFD_DIR}/lib/confd/lib/core/stdlib/include/ms_transform.hrl
      ${CONFD_DIR}/lib/confd/lib/core/stdlib/include/qlc.hrl
      ${CONFD_DIR}/lib/confd/lib/core/stdlib/include/zip.hrl
    DESTINATION usr/local/confd/lib/confd/lib/core/stdlib/include
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/tts/priv/tts_nif.so
    DESTINATION usr/local/confd/lib/confd/lib/core/tts/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/util/priv/pregex_nif.so
      ${CONFD_DIR}/lib/confd/lib/core/util/priv/syslog_nif.so
      ${CONFD_DIR}/lib/confd/lib/core/util/priv/syst_nif.so
      ${CONFD_DIR}/lib/confd/lib/core/util/priv/w3cregex_nif.so
    DESTINATION usr/local/confd/lib/confd/lib/core/util/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/confd/lib/core/xds/priv/otts_nif.so
    DESTINATION usr/local/confd/lib/confd/lib/core/xds/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/netconf/ncm.ez
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf.ez
    DESTINATION usr/local/confd/lib/confd/lib/netconf
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/ietf_with_defaults.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_actions.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_forward.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_inactive.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_netmod.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_notification.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_partial_lock.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_tailf_commit.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/netconf_transactions.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/tailf-netconf-extensions.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/tailf-netconf-query.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/tailf_netconf.fxs
      ${CONFD_DIR}/lib/confd/lib/netconf/netconf/priv/yang.fxs
    DESTINATION usr/local/confd/lib/confd/lib/netconf/netconf/priv
    COMPONENT ${INSTALL_COMPONENT}
  )

  if(RIFT_AGENT_BUILD STREQUAL "CONFD_FULL")
    install(
      FILES
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp.ez
        ${CONFD_DIR}/lib/confd/lib/snmp/snmpa.ez
        ${CONFD_DIR}/lib/confd/lib/snmp/snmpgw.ez
        ${CONFD_DIR}/lib/confd/lib/snmp/snmpm.ez
      DESTINATION usr/local/confd/lib/confd/lib/snmp
      COMPONENT ${INSTALL_COMPONENT}
    )

    install(
      FILES
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/INET-ADDRESS-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/OTP-SNMPEA-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/RFC1213-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-COMMUNITY-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-FRAMEWORK-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-MPD-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-NOTIFICATION-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-TARGET-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-USER-BASED-SM-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-USM-AES-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMP-VIEW-BASED-ACM-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMPv2-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMPv2-TC.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/SNMPv2-TM.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/STANDARD-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/TRANSPORT-ADDRESS-MIB.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/snmp_tables.hrl
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/include/snmp_types.hrl
      DESTINATION usr/local/confd/lib/confd/lib/snmp/snmp/include
      COMPONENT ${INSTALL_COMPONENT}
    )

    install(
      FILES
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/IPV6-TC.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMP-COMMUNITY-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMP-FRAMEWORK-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMP-MPD-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMP-NOTIFICATION-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMP-TARGET-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMP-USER-BASED-SM-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMP-VIEW-BASED-ACM-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/SNMPv2-MIB.bin
        ${CONFD_DIR}/lib/confd/lib/snmp/snmp/priv/mibs/TRANSPORT-ADDRESS-MIB.bin
      DESTINATION usr/local/confd/lib/confd/lib/snmp/snmp/priv/mibs
      COMPONENT ${INSTALL_COMPONENT}
    )
  endif()

  install(
    PROGRAMS
      ${CONFD_DIR}/lib/pyang/bin/pyang
    DESTINATION usr/local/confd/lib/pyang/bin
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/pyang/pyang/__init__.py
      ${CONFD_DIR}/lib/pyang/pyang/__init__.pyc
      ${CONFD_DIR}/lib/pyang/pyang/error.py
      ${CONFD_DIR}/lib/pyang/pyang/error.pyc
      ${CONFD_DIR}/lib/pyang/pyang/grammar.py
      ${CONFD_DIR}/lib/pyang/pyang/grammar.pyc
      ${CONFD_DIR}/lib/pyang/pyang/hello.py
      ${CONFD_DIR}/lib/pyang/pyang/hello.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugin.py
      ${CONFD_DIR}/lib/pyang/pyang/plugin.pyc
      ${CONFD_DIR}/lib/pyang/pyang/statements.py
      ${CONFD_DIR}/lib/pyang/pyang/statements.pyc
      ${CONFD_DIR}/lib/pyang/pyang/syntax.py
      ${CONFD_DIR}/lib/pyang/pyang/syntax.pyc
      ${CONFD_DIR}/lib/pyang/pyang/types.py
      ${CONFD_DIR}/lib/pyang/pyang/types.pyc
      ${CONFD_DIR}/lib/pyang/pyang/util.py
      ${CONFD_DIR}/lib/pyang/pyang/util.pyc
      ${CONFD_DIR}/lib/pyang/pyang/xpath.py
      ${CONFD_DIR}/lib/pyang/pyang/xpath.pyc
      ${CONFD_DIR}/lib/pyang/pyang/yang_parser.py
      ${CONFD_DIR}/lib/pyang/pyang/yang_parser.pyc
      ${CONFD_DIR}/lib/pyang/pyang/yin_parser.py
      ${CONFD_DIR}/lib/pyang/pyang/yin_parser.pyc
    DESTINATION usr/local/confd/lib/pyang/pyang
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/pyang/pyang/plugins/__init__.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/__init__.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/capability.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/capability.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/check_update.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/check_update.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/cs.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/cs.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/depend.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/depend.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/html.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/html.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/ietf.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/ietf.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/jsonxsl.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/jsonxsl.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/jstree.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/jstree.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/jtox.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/jtox.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/omni.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/omni.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/rest_doc.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/rest_doc.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/sample-xml-skeleton.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/sample-xml-skeleton.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/smi.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/smi.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/tailf.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/tailf.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/tree.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/tree.pyc
      ${CONFD_DIR}/lib/pyang/pyang/plugins/uml.py
      ${CONFD_DIR}/lib/pyang/pyang/plugins/uml.pyc
    DESTINATION usr/local/confd/lib/pyang/pyang/plugins
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/lib/pyang/pyang/translators/__init__.py
      ${CONFD_DIR}/lib/pyang/pyang/translators/__init__.pyc
      ${CONFD_DIR}/lib/pyang/pyang/translators/dsdl.py
      ${CONFD_DIR}/lib/pyang/pyang/translators/dsdl.pyc
      ${CONFD_DIR}/lib/pyang/pyang/translators/netmod.rng
      ${CONFD_DIR}/lib/pyang/pyang/translators/schemanode.py
      ${CONFD_DIR}/lib/pyang/pyang/translators/schemanode.pyc
      ${CONFD_DIR}/lib/pyang/pyang/translators/xsd.py
      ${CONFD_DIR}/lib/pyang/pyang/translators/xsd.pyc
      ${CONFD_DIR}/lib/pyang/pyang/translators/yang.py
      ${CONFD_DIR}/lib/pyang/pyang/translators/yang.pyc
      ${CONFD_DIR}/lib/pyang/pyang/translators/yin.py
      ${CONFD_DIR}/lib/pyang/pyang/translators/yin.pyc
    DESTINATION usr/local/confd/lib/pyang/pyang/translators
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/src/confd/yang/Makefile
      ${CONFD_DIR}/src/confd/yang/iana-crypt-hash.yang
      ${CONFD_DIR}/src/confd/yang/ietf-inet-types.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf-acm-ann.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf-acm.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf-monitoring-ann.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf-monitoring.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf-notifications-ann.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf-notifications.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf-with-defaults.yang
      ${CONFD_DIR}/src/confd/yang/ietf-netconf.yang
      ${CONFD_DIR}/src/confd/yang/ietf-yang-library.yang
      ${CONFD_DIR}/src/confd/yang/ietf-yang-smiv2.yang
      ${CONFD_DIR}/src/confd/yang/ietf-yang-types.yang
      ${CONFD_DIR}/src/confd/yang/tailf-acm.yang
      ${CONFD_DIR}/src/confd/yang/tailf-cli-extensions.yang
      ${CONFD_DIR}/src/confd/yang/tailf-common-monitoring-ann.yang
      ${CONFD_DIR}/src/confd/yang/tailf-common-monitoring.yang
      ${CONFD_DIR}/src/confd/yang/tailf-common-query.yang
      ${CONFD_DIR}/src/confd/yang/tailf-common.yang
      ${CONFD_DIR}/src/confd/yang/tailf-confd-monitoring-ann.yang
      ${CONFD_DIR}/src/confd/yang/tailf-confd-monitoring.yang
      ${CONFD_DIR}/src/confd/yang/tailf-kicker.yang
      ${CONFD_DIR}/src/confd/yang/tailf-meta-extensions.yang
      ${CONFD_DIR}/src/confd/yang/tailf-netconf-inactive.yang
      ${CONFD_DIR}/src/confd/yang/tailf-netconf-monitoring.yang
      ${CONFD_DIR}/src/confd/yang/tailf-netconf-query.yang
      ${CONFD_DIR}/src/confd/yang/tailf-netconf-transactions.yang
      ${CONFD_DIR}/src/confd/yang/tailf-rest-error.yang
      ${CONFD_DIR}/src/confd/yang/tailf-rest-query.yang
      ${CONFD_DIR}/src/confd/yang/tailf-webui.yang
      ${CONFD_DIR}/src/confd/yang/tailf-xsd-types.yang
    DESTINATION usr/local/confd/src/confd/yang
    COMPONENT ${INSTALL_COMPONENT}
  )

  install(
    FILES
      ${CONFD_DIR}/var/confd/cdb/aaa_init.xml
    DESTINATION usr/local/confd/var/confd/cdb
    COMPONENT ${INSTALL_COMPONENT}
  )

endif()




