# RIFT_IO_STANDARD_MAKEFILE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 11/18/2013
# RIFT_IO_STANDARD_MAKEFILE_COPYRIGHT_HEADER(END)

SHELL := /bin/bash
.DEFAULT_GOAL := rw

##
# Set a variable for the top level directory
##


makefile.top := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

# Enter rift-shell to populate modules/toolchain if it doesn't exist
rift-shell := $(shell [ -e modules/toolchain ] || ${makefile.top}/rift-shell -- echo -n "")
platform.out := $(shell $(makefile.top)/modules/toolchain/home/get_platform.sh)

TOP_SRC_PATH := $(makefile.top)

HARNESS_EXE = $(RIFT_INSTALL)/usr/rift/systemtest/harness/harness

CONFD = FULL

##
# Function to get build type
##
ifeq ($(CMAKE_BUILD_TYPE),)
  get_build_type=Debug
else
  get_build_type=$(CMAKE_BUILD_TYPE)
endif

##
# Function to get platform
##
ifeq ($(PLATFORM),)
  get_platform:=$(platform.out)
else
  get_platform=$(PLATFORM)
endif

##
# Set the build directory based on the platform and release flavor
##
RIFT_PLATFORM_DIR := $(TOP_SRC_PATH)/build/$(get_platform)_$(shell echo $(get_build_type) | tr A-Z a-z)
RIFT_BUILD := $(RIFT_PLATFORM_DIR)/build
RIFT_INSTALL := $(RIFT_PLATFORM_DIR)/install/usr/rift
RIFT_ARTIFACTS := $(RIFT_PLATFORM_DIR)/artifacts
RIFT_MODULE_TEST := $(RIFT_ARTIFACTS)/moduletest

RIFT_SHELL_EXE := $(TOP_SRC_PATH)/rift-shell -f --include-build-dirs -d "${TOP_SRC_PATH}" \
	--build-dir "${RIFT_BUILD}" --install-dir "${RIFT_INSTALL}" --artifacts-dir "${RIFT_ARTIFACTS}" --

##
# Function to get coverage build type
##
ifeq ($(COVERAGE_BUILD),)
  is_coverage=FALSE
else
  is_coverage=$(COVERAGE_BUILD)
endif

##
# Function to get developer build type
##
ifeq ($(NOT_DEVELOPER_BUILD),)
  is_not_developer=FALSE
else
  is_not_developer=$(NOT_DEVELOPER_BUILD)
endif

##
# Function to get commit revision to checkout
##
ifeq ($(COMMIT),)
  get_commit=
else
  get_commit=$(COMMIT)
endif

##
# Function to get whether to abort on first unit test failure
##
ifeq ($(ABORT_ON_TEST_FAILURE),)
  get_abort_on_test_failure=1
else
  get_abort_on_test_failure=0
endif

##
# Function to get the rift agent build type.
# Allowed values are CONFD_BASIC, CONFD_FULL, XML_ONLY
##
ifeq ($(RIFT_AGENT_BUILD),)
  get_rift_agent_build=CONFD_FULL
else
  get_rift_agent_build=$(RIFT_AGENT_BUILD)
endif


##
# Function to lookup submodules
##
ifeq ($(SUBMODULE),)
  lookup_submodule=$(error ${newline}ERROR: SUBMODULE=XYZ missing on command line:${newline})
else ifeq ($(findstring modules/, $(SUBMODULE)), modules/)
  lookup_submodule=$(SUBMODULE)
else
  lookup_submodule=$(error ${newline}ERROR: Invalid SUBMODULE=XYZ specifier on command line${newline})
endif

##
# Function to set the package type
# we need RPMs even on DEBIAN when in developer mode for populating the build cache
##
ifeq ($(get_platform),fc20)
  get_package=RPM
else
  ifeq ($(is_not_developer),TRUE)
    get_package=DEB
  else
    get_package=RPM
  endif
endif

.PHONY: all test_vars
all: rw

test_vars:
	@echo 'RIFT_BUILD: $(RIFT_BUILD)'
	@echo 'RIFT_SHELL_EXE: $(RIFT_SHELL_EXE)'
	@echo 'RIFT_ARTIFACTS: $(RIFT_INSTALL)'
	@echo 'RIFT_MODULE_TEST: $(RIFT_MODULE_TEST)'
	@echo 'RIFT_INSTALL: $(RIFT_INSTALL)'

##
#
##
rw.list:
	@echo ''
	@echo '================================================================================'
	@echo ' List of Make targets'
	@echo '================================================================================'
	@echo ''
	@echo '    make rw.checkout SUBMODULE=<submodule>'
	@echo '                         - Generic target to checkout a specific submodule'
	@echo '                             e.g. make rw.checkout SUBMODULE=modules/core/util'
	@echo ''
	@echo '    make rw.submodule SUBMODULE=<submodule>'
	@echo '                         - Generic target to checkout & build a submodule'
	@echo '                             e.g. make rw.submodule SUBMODULE=modules/core/util'
	@echo ''
	@echo '    make rw.checkout.world COMMIT=<superproject Git hash>'
	@echo '                         - Check out the entire tree at a particular superproject hash-state (or branch-tag)'
	@echo ''
	@echo ''
	@echo 'Shortcuts:'
	@echo '    make rw                   			- Just want an incremental build'
	@echo '    make rw.bcache            			- Populate the build cache'
	@echo '    make rw.checkout.world    			- Checkout ALL submodules (whole world)'
	@echo '    make rw.checkout.stack    			- Checkout Openstack submodules'
	@echo '    make rw.core.fpath        			- Core FastPath (checkout & build)'
	@echo '    make rw.core.ipc          			- Core IPC packages (checkout & build)'
	@echo '    make rw.core.mgmt         			- Core mgmt packages (checkout & build)'
	@echo '    make rw.core.schema       			- Core management packages (checkout & build)'
	@echo '    make rw.core.util         			- Core utilities (checkout & build)'
	@echo '    make rw.cscope            			- Generate cscope symbols'
	@echo '                              			  (in top directory)'
	@echo '    make rw.pycscope            			- Generate pycscope symbols'
	@echo '                              			  (in top directory)'
	@echo '    make rw.coverage          			- Run coverage'
	@echo '                              			  (results in ${RIFT_ARTIFACTS}/coverage)'
	@echo '    make rw.pycheck           			- Run simple Python compile-check for scripts under modules/*'
	@echo '    make rw.doxygen           			- Generate doxygen documentation'
	@echo '                              			  (in ${RIFT_BUILD}/documentation dir)'
	@echo '    make rw.docs              			- Documentation (checkout & build)'
	@echo '    make rw.dependency_graph  			- Generate submodule dependency dot graph'
	@echo '                              			  (in ${RIFT_BUILD}/dependency.png)'
	@echo '    make rw.package           			- Generate RPM packages'
	@echo '    make rw.unittest          			- Run the unittests'
	@echo '                              			  (results in ${RIFT_ARTIFACTS}/unittest)'
	@echo '    make rw.unittest_long     			- Run long unittests'
	@echo '                              			  (results in ${RIFT_ARTIFACTS}/unittest)'
	@echo '    make rw.automation.systemtest		- Checkout modules/automation/systemtest but do not run the systemtests'
	@echo '    make rw.sanity            			- Run a single harness smoke test (default: trafgen)'
	@echo '                              			  (takes optional TEST=[trafgen, seagull, ltesim] parameter)'
	@echo '    make rw.systemtest        			- Run the harness smoke tests'
	@echo '    make rw.systemtest_local  			- Run the local systemtest'
	@echo '                              			  (results in ${RIFT_ARTIFACTS}/systemtest)'
	@echo '    make rw.rift             			- Checkout & build rift (no ext)'
	@echo '    make rw.world             			- Checkout & build'
	@echo '    make RIFT_AGENT_BUILD=CONFD_BASIC            - Checkout & build using Confd BASIC version'
	@echo '    make RIFT_AGENT_BUILD=CONFD_FULL             - Checkout & build using Confd FULL version. This is the default option.' 
	@echo '    make RIFT_AGENT_BUILD=XML_ONLY               - Checkout & build without confd support.'
	@echo
	@echo 'Examples w/misc. options:'
	@echo '    make rw VERBOSE=1 CMAKE_BUILD_TYPE=Release'
	@echo '    make rw VERBOSE=1  NOT_DEVELOPER_BUILD=TRUE CMAKE_BUILD_TYPE=Release COVERAGE_BUILD=TRUE'
	@echo ''
	@echo ''
	@echo ''
	@echo 'Image building commands:'
	@echo '    NOTE:  Images require root access and a fully built tree that used NOT_DEVELOPER_BUILD=TRUE'
	@echo '           As we do not want to built the entire tree as root, they do not depend on rw.world, it'
	@echo '           remains up to the caller to first call "make rw.world NOT_DEVELOPER_BUILD=TRUE"'
	@echo '    make rw.ec2-image         			- Image suitable for uploading to EC2'
	@echo '    make rw.kvm-image         			- Image suitable for uploading to OpenStack'
	@echo ''
	@echo 'Instructions to run the trafgen simulation (as of 04/15/2015):'
	@echo '    cd top-of-your-build-dir'
	@echo '    ./rift-shell'
	@echo '    ./modules/automation/systemtest/fpath/demos/trafgen_111.py -c -m ethsim --configure --start-traffic'
	@echo ''
	@echo '    ## To see port statistics:'
	@echo '      show colony trafsink port-state trafsink/5/1 counters'
	@echo '    ## [Here you should see port rx/tx counters, if the test is running successfully...]'
	@echo ''
	@echo 'Smoke-Test Instructions:'
	@echo '    Wiki: http://confluence.eng.riftio.com/display/AUT/Fpath+smoke+test'
	@echo ' Example: ./modules/automation/systemtest/fpath/fp_smoke'
	@echo ''
	@echo ''

##
# Make rule to display help for all targets
##

help:
	@echo '================================================================================'
	@echo 'Makefile targets - the default target is "help"'
	@echo '================================================================================'
	@echo ''
	@echo '    primer  - help message to build source code for the first time'
	@echo '    help    - this message'
	@echo '    cmake   - invoke cmake for the module this directory is in'
	@echo '    rw.list - list of make targets and usage'
	$(RIFT_SHELL_EXE) $(MAKE) rw.list



##
# Make rule to display a primer on how to easily checkout/build the software
##

primer:
	@echo '================================================================================'
	@echo 'RiftWare software build primer'
	@echo '================================================================================'
	@echo ''
	@echo 'Step #1 -- First checkout the software module that you wish to build'
	@echo '--------------------------------------------------------------------------------'
	@echo 'Assuming this is the "rw.core.util" submodule, then:'
	@echo ''
	@echo '$$ make rw.checkout SUBMODULE=rw.core.util'
	@echo ''
	@echo 'If you know the git submodule name, you can also specify:'
	@echo ''
	@echo '$$ make rw.checkout SUBMODULE=modules/core/util'
	@echo ''
	@echo 'Step #2 -- Now run the cmake target'
	@echo '--------------------------------------------------------------------------------'
	@echo 'This makes a build directory, runs cmake, and runs make on the generated files'
	@echo ''
	@echo '$$ make cmake'


##
# Clean up all generated files from previous builds.
##
clean:
	rm -rf $(RIFT_ARTIFACTS)
	rm -rf $(RIFT_INSTALL)
	rm -rf $(RIFT_BUILD)

rw.clean: clean

clean.fast:
	@touch $(RIFT_ARTIFACTS)
	@touch $(RIFT_INSTALL)
	@touch $(RIFT_BUILD)
	@$(eval DELETE := $(shell mktemp -d --tmpdir=$(RIFT_ROOT) .deleteXXXXXX))
	@mv -f $(RIFT_ARTIFACTS) $(DELETE)
	@mv -f $(RIFT_INSTALL) $(DELETE)
	@mv -f $(RIFT_BUILD) $(DELETE)
	@(rm -rf $(DELETE) &>/dev/null &)

##
# Rule to invoke cmake
##
cmake:: BUILD_TYPE=$(call get_build_type)
cmake:: COVERAGE_TYPE=$(call is_coverage)
cmake:: NOT_DEVELOPER_TYPE=$(call is_not_developer)
cmake:: RIFT_AGENT_BUILD=$(call get_rift_agent_build)
cmake:: RELEASE_NUMBER=$(shell cat RELEASE)
cmake:: BUILD_NUMBER=$(shell bin/getbuild build $(get_platform))
cmake::
	@if [ "$(RIFT_AGENT_BUILD)" != "XML_ONLY" ] && [ "$(RIFT_AGENT_BUILD)" != "CONFD_FULL" ] && [ "$(RIFT_AGENT_BUILD)" != "CONFD_BASIC" ]; then \
		echo; \
		echo "ERROR: Invalid value set for RIFT_AGENT_BUILD"; \
		exit 1; \
	fi
	mkdir -p $(RIFT_BUILD)
	mkdir -p $(RIFT_ARTIFACTS)
	mkdir -p $(RIFT_MODULE_TEST)
	mkdir -p $(RIFT_INSTALL)
	cd $(RIFT_BUILD) && $(RIFT_SHELL_EXE) cmake $(TOP_SRC_PATH) -DCMAKE_INSTALL_PREFIX=$(RIFT_INSTALL) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DNOT_DEVELOPER_BUILD=$(NOT_DEVELOPER_TYPE) -DCOVERAGE_BUILD=$(COVERAGE_TYPE) -DRIFT_AGENT_BUILD=$(RIFT_AGENT_BUILD) -DRELEASE_NUMBER=$(RELEASE_NUMBER) -DBUILD_NUMBER=$(BUILD_NUMBER) -DRIFT_PACKAGE_GENERATOR=$(get_package)

##
# Rule to checkout non-external components
##
rw.checkout.rift: CHANGESET=$(call get_commit)
rw.checkout.rift:
	git xinit -e modules/ext/*
	git xcheckout $(CHANGESET)

##
# Rule to checkout all components
##
rw.checkout.world: CHANGESET=$(call get_commit)
rw.checkout.world:
	git xinit
	git xcheckout $(CHANGESET)

##
# Rule for rw.checkout
#
# This is done with a "git submodule init" followed by a "git submodule update"
# Then checkout the master branch of the source code
##
rw.checkout::	SUBMODULE_DIR=$(call lookup_submodule)
rw.checkout:: CHANGESET=$(call get_commit)
rw.checkout::
	git xinit -s $(SUBMODULE_DIR)
	git xcheckout $(CHANGESET)

##
# Generic code to checkout submodule and make it
##
rw.submodule:: SUBMODULE_DIR=$(call lookup_submodule)
rw.submodule:: CHANGESET=$(call get_commit)
rw.submodule:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=$(SUBMODULE_DIR) COMMIT=$(CHANGESET)
	$(RIFT_SHELL_EXE) $(MAKE) rw

##
# Shortcut checkout/make rules for various modules
#
# These commands are shortcuts to checkout and build the specified submodule
##
rw.core.util:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/core/util
	$(RIFT_SHELL_EXE) $(MAKE) rw

rw.core.fpath:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/core/fpath
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/automation/systemtest
	$(RIFT_SHELL_EXE) $(MAKE) rw

rw.docs:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/docs
	$(RIFT_SHELL_EXE) $(MAKE) rw

##
# SAMPLE target for making a tar file of the exportable-docs
# as of this moment, no documents are exportable, so this is just a placeholder
##
rw.export_docs: rw.docs
	tar -c -h -C $(RIFT_INSTALL)/documentation -f $(RIFT_INSTALL)/documents.tar riftio/pdf/riftio_distributed_fpath.pdf config

rw.core.mgmt:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/core/mgmt
	$(RIFT_SHELL_EXE) $(MAKE) rw

rw.core.ipc:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/core/ipc
	$(RIFT_SHELL_EXE) $(MAKE) rw

rw.core.rwvx:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/core/rwvx
	$(RIFT_SHELL_EXE) $(MAKE) rw

rw.automation.systemtest:
	-$(RIFT_SHELL_EXE) $(MAKE) rw.checkout SUBMODULE=modules/automation/systemtest
	$(RIFT_SHELL_EXE) $(MAKE) rw

core_fpath: cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) $@

core_mgmt: cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) $@

core_util: cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) $@

core_rwvx: cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) $@


#
# Rule to checkout and build rift 
#
rw.world: rw.checkout.world
	$(RIFT_SHELL_EXE) $(MAKE) rw


#
# Rule to checkout and build rift without external packages
#
rw.rift: rw.checkout.rift
	$(RIFT_SHELL_EXE) $(MAKE) rw

#
# Rule to run the systemtest smoke test via the harness
#
rw.systemtest:
	$(RIFT_SHELL_EXE) $(HARNESS_EXE) run -i smoke_stable --serial --stdout 


#
# Get the harness test name from the TEST= make argument (default is trafgen)
# This will convert between a simple test name (trafgen, ltesim, seagull)
# into the corresponding harness test name (passed to harness via --name parameter)
# These test names are found in the respective .racfg test configuration files
#
ifeq ($(TEST),)
  get_sanity_test=^TC_TRAFGEN111_0100$$
else ifeq ($(TEST), trafgen)
  get_sanity_test=^TC_TRAFGEN111_0100$$
else ifeq ($(TEST), ltesim)
  get_sanity_test=^TC_LTESIMCOMBINED_0101$$
else ifeq ($(TEST), seagull)
  get_sanity_test=^TC_SEAGULL_0001$$
endif

#
# Rule to run a single test via the harness
#
rw.sanity:: HARNESS_TEST=$(call get_sanity_test)
rw.sanity::
	$(RIFT_SHELL_EXE) $(HARNESS_EXE) run --no-user --serial --stdout --name $(HARNESS_TEST)

##
# Rule to invoke systemtest locally
##
rw.systemtest_local: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) RIFT_NO_SUDO_REAPER=1 rw.systemtest


#
# Rule to fix the permissions in the .install directory after running a demo as root
#
rw.fix_perms:
	$(RIFT_SHELL_EXE) $(HARNESS_EXE) run --serial --stdout --name FIX_INSTALL_PERMISSIONS_9001

##
# Rule to create the combined foss.html from all foss.txt files in
# installed submodules.
##
rw.foss: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.foss


##
# This target runs the cmake step.
# The cmake is invoked under the following two conditions:
#  - code was checked out and the cmake step was never invoked
#  - cmake step was invoked once, however a new submodule
#    was checked out since then
##
rw.cmake:
	if [[ ! -f $(RIFT_BUILD)/Makefile ]] ; then \
	  $(MAKE) cmake; \
	else \
	  grep "path = " .gitmodules | awk '{print $$3}' | \
	    while read submodule; do \
	      if [[ -f $$submodule/CMakeLists.txt ]] ; then \
	        if [[ ! -d $(RIFT_BUILD)/$$submodule ]] ; then \
	          $(MAKE) cmake; \
			  break; \
	        fi; \
	      fi; \
	    done; \
	fi; \

# Rule to invoke the incremental build
# This should be invoked after a target that already invoked "make cmake"
# For example after the "make rw.core.util" is invoked first, one can just
# invoke "make rw"
# NOTE: This will not rebuild the external projects in submodules
##
rw: rw.cmake
	git rev-parse --abbrev-ref HEAD >$(RIFT_INSTALL)/.git_status
	git rev-parse HEAD >>$(RIFT_INSTALL)/.git_status
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD)

##
# Rule to invoke the incremental build
# This should be invoked after a target that already invoked "make cmake"
# For example after the "make rw.core.util" is invoked first, one can just
# invoke "make rw-dammit"
# NOTE: This will rebuild the external projects in submodules.
##
rw.dammit: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.dammit

##
# Rule to invoke the clean build
# This should be invoked after a target that already invoked "make cmake"
# For example after the "make rw.core.util" is invoked first, one can just
# invoke "make rw.clean_and_rebuid"
# NOTE: This will remove the current install directory and submodule
# build directories and build everything from scratch
##
rw.clean_and_rebuild: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.clean_and_rebuild

##
# Rule generate doxygen documentation
##
rw.doxygen: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.doxygen

##
# Rule to generate dependency graph
##
rw.dependency_graph: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.dependency_graph

##
# Rule to invoke unittest
##
rw.unittest: ABORT_ON_TEST_FAILURE=$(call get_abort_on_test_failure)
rw.unittest: rw.cmake
	@if [ "$(shell ulimit -c)" == "0" ]; then \
		ulimit -S -c unlimited; \
	fi && \
	ABORT_ON_TEST_FAILURE=$(ABORT_ON_TEST_FAILURE) $(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.unittest

##
# Rule to invoke unittest
##
rw.unittest_long: ABORT_ON_TEST_FAILURE=$(call get_abort_on_test_failure)
rw.unittest_long: rw.cmake
	@if [ "$(shell ulimit -c)" == "0" ]; then \
		ulimit -S -c unlimited; \
	fi && \
	ABORT_ON_TEST_FAILURE=$(ABORT_ON_TEST_FAILURE) $(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.unittest_long
##
# Rule to invoke python checks
##
rw.pycheck:
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.pycheck

##
# Rule to invoke coverage target
##
rw.coverage: ABORT_ON_TEST_FAILURE=$(call get_abort_on_test_failure)
rw.coverage: export COVERAGE_BUILD = TRUE
rw.coverage: rw.cmake
	@if [ "$(shell ulimit -c)" == "0" ]; then \
		ulimit -S -c unlimited; \
	fi && \
	ABORT_ON_TEST_FAILURE=$(ABORT_ON_TEST_FAILURE) $(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.coverage

##
# Rule to generate cscope symbols
##
rw.cscope:
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.cscope

##
# Rule to generate pycscope symbols
##
rw.pycscope:
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.pycscope

##
# Rule to generate ctags
##
rw.ctags:
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.ctags

##
# Rule for rw.package
rw.package: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.package

##
# Rule for generating build cache
##
rw.bcache: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rw.bcache

##
# Rule for generating EC2 images
##
rw.ec2-image: cmake
	@if [ "$(NOT_DEVELOPER_BUILD)" != "TRUE" ]; then \
		echo; \
		echo "ERROR:  Images must be built with NOT_DEVELOPER_BUILD=TRUE"; \
		exit 1; \
	fi
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) ec2-image

##
# Rule for generating KVM images
##
rw.kvm-image: cmake
	@if [ "$(NOT_DEVELOPER_BUILD)" != "TRUE" ]; then \
		echo; \
		echo "ERROR:  Images must be built with NOT_DEVELOPER_BUILD=TRUE"; \
		exit 1; \
	fi
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) kvm-image
#
rw.rpmbuild: rw.cmake
	$(RIFT_SHELL_EXE) $(MAKE) -C $(RIFT_BUILD) rpmbuild

# Rule to install packages built in local workspace
.PHONY: install-platform-packages
install-platform-packages:
	./modules/tools/scripts/installers/install-riftware --local platform-confd-basic
