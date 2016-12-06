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
# Author(s): Sanil Puthiyandyil
# Creation Date: 6/12/2014
# 

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_sim.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_RING) += rw_pmd_eth_sim.c rw_virt_eth_switch.c

#
# Export include files
#
SYMLINK-y-include += rw_pmd_eth_sim.h rw_virt_eth_switch.h

# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_RING) += lib/librte_eal lib/librte_ring
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_RING) += lib/librte_mbuf lib/librte_ether

include $(RTE_SDK)/mk/rte.lib.mk
