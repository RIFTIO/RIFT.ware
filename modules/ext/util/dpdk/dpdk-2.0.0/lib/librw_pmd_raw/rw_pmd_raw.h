#ifndef _RW_PMD_RAW_H_
#define _RW_PMD_RAW_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_RW_PMD_RAW_PARAM_NAME "eth_raw"

#define RW_PMD_RAW_IFNAME_LEN 64

#ifndef ASSERT
#define ASSERT assert
#endif

/**
 * For use by the EAL only. Called as part of EAL init to set up any dummy NICs
 * configured on command line.
 */
   
int rw_pmd_raw_init(const char *name, const char *params); 
#ifdef __cplusplus
}
#endif

#endif
