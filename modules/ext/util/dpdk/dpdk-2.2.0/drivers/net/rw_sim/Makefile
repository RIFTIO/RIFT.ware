# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Sanil Puthiyandyil
# Creation Date: 6/12/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_sim.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rw_pmd_sim_version.map
LIBABIVER := 2

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
