#   BSD LICENSE
# 
#   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
#   All rights reserved.
# 
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
# 
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librw_piot.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3 
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linux/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include
CFLAGS += -I$(RTE_SDK)/lib/librte_mempool
CFLAGS += -I$(RTE_SDK)/lib/librte_malloc
CFLAGS += -I$(RTE_SDK)/lib/librte_ether
CFLAGS += -I$(RTE_SDK)/lib/librte_pmd_ring
CFLAGS += -I$(RTE_SDK)/lib/librte_pmd_pcap
CFLAGS += -I$(RTE_SDK)/lib/librte_kni
CFLAGS += -I$(RTE_SDK)/lib/librw_pmd_raw
CFLAGS += -I$(RTE_SDK)/lib/librw_pmd_tun

EXPORT_MAP := rw_piot_version.map
LIBABIVER := 2

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRW_PIOT) := rw_piot.c rw_piot_api.c rw_piot_kni.c rw_piot_setup.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRW_PIOT)-include := rw_piot.h rw_piot_api.h

# this lib needs eal
DEPDIRS-$(CONFIG_RTE_LIBRW_PIOT) += lib/librte_eal lib/librte_mbuf
#lib/librte_kni


include $(RTE_SDK)/mk/rte.lib.mk
