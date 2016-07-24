/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_piot.h
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 02/03/2014
 * @brief Network Fastpath Packet I/O Tool Kit (PIOT) utility routines
 * 
 * @details
 */

#ifndef _RW_PIOT_H_
#define _RW_PIOT_H_

#include <rw_piot_api.h>

typedef struct { 
    void                   *instance_ptr;     /***< fpath pointer initializing */
    f_rw_piot_log_t        log_fn;
    struct internal_config *eal_internal_config;
    rw_piot_device_t       device[RWPIOT_MAX_DEVICES];
} rw_piot_global_config_t;



#if 0

/*
 * handle and index are of type unit32_t
 */
#define RWPIOT_GET_INDEX_FROM_HANDLE(handle)  ((handle) % (RWPIOT_MAX_DEVICES))

/*
 * Genarate a non-zero handle from the index
 * To prevent and detect accidental reuse of old handles, a (random number * max_index) is added to the
 * index. so that finding the index is a simple modulo operation * max_index (RWPIOT_MAX_DEVICES) 
 * is defined as a number multiple of 2
 */
#define RWPIOT_GENERATE_HANDLE(index, handle)                                                   \
                                    {                                                           \
                                      int r;                                                    \
                                      do {                                                      \
                                        while (0==(r=rand()));                                  \
                                        (handle) = (((RWPIOT_MAX_DEVICES) * r) + index);        \
                                      } while (0==handle);                                      \
                                    }   

#define RWPIOT_GET_DEVICE(handle)                                                               \
    ((rw_piot_global_config.device[RWPIOT_GET_INDEX_FROM_HANDLE(handle)].piot_api_handle ==      \
     (handle))? &(rw_piot_global_config.device[RWPIOT_GET_INDEX_FROM_HANDLE(handle)]) : NULL)

#else

#define RWPIOT_GENERATE_HANDLE(dev_ptr)   ((void *)(dev_ptr))

#endif

rw_piot_device_t *rw_piot_device_alloc(rw_piot_devgroup_t *group);

int rw_piot_device_free(rw_piot_device_t *dev);

int
rw_piot_config_device(rw_piot_device_t *dev,
                      rw_piot_open_request_info_t *req,
                      rw_piot_open_response_info_t *rsp);
#endif
