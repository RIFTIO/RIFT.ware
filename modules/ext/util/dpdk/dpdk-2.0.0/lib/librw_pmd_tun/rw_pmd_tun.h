#ifndef _RW_PMD_TUN_H_
#define _RW_PMD_TUN_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_RW_PMD_TUN_PARAM_NAME "eth_tun"

#define RW_PMD_TUN_IFNAME_LEN 64

#ifndef ASSERT
#define ASSERT assert
#endif

/**
 * For use by the EAL only. Called as part of EAL init to set up any dummy NICs
 * configured on command line.
 */
   
int rw_pmd_tun_init(const char *name, const char *params); 
#ifdef __cplusplus
}
#endif

#endif
