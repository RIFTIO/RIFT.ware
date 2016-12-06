# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# 

cmake_minimum_required(VERSION 2.8)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/..)

##
# Make a shell script for running yangdump here
##
rift_yangdump_shell(
  yangtools_yangdump_shell
  NAME ${CMAKE_CURRENT_BINARY_DIR}/yangdump.sh
)

##
# Make a shell script for running pyang here
##
rift_pyang_shell(
  yangtools_pyang_shell
  NAME ${CMAKE_CURRENT_BINARY_DIR}/pyang.sh
)

##
# Make a shell script for running yangpbc here
##
rift_yangpbc_shell(
  yangtools_yangpbc_shell
  NAME ${CMAKE_CURRENT_BINARY_DIR}/yangpbc.sh
)


###############################################################################
##
# Yang files that are parsed to protoc-c in order to be used in various
# unit tests.  These are required to compile the unit tests.  The C
# files must be compiled and linked into a library.
##
rift_files_prepend_path(
  yangtools_test_yang_gtest_files
  ${CMAKE_CURRENT_SOURCE_DIR}
  base-conversion.yang          ## pb2xml and back base definitions
  bumpy-conversion.yang         ## pb2xml and back with bumpy
  exampletest.yang              ## Testing Gi ref of protobuf
  ctype-test.yang               ## for ctype_test.cpp
  flat-conversion.yang          ## pb2xml and back with flats
  minikeys.yang                 ## multi-keys
  other-data_rwvcs.yang         ## for yangdom_test.cpp
  rift-cli-test.yang            ## yangcli
  test-leafref.yang             ## for yangpbc_test.cpp
  test-rwapis.yang              ## for rw_schema_test.cpp
  test-tag-generation-base.yang ## for tag generation test
  test-tag-generation.yang      ## for tag generation test
  test-xml2pb.yang              ## for xml2pb.cpp
  test-yang-desc.yang           ## for pb2c_test.cpp
  test-ydom-aug.yang            ## for yangdom_test.cpp
  test-ydom-top.yang            ## for yangdom_test.cpp
  testncx-app-data.yang         ## for yangdom_test.cpp
  testy2p-search-node1.yang     ## for testy2p.cpp
  testy2p-search-node2.yang     ## for testy2p.cpp
  testy2p-top1.yang             ## for testy2p.cpp
  testy2p-top2.yang             ## for testy2p.cpp
  test-field-merge.yang         ## for field merge test
  test-field-merge-b.yang       ## for protobuf message diff test
  test-json-schema-case.yang
)

##
# Yang parsing needed for google tests
##
rift_add_yang_target(
  TARGET yangtools_test_yang_gtest
  YANG_FILES ${yangtools_test_yang_gtest_files}
  WITHOUT_GI
  NO_INSTALL
  LIBRARIES yangtools_gtest_lib rwext_yang_gen ietf_yang_gen
  GIR_PATHS ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/test
  LIBRARY_PATHS ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/test
  DEPENDS yangtools_gtest_lib
)


###############################################################################
##
# Yang files that are parsed to protoc-c as a test of the toolchain.
# These files are not required in order to compile the unit tests.
##
rift_files_prepend_path(
  yangtools_test_yang_testing_files
  ${CMAKE_CURRENT_SOURCE_DIR}

  mangle-base.yang
  mangle-test.yang                # Test some name mangling
  other-config_base.yang          ## sanity
  other-config_ipsec.yang         ## sanity
  other-config_root.yang          ## sanity
  other-config_rwfpath.yang       ## sanity
  other-config_rwfpath_base.yang  ## sanity
  other-config_rwhelloworld.yang  ## sanity
  other-config_rwinit.yang        ## sanity
  other-config_strongswan.yang    ## sanity
  other-data_rwfpath.yang         ## sanity
  other-data_rwinit.yang          ## sanity
  other-rwmanifest.yang           ## sanity
  other-rwuagent-cli.yang         ## sanity
  other-rwzk.yang                 ## sanity
  reuse-grouping-rwmanifest.yang  ## sanity
  test-max-proto-buf-size.yang    ## this is a test of yangpbc for test-max-proto-buf-size
  test-module-ext-another.yang    ## sanity
  test-module-ext-top.yang        ## sanity
  test-module-ext.yang            ## sanity
  testncx-base.yang               ## this is a test of yangncx
  testncx-ext.yang                ## sanity
  testncx-idref-base1.yang        ## this is a test of yangncx
  testncx-idref-base2.yang        ## this is a test of yangncx
  testncx-idref-top1.yang         ## sanity
  testncx-idref-top2.yang         ## sanity
  testncx-mod-a6.yang             ## this is a test of yangncx
  testncx-mod-app-data.yang       ## sanity
  testncx-mod-base1.yang          ## this is a test of yangncx
  testncx-mod-base2.yang          ## this is a test of yangncx
  testncx-mod-base3.yang          ## this is a test of yangncx
  testncx-mod-base4.yang          ## this is a test of yangncx
  testncx-mod-base5.yang          ## this is a test of yangncx
  testncx-mod-top1.yang           ## sanity
  testncx-mod-top2.yang           ## sanity
  testncx-mod-top3.yang           ## sanity
  testncx.yang                    ## this is a test of yangncx
  testycli-ia.yang                ## sanity
  #tailf-raw.yang                 ## this is a test of confdc annotation files
)

##
# Yang parsing that is basically testing the tools
##
rift_add_yang_target(
  TARGET yangtools_test_yang_testing
  YANG_FILES ${yangtools_test_yang_testing_files}
  WITHOUT_GI
  NO_INSTALL
  LIBRARIES rwext_yang_gen ietf_yang_gen
)


###############################################################################
##
# Yang files that are needed for the GI test, but not converted themselves.
##
rift_files_prepend_path(
  yangtools_test_yang_gi_files
  ${CMAKE_CURRENT_SOURCE_DIR}

  rw-3gpp-types-d.yang
  rw-appmgr-d.yang
  rw-appmgr-log-d.yang
  rw-base-d.yang
  rw-composite-d.yang
  rw-dts-api-log-d.yang
  rw-dts-router-log-d.yang
  rw-fpath-d.yang
  rw-fpath-log-d.yang
  rw-log-d.yang
  rw-notify-ext-d.yang
  rw-vcs-d.yang
  rw-ipsec-d.yang
  rwdts-data-d.yang
  rwlog-mgmt-d.yang
  rwmanifest-d.yang
  rw-manifest-d.yang
  rwmsg-data-d.yang
  rwuagent-cli-d.yang
  rw-dts-d.yang
  rw-base-e.yang
  rw-dts-e.yang
  rw-fpath-data-e.yang
  rw-fpath-e.yang
  rw-log-e.yang
  rw-manifest-e.yang
  rw-ncmgr-data-e.yang
  rw-test-au1.yang
  rw-test-au2.yang
  alias-1.yang
  alias-2.yang
  rw-base-f.yang
  ifmgr.yang
  trafdata.yang
  test-py-gi-pb.yang

  test-a-mano-base.yang
  test-a-rw-ipreceiverapp-data.yang
  test-a-rw-vnf-base-types.yang
  test-a-rw-vnf-base-config.yang
  test-a-rw-dest-nat.yang
  test-a-rw-ip-receiver-app.yang
  test-a-rw-vnf-base-opdata.yang
  test-a-rw-sfmgr-data.yang
  test-a-rw-trafgen-data.yang
  test-a-rw-fpath-data.yang
  test-a-composite.yang
  test-output.yang
  company.yang         ## For XMLWalker Tests
  company-augment.yang ## For XMLWalker Tests
  document.yang        ## For documentation - need all artifacts
  test-augment-a1.yang ## for yangpbc_test.cpp
  test-augment-b1.yang ## for yangpbc_test.cpp
  test-json-schema-aug-base.yang
  test-json-schema.yang
  test-vehicle-a.yang  ## for test_gi_pb_yaml_json.py
)

##
# Yang parsing that is basically testing the and GI support
##
rift_add_yang_target(
  TARGET yangtools_test_yang_gi
  YANG_FILES ${yangtools_test_yang_gi_files}
  NO_INSTALL
  LIBRARIES rwext_yang_gen ietf_yang_gen
  DEPENDS RwYang_gi RwKeyspec_gi
)

