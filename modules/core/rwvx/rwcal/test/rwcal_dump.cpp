
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file cal_dump
 * @author Jeremy Mordkoff
 * @date 05/14/2015 
 * @brief test program to dump what we can glean from an installation
 */


#include <limits.h>
#include <cstdlib>
#include <iostream>

#include "rwcal-api.h"


int main(int argc, char ** argv, char ** envp)
{

#if 0
    rw_status_t status;
    rwcal_module_ptr_t m_mod;
    Rwcal__YangData__Rwcal__Flavorinfo__FlavorinfoList  *flavor;
    rwpb_gi_Rwcal_FlavorInfo *flavors;
    Rwcal__YangData__Rwcal__Flavorinfo *flavorinfo;
    unsigned int i;
    char url[128];

    if (argc != 4 ) {
    	fprintf(stderr, "args are IP user password\n");
    	return(1);
    }
    snprintf(url, 128, "http://%s:35357/v2.0/tokens", argv[1] );

    m_mod = rwcal_module_alloc();
    status = rwcal_cloud_init(m_mod, RW_MANIFEST_RWCAL_CLOUD_TYPE_OPENSTACK_AUTH_URL, argv[2], argv[3], url );
    if (status != RW_STATUS_SUCCESS)
      return status;

    status = rwcal_cloud_flavor_infos(m_mod, &flavors);
    if (status != RW_STATUS_SUCCESS)
      return status;
    flavorinfo = flavors->s.message;
    printf("ID                                       NAME             MEM    DISK VCPU PCI  HP TC\n");
    printf("---------------------------------------- ---------------- ------ ---- ---- ---- -- --\n");
    for (i = 0; i<flavorinfo->n_flavorinfo_list; i++) {
      flavor = flavorinfo->flavorinfo_list[i];
      printf("%-40s %-16s %6d %4d %4d %4d %2d %2d\n", flavor->id, flavor->name, flavor->memory, flavor->disk, flavor->vcpus, flavor->pci_passthru_bw, 
              flavor->has_huge_pages, flavor->trusted_host_only );
    }

    rwcal__yang_data__rwcal__flavorinfo__gi_unref(flavors);
#endif
    return 0;

}

