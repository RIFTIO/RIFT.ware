#
# Version: 1.6
#
# Copyright (C) 2008-2014 Intel Corporation.  All Rights Reserved.
#
#     This file is part of SEP Development Kit
#
#     SEP Development Kit is free software; you can redistribute it
#     and/or modify it under the terms of the GNU General Public License
#     version 2 as published by the Free Software Foundation.
#
#     SEP Development Kit is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with SEP Development Kit; if not, write to the Free Software
#     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#     As a special exception, you may use this file as part of a free software
#     library without restriction.  Specifically, if other files instantiate
#     templates or use macros or inline functions from this file, or you compile
#     this file and link it with other files to produce an executable, this
#     file does not by itself cause the resulting executable to be covered by
#     the GNU General Public License.  This exception does not however
#     invalidate any other reasons why the executable file might be covered by
#     the GNU General Public License.
#

# -------------------- user configurable options ------------------------

# base name of SEP driver
DRIVER_NAME = sep3_15

# location to install driver
INSTALL = .

# current working directory
DRV_CWD := $(shell pwd)

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
# Example flags are "-Werror", "-Wno-error", etc.
EXTRA_CFLAGS += -I$(LDDINCDIR) -I$(LDDINCDIR1)

# if ARCH variable is set, unset it to avoid conflicts with kbuild
unexport ARCH

# platform details
MACH ?= $(shell uname -m)
export MACH
ifeq ($(MACH),x86_64)
PLATFORM=x32_64
endif
ifeq ($(PLATFORM),)
PLATFORM=x32
endif
KERNEL_VERSION ?= $(shell uname -r)
SMP ?= $(shell uname -v | grep SMP)
ARITY=up
ifneq ($(SMP),)
ARITY=smp
endif

ifeq ($(PER_USER_MODE),YES)
SEP_PER_USER=-pu
endif

KERNEL_VERSION_NMI ?= $(shell uname -r | cut -f1-1 -d -)
# set this variable to 'NMI' to enable NMI Interrupts
INTERRUPT_MODE=
ifeq ($(INTERRUPT_MODE),NMI)
    ifeq ($(KERNEL_VERSION_NMI),3.2.0)
        $(info NOTE: NMI Interrupt Mode is set on $(KERNEL_VERSION) kernel)
        INTERRUPT_MODE=NMI
    else
        HIGHER_VERSION=$(shell echo -e "$(KERNEL_VERSION_NMI)\n3.2.0" | cut -f1-1 -d - | sort -g -t '.' | tail --lines=1)
        ifeq ($(HIGHER_VERSION),3.2.0)
            $(info WARNING: NMI Interrupt Mechanism is not supported on $(KERNEL_VERSION) kernel. Switching to PMI Mode.)
            INTERRUPT_MODE=
        else
            $(info NOTE: NMI Interrupt Mode is set on $(KERNEL_VERSION) kernel)
            INTERRUPT_MODE=NMI
        endif
    endif
endif

# eventual filename of SEP driver
DRIVER_MODE=$(DRIVER_NAME)$(SEP_PER_USER)
DRIVER_TYPE=$(PLATFORM)-$(KERNEL_VERSION)$(ARITY)
DRIVER_FILENAME=$(DRIVER_MODE)-$(DRIVER_TYPE).ko

# By default, build the EMON portions of the driver
ifeq ($(EMON),)
	EMON=YES
endif

# build options ...
ifneq ($(KERNELRELEASE),)
	obj-m := $(DRIVER_NAME).o

ifeq ($(PLATFORM),x32)
	arch-objs :=    sys32.o
endif
ifeq ($(PLATFORM),x32_64)
	arch-objs :=    sys64.o
endif

ifeq ($(PER_USER_MODE),YES)
    EXTRA_CFLAGS += -DSECURE_SEP
endif

ifeq ($(EMON),YES)
    EXTRA_CFLAGS += -DEMON -DEMON_INTERNAL
endif

ifeq ($(INTERRUPT_MODE),NMI)
    EXTRA_CFLAGS += -DDRV_USE_NMI
endif

$(DRIVER_NAME)-objs :=    \
		lwpmudrv.o        \
		control.o         \
		cpumon.o          \
		eventmux.o        \
		linuxos.o         \
		output.o          \
		pmi.o             \
		sys_info.o        \
		utility.o         \
		chap.o            \
		gmch.o            \
		core2.o           \
		perfver4.o        \
		corei7_unc.o      \
		unc_wbox.o        \
		silvermont.o      \
		pci.o             \
		apic.o            \
		pebs.o            \
		unc_gt.o          \
		unc_client_imc.o  \
		unc_ncu.o         \
		unc_pcu.o         \
		unc_ha.o          \
		unc_cbo.o         \
		unc_ubox.o        \
		unc_power.o       \
		unc_common.o      \
		unc_qpi.o         \
		unc_r3qpi.o       \
		unc_imc.o         \
		unc_r2pcie.o      \
		unc_irp.o         \
		unc_sbox.o         \
		sepdrv_p_state.o  \
		valleyview_sochap.o    \
		$(private-objs)   \
		$(arch-objs)

# targets ...

# Otherwise, we were called directly from the command
# line, so the kernel build system will be used.
else
	KERNEL_SRC_DIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

all: default

default:
	$(MAKE) -C $(KERNEL_SRC_DIR) M=$(PWD) LDDINCDIR=$(PWD)/../include LDDINCDIR1=$(PWD)/inc modules PWD=$(PWD)
	cp $(DRIVER_NAME).ko $(DRIVER_FILENAME)
	@(cd pax && make all)
	@if [ -d vtsspp ]; then          \
		(cd vtsspp && make all); \
	fi;

endif

install:
    ifneq ($(DRV_CWD),$(INSTALL))
		@mkdir -p $(INSTALL)
		@cp $(DRIVER_FILENAME) $(INSTALL)/$(DRIVER_FILENAME)
		@cp insmod-sep3 $(INSTALL)/insmod-sep3
		@cp rmmod-sep3 $(INSTALL)/rmmod-sep3
		@echo "Installed $(DRIVER_NAME) driver to $(INSTALL)/$(DRIVER_FILENAME) ."
		@(cd pax && mkdir -p $(INSTALL)/pax && make install INSTALL=$(INSTALL)/pax)
    endif
	@if [ -d vtsspp ]; then                                        \
		(cd vtsspp && make install INSTALL=$(INSTALL)/vtsspp); \
	fi;

clean:
	rm -f *.o .*.o.cmd .*.o.d .*.ko.cmd .*.ko.unsigned.cmd *.gcno
	rm -f $(DRIVER_NAME).ko $(DRIVER_NAME).ko.unsigned
	rm -f $(DRIVER_MODE)*$(DRIVER_TYPE).ko
	rm -f Module.symvers Modules.symvers *.mod.c modules.order Module.markers
	rm -rf .tmp_versions
	@(cd pax && make clean)
	@if [ -d vtsspp ]; then            \
		(cd vtsspp && make clean); \
	fi;

distclean: clean
	rm -f $(DRIVER_NAME)*.ko
	@(cd pax && make distclean)
	@if [ -d vtsspp ]; then                \
		(cd vtsspp && make distclean); \
	fi;
