/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file   rwdts_appstats_api.h
 * @brief  DTS stats API Framework
 * @author Rajesh Velandy
 * @date   02/12/2015
 *
 * @details

\anchor DTSAppStats
\anchor appdata

## Overview of DTSAppStats Framework

The DTSAppStats  provides a framework for apps to register as DTS
members to provide their statics and counters to DTS Query consumers.


The DTSAppStats framework allows the app to register keyspecs
and provide the stats in a more app friendly proto derived raw structures
than the full protobuf derived C structures.

### Basic sequence

An application using the DTSAppStats framework registers with DTSAppStats
framework with the keyspec  Using one  of the following APIs.
The API choice will determine how DTSAppStats framework gets the
statistics information from the application.

1) When rwdts_appstats_register() is used with raw proto, the application provides
   pointer to the raw proto structure containing the statistics/counters.
   This structure need to be maintained in the same scheduler queue where the DTS
   instance is bound to. The keyspec specified when this API is used should also
   be fully keyed.

2) rwdts_appstats_register() isAPI registers a get_next_stats() callback
   that DTSAppStats can invoke to get the statistics at the registration keyspec.
   This keyspec can be wildcarded at the end for listy registrations, butr need
   to be fully keyed otherwise.

### DTS registrations

Register rawpb

~~~{.c}


// The following is the structure where the stats are maintained
RWPB_T_RAW_MSG(SomeModule_data_App_Subscriber_stat) *mystat = get_my_stat_reference();

// Register for subscriber stats

rwdts_keyspec_t *keyspec = _nosuchapi_keyspec_from_path("/app[name="xyz"/subscriber[name="abc"]/stats");
// Register with DTS
rwdts_member_reg_handle_t  regh = rwdts_appstats_register(_nosuchapi_keyspec_from_path,
                                                          RWPB_G_MSG_PBCMD(SomeModule_data_App_Subscriber_stat),
                                                          NULL,
                                                          mystat,
                                                          sizeof(*mystat),
                                                          NULL);
~~~

or register  callback

~~~{.c}

// Define a callback for providing stats
rwdts_member_rsp_code_t get_next_subscriber_stat(rwdts_appstats_context_t*        ctx,
                                                 rw_keyspec_path_t**                   keyspec,
                                                 ProtobufCMessage**               pbcm,
                                                 void*                            ud)
{
  rwdts_member_rsp_code_t rsp;
  if (*keyspec == NULL) {
   rsp = get_first_subscriber_stat(keyspec, pbcm);
  } else {
   rsp = get_next_subscriber_stat(keyspec, pbcm);
  }
  return rsp;
}


// Register for subscriber stats
rwdts_keyspec_t *keyspec = _nosuchapi_keyspec_from_path("/app[name="xyz"/subscriber[name="abc"]/stats");

// User context provided in callback
myappcontext = get_my_app_context();

// Define callback implementation
rwdts_appstat_reg_cb_t appstat_cb =  {
  .get_next_stat = get_next_subscriber_stat,
  .ud = myappcontext;
};

// Register with DTS
rwdts_member_reg_handle_t  regh = rwdts_appstats_register(_nosuchapi_keyspec_from_path,
                                                          RWPB_G_MSG_PBCMD(SomeModule_data_App_Subscriber_stat),
                                                          NULL,
                                                          NULL,
                                                          0,
                                                          &appstat_cb);
~~~

### Dregister
~~~{.c}
 // Deregister a registration handle obtained through rwdts_appstats_register()
 rw_status_t rs = rwdts_appstats_deregister(regh);
~~~

 */




#ifndef __RWDTS_APPSTATS_API_H
#define __RWDTS_APPSTATS_API_H

#include <rwdts.h>

__BEGIN_DECLS

/*
 * DTS AppStats API definitions
 */

/*!
 * Maintains the DTSAppStats context
 */
typedef struct rwdts_appstats_context_s {
  rwdts_xact_t*                     xact;  /*!< DTS transaction handle */
  rwdts_query_handle_t              query; /*!< DTS query handle */
   rwdts_member_registration_t*     reg;   /*!< Registration handle */
} rwdts_appstats_context_t;

/*!
 *
 * DTSAppStat invokes this callback when it receives a request matching a keyspec
 * that was registered using the rwdts_appstats_register() method.
 * The callback will be continously invoked until the application responds back with
 * NULL indicating end of responses.
 *
 * @param ctx     DTSAppStats contex.
 * @param reg     The macthing registration context.
 * @param keyspec The keyspec of the previous element -- NULL for the first call.
 * @param pbcm    The protobuf assocaited with this registration.
 * @param ud      The user data associated with the registration.
 */

typedef  rwdts_member_rsp_code_t (*rwdts_get_next_stats_cb_t) (rwdts_appstats_context_t*        ctx,
                                                               rw_keyspec_path_t**                   keyspec,
                                                               ProtobufCMessage**               pbcm,
                                                               void*                            ud);

/*!
 * rwdts appstat registration structure
 *
 */

typedef struct rwdts_appstat_reg_s {
  rwdts_get_next_stats_cb_t  get_next_stats; /*!< callback for getting next stats */
  void*                      ud;             /*!< The user conext associated with the registration */
} rwdts_appstat_reg_cb_t;


/*!
 * Register raw protobuf pointer or callback with the DTSAppStat framework  using this API.
 * This registration can be either in terms of raw protobuf pointer or a callback
 *
 * @param keysepc    The keyspec associated with this registration.
 * @param pbdesc     The Protobuf C Descriptor matching the registered keyspec.
 * @param shard      Sharding detail associated with this registration.
 * @param rawpb      The raw protobuf pointer associated with this registration.
 * @param rawpb_size The size of the rawpb structure.  The size here should match.
 *                   with the size of the  raw pb structure associated with this registration.
 * @param appstat    The callback associated with this registration
 *
 * @return           dts member registration handle if this registration was successfulll, null otheriwse
 */
rwdts_member_reg_handle_t
rwdts_appstats_register(const rw_keyspec_path_t*               keyspec,
                        const ProtobufCMessageDescriptor* pbdesc,
                        rwdts_shard_info_detail_t*        shard,
                        void*                             rawpb,
                        size_t                            rawpb_size,
                        rwdts_appstat_reg_cb_t*           appstat);

/*!
 * Register  raw protobuf pointer with the DTSAppStat framework  using this API.
 *
 * The passed keyspec should be fully keyed and the  passed raw protobuf
 * must be maintained in DTS's concurrency context.
 * Before  the raw protobuf pointer is deleted, the registration with
 * DTS must be removed by calling rwdts_appstats_deregister.
 *
 * @param keysepc    The keyspec associated with this registration.
 * @param pbdesc     The Protobuf C Descriptor matching the registered keyspec.
 * @param shard      Sharding detail associated with this registration.
 * @param rawpb      The raw protobuf pointer associated with this registration.
 * @param rawpb_size The size of the rawpb structure.  The size here should match.
 *                   with the size of the  raw pb structure associated with this registration.
 * @param appstat    The callback associated with this registration
 *
 * @return           dts member registration handle if this registration was successfulll, null otheriwse
 *
 */
rwdts_member_reg_handle_t
rwdts_appstats_register_rawpb(const rw_keyspec_path_t*               keyspec,
                              const ProtobufCMessageDescriptor* pbdesc,
                              rwdts_shard_info_detail_t*        shard,
                              void*                             rawpb,
                              size_t                            rawpb_size,
                              rwdts_appstat_reg_cb_t*           appstat);

/*!
 * Register callback with the DTSAppStat framework  using this API.
 *
 * DTSAppStats framework will invoke the registered callback on receiving
 * a query that matched the registered keyspec.
 *
 * @param keysepc    The keyspec associated with this registration.
 * @param pbdesc     The Protobuf C Descriptor matching the registered keyspec.
 * @param shard      Sharding detail associated with this registration.
 * @param rawpb      The raw protobuf pointer associated with this registration.
 * @param rawpb_size The size of the rawpb structure.  The size here should match.
 *                   with the size of the  raw pb structure associated with this registration.
 * @param appstat    The callback associated with this registration
 *
 * @return           dts member registration handle if this registration was successfulll, null otheriwse
 *
 */
rwdts_member_reg_handle_t
rwdts_appstats_register_callback(const rw_keyspec_path_t*               keyspec,
                                 const ProtobufCMessageDescriptor* pbdesc,
                                 rwdts_shard_info_detail_t*        shard,
                                 void*                             rawpb,
                                 size_t                            rawpb_size,
                                 rwdts_appstat_reg_cb_t*           appstat);

/*!
 * Deregister a DTSAppStats framework registration
 *
 * @param regh The registration handle to dergister
 *
 */

rw_status_t
rwdts_appstats_deregister(rwdts_member_reg_handle_t regh);


__END_DECLS

#endif /* __RWDTS_APPSTATS_API_H */
