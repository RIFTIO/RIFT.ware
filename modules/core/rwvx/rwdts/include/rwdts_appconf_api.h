
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */


/*!
 * @file rwdts_appconf_api.h
 * @brief DTS AppConf API.
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 *

\page appconf AppConf

\anchor appconf

## Overview of Application Config API

The appconf API provides a framework for apps to register as DTS
Members which consume configuration.  Apps running within the appconf
framework receive a small set of validation and reconciliation
callbacks together with the existing and/or a proposed configuration.

The appconf API operates on the level of a config "group".  This is
any arbitrary grouping of keyspec registrations which are implemented
together as a unit.  Actual config transactions may involve any number
of these groups; each group will execute independently.  Groups may
not have overlapping or shared registrations.  The mechanism is the
same as general registration groups elsewhere in the Member API.

To fit cleanly into DTS configuration transactions, certain resource
assignments and remote communication can be executed as
subtransactions out of the optional appconf-layer prepare callback.
Apart from these explicitly requested per-object prepare events, all
DTS events around configuration changes are consolidated into an
API-driven validate / reconcile / recover configuration state machine.

### Basic sequence

An application wishing to receive configuration should allocate a
group with rwdts_appconf_group_create() and then subscribe to each
schema path needed, using rwdts_appconf_register().  Once all keyspecs
are registered the appconf layer can be told to end the registration
phase, at which point it will retrieve and install the entire group's
configuration and proceed with subscribed configuration updates.

~~~{.c}

 // Declare a group- and transaction-specific "stratch" object to hold
 // state around resource reservations or subtransactions or the like

 typedef struct {
   myapp_router_object *rtobj;
 } myapp_conf1_xact_scratch_t;

 static void* myapp_conf1_xact_init(rwdts_appconf_t *appconf,
                                    rwdts_xact_t *xact,
                                    void *instance) {
   myapp_conf1_xact_scratch_t *scratch = calloc(1, sizeof(*scratch));
   return scratch;
 }
 static void myapp_conf1_xact_deinit(rwdts_appconf_t *appconf,
                                     rwdts_xact_t *xact,
                                     void *instance,
                                     void *scratch) {
   myapp_conf1_xact_scratch_t *s = (myapp_conf1_xact_scratch_t *)scratch;
   if (s) {
     if (s->rtobj) {
       myapp_release_a_reserved_router_object(instance, s->rtobj);
     }
     free(s);
   }
 }


 // Create the config group
 struct rwdts_appconf_cbset {
 } appconf_cbs;
 instance->conf1.appconf = rwdts_appconf_register_group(apih,
                                                        myapp_conf1_xact_init,      // start of a transaction
                                                        myapp_conf1_xact_deinit,    // end of a transaction
                                                        myapp_conf1_validate,       // sanity-check config
                                                        myapp_conf1_apply,          // reconcile / apply config
                                                        instance);                  // groupwide userdata
 :
 :
~~~

#### Registration

~~~{.c}

 // Register the schema elements which compose this config group.  Optionally
 // include prepare callback to process ie resource reservation, subtransaction
 // behavior, etc

 // This looks like it might be a table, but in fact the keyspecs and
 // shard parameters aren't constant, so there's not too much
 // advantage to having a table.

 // Register for routers.
 uint32_t flags = 0;
 const char *key = "/ncmgr/{1}/router/{*}";
 rwdts_appconf_register(instance->conf1.appconf, key, flags, myapp_conf1_router_prepare, NULL);

 // Register for ports
 flags = 0;
 key = "/ncmgr/{1}/port/{*}";
 rwdts_appconf_register(instance->conf1.appconf, key, flags, myapp_conf1_port_prepare, NULL);

 // Register for interfaces
 flags = 0;
 key = "/ncmgr/{1}/port/{*}";
 // We give a NULL prepare callback, so we don't care about doing anything with interfaces before commit time
 rwdts_appconf_register(instance->conf1.appconf, key, flags, NULL, NULL);

 // Signal end of primary registrations.  Publication of this factoid
 // to the world will be deferred until the completion of all
 // asynchronous registration activities.
 rwdts_appconf_phase_complete(instance->conf1.appconf, RWDTS_APPCONF_PHASE_REGISTER);

 // When the above registered configuration has been recovered from
 // the publisher, this config state will be installed into the app
 // with the apply callback, INSTALL action.

~~~


### Sample prepare callback.

   Triggered by the core DTS prepare or CRUD callbacks as appropriate.

   Each callback is given the data for a particular object change.
   The appconf framework deals entirely in configuration updates, so
   in general all keyspecs will be concrete with all key values filled
   in.

   The payload provided will in the future be a delta format called
   "Protobuf delta"; however for now updated objects are presented
   whole.  This gives rise to some unfortunate ambiguities.

   In the PREPARE phase, no action should be actually applied to the
   application's running state, however this is the only opportunity
   to consider the arriving config and reserve resources, communicate
   with other tasks, or execute DTS subqueries.

   The result can be asynchronously returned.  EVERY appconf prepare
   callback must be closed out with exactly one matching
   rwdts_appconf_prepare_complete_ function call.  Thus one can go off
   and do async activities like sending additional DTS queries.

   As with other transaction DTS activities, members should execute
   related DTS queries within the context of the triggering
   transaction.  In general this means that any DTS actions executed
   from a member callback such as these should turn around and pass
   the same rwdts_xact_t "xact" handle back into any other DTS APIs
   invoked, such as rwdts_xact_block_create() or rwdts_member_data
   routines.

   The scratch value is an appconf group specific userdata-themed
   void* value.  Any application state which needs to be held together
   with this transaction until it is committed (or aborted) should be
   kept here.  This example has a "router object" of some sort
   reserved and held in scratch.

~~~{.c}

   void myapp_conf1_router_prepare(apih, appconf, xact, xact_info, keyspec, protomsg, instance, scratch) {
     // the proposed value (maybe nul for delete?)
     MyRouterConf *rt = (MyRouterConf *)protomsg;

     // Our shard is router numbers 10-19, so our key had better be 10-19
     if (rt && (rt->key.number < 10 || rt->key.number > 19)) {
       rwdts_appconf_prepare_complete_na(appconf, xact_info);
       return;
     }

     switch (rwdts_member_get_query_action(xact_info)) {
       default:
       case RWDTS_ACTION_UPDATE:
       case RWDTS_ACTION_DELETE:
          break;
       case RWDTS_ACTION_CREATE:
          scratch->rtobj = myapp_reserve_a_router_object(instance, rt->key.number);
          if (!scratch->rtobj) {
            // Signal that we won't be able to do it
            rwdts_appconf_prepare_complete_fail(appconf, xact_info,
                                                RW_STATUS_NOT_ENOUGH_RESOURCES,
                                                "Unable to reserve a router");
            return;
          }
          break;

      }

      // this may be asynchronously called after going off to do stuff asynchronously
      rwdts_appconf_prepare_complete_ok(appconf, xact_info);
   }

~~~

### Sample validate callback.

   Run at the whim of the appconf API.

   This function examines the config and returns a yes/no indication
   that it is OK.  The config may be running, or may be a candidate
   config.

   The validation does not have to be 100% bulletproof.  However
   anything that validation catches will avoid a traumatic and
   potentially service wobbling rollback/reconciliation operation when
   the config fails to apply.  So there is considerable incentive to
   find it all.

   The validation result is returned with zero or more
   rwdts_appconf_xact_add_issue call(s), each of which returns an
   issue with a particular registration object.  The validation can
   only be completed synchronously, that is all _add_issue calls must
   be made from within the context of the validate function.  If there
   are no errors returned during the lifetime of the validate
   callback, the config is considered to be OK.


~~~{.c}

   void myapp_conf1_validate(apih, appconf, xact, instance, scratch) {

     // iterate over the config and validate
     // we iterate over the set of registrations { and the set of keys { to see it all } } using Rajesh's get-obj and get-next API
     //  + we would maybe prefer to have a monster config tree data structure in protobuf?  maybe that's an option for the future?
     // we can actually see beyond the "conf1" appconfig group into any subscribed config in this apih
     // this is in isolation from other extant config states, is this config kosher with itself and current runtime state?
     // no messaging or dts subtransactions, if this is a proposed config, it is closed
     // note that xact may or may not be NULL, but we don't mind, and the APIs that take an xact all accept NULL for non-transaction-related
     // scratch may be NULL if this is not in a transaction context

     rwdts_member_cursor_t *cur = ??? HOW TO GET CURSOR ???
     MyConfStruct *cs1 = NULL;
     while ((cs1 = rwdts_member_data_get_next_list_element_old(xact, xact, instance->conf1.regh_cs1, cursor)) {
       // the key for cs1 is in cs1 itself, we will know what it is
       if (cs1->monochrome) {
         if (cs1->color != BLACK && cs1->color != WHITE) {
           // Invalid color
           rwdts_appconf_xact_add_issue(appconf, xact, regh, NULL, RW_STATUS_INVALID, "What's with the rainbow?");
         }
       }
     }

     MyServiceStruct2 *cs2 = NULL;
     cs2 = rwdts_member_data_get_obj(apih, xact, instance->conf1.regh_cs2);
     if (cs2->thing) {
       // find some other value(s) and correlate...
     }
     :
     :

   }
~~~


### Sample reconcile callback.

This is invoked by the appconf layer to apply a configuration state.
The new config can be seen as a set of protobuf objects obtainable by
querying the registrations from the apih+xact.

The callback is given an action.  If the action is INSTALL, the app
must replace the current config with the new one, or die trying.  If
the action is RECONCILE, the app should compare all items in the
current config with the new config as seen in the apih+xact, and
update accordingly.

As with the validate callback, errors and warnings are reported via
zero or more calls back into the api of rwdts_appconf_xact_add_issue().

Failure to succeed may cause drastic action.  If a RECONCILE fails,
the configuration will be rejected and the previous configuration
installed; if an INSTALL fails the application will be restarted.

~~~{.c}
 rw_status_t myapp_conf1_apply(apih, appconf, xact, action, instance, scratch) {

    bool force = (action == RWDTS_APPCONF_ACTION_INSTALL);

    if (force) {
      // disregard current configuration, the proposed config MUST now be installed
      delete ud->config;
      ud->config = myapp_build_config_struct_from_xact(instance, apih, xact);
    } else {
      // reconcile and apply the apih+xact configuration into our current running config

      MyConfStruct *cs1_new;
      rwdts_member_cursor_t *cursor = ???;
      while (cs1_new = rwdts_member_data_get_next_list_element_old(apih, xact, instance->conf1.regh_cs1, cursor)) {

        // this is buggy, we need to iterate and do lookups in both directions to spot all comings and goings

        MyConfStruct *cs1 = NULL;
        UTHASH_LOOKUP(instance->config.cs1_hash, &cs1_new->key, cs1);

        if (!cs1 || cs1->thing != cs1_new->thing) {
          // Thing has changed, go change it
          if (!myapp_do_establish_thing_value(cs1_new->thing)) {
            rwdts_appconf_apply_add_issue(xact, instance->conf1.regh_cs1, (rwdts_minikey_t*)&cs1_new->key, RW_STATUS_FAILURE, "I hate this thing");
            goto sad;
          }
        }

        if (!cs1 || cs1->color != cs1_new->color) {
          // Color has changed, go change it
          if (!myapp_do_establish_color_value(cs1_new->color)) {
            rwdts_appconf_apply_add_issue(xact, instance->conf1.regh_cs1, (rwdts_minikey_t*)&cs1_new->key, RW_STATUS_WARNING, "Color not accepted");
            // nonfatal, just a warning...
          }
        }

      }

      MyRouterConf *rtobj = rwdts_member_data_get_obj(apih, xact, instance->conf1.regh_rt);
      if (rtobj && !instance->config.rtobj) {
        // new router object
        assert(scratch->rtobj);
        assert(!instance->rtobj);
        instance->rtobj = myapp_create_a_router_object_from_reservation(instance, scratch->rtobj);
        if (!instance->rtobj) {
          rwdts_appconf_apply_add_issue(xact, instance->conf1.regh_rt, NULL, RW_STATUS_FAILURE, "Unable to create router even though it was reserved!?");
          goto sad;
        }
        instance->config.rtobj = pbapi_copy_object(rtobj);
        assert(instance->config.rtobj);

      } else if (!rtobj && instance->config.rtobj) {
        myapp_conf1_rtobj_delete(instance, instance->config.rtobj);

      } else if (rtobj && instance->config.rtobj) {
        // already had one, compare and update
        :
        :
      }

    }

   return RW_STATUS_SUCCESS;
sad:
   return RW_STATUS_FAILURE;
 }
~~~


 */


#ifndef __RWDTS_APPCONF_API_H
#define __RWDTS_APPCONF_API_H
#include <rwdts_api_gi_enum.h>


__BEGIN_DECLS

/*
 * DTS Appconf API defintiions
 */


/*!
 * rwdts appconf registed callbacks - ATTN need proper documentation
 *
 */
typedef struct  {
  union {
    appconf_xact_init xact_init;
    appconf_xact_init_gi xact_init_gi;
  };
  union {
    appconf_xact_deinit xact_deinit;
    appconf_xact_deinit_gi xact_deinit_gi;
  };
  union {
    appconf_config_validate config_validate;
    appconf_config_validate_gi config_validate_gi;
  };
  union {
    appconf_config_apply config_apply;
    appconf_config_apply_gi config_apply_gi;
  };
  union {
    void *ctx;
    GValue *ctx_gi;
  };

  /* Only for Gi binding */
  GDestroyNotify xact_init_dtor;
  GDestroyNotify xact_deinit_dtor;
  GDestroyNotify config_validate_dtor;
  GDestroyNotify config_apply_dtor;
} rwdts_appconf_cbset_t;



/*!
 * Create an AppConf group.  Gi users please see rwdts_api_appconf_group_create()
 *
 * @param apih The DTS Member API handle.
 * @param cbset A callback set to be copied into the appconf group
 */
rwdts_appconf_t *rwdts_appconf_group_create(rwdts_api_t *apih,
                                            rwdts_xact_t* xact,
                                            rwdts_appconf_cbset_t *cbset);

/*!
 * App conf prepare callback  routine. This callback will be issued by DTS
 * on an incoming config advise xact.
 *
 * @param apih       The DTS API handle
 * @param ac         The appconf group
 * @param xact       DTS transaction handle from original prepare call.
 * @param query      The query handle from the orginal prepare call.
 * @param keyspec    The keyspec associated with this transacton
 * @param pbmsg      The config data associated with this transaction as protobuf
 * @param ctx        The context associated with this registration
 * @param scratch_in ATTN -- need documentation
 *
 */
#if 0
typedef void (*rwdts_appconf_prepare_cb_t)(rwdts_api_t*            apih,
                                           rwdts_appconf_t*        ac,
                                           rwdts_xact_t*           xact,
                                           rwdts_query_handle_t    queryh,
                                           rw_keyspec_path_t*           keyspec,
                                           const ProtobufCMessage* pbmsg,
                                           void*                   ctx,
                                           void*                   scratch_in);
#endif

/*!
 * Make a registration with the appconf group for configuration item(s) to handle
 *
 * @param ac The appconf group
 * @param ks A keyspec to subscribe to
 * @param shard Details around shard routing for this registration
 * @param pbdesc The Protobuf C Descriptor matching the registered keyspec
 * @param flags Registration options
 * @param prepare A prepare callback for this registration.  Can be NULL.
 */
#if 1
rwdts_member_reg_handle_t rwdts_appconf_register(rwdts_appconf_t*                  ac,
                                                 const rw_keyspec_path_t*                     ks,
                                                 rwdts_shard_info_detail_t*        shard,
                                                 const ProtobufCMessageDescriptor* pbdesc,
                                                 uint32_t                          flags,
                                                 rwdts_appconf_prepare_cb_t        prepare);
#endif

/*!
 * Signal to the AppConf layer when an application execute phase is
 * complete.  Currently there is only one phase defined:
 * registrations.  Appconf will proceed to playback / install the
 * current configuration when registration completes.
 * @param ac     The appconf group
 * @param phase RWDTS_APPCONF_PHASE_REGISTER
 */
#if 0
void rwdts_appconf_phase_complete(rwdts_appconf_t*           ac,
                                  enum rwdts_appconf_phase_e phase);
#endif

/*!
 * Signal an error with a prepare action.
 * @param ac     The appconf group
 * @param xact   DTS transaction handle from original prepare call.
 * @param query  The query handle from the orginal prepare call.
 * @param rs The general status code value for the error.
 * @param errstr A human-readable string.  The string is purely for
 *               debugging; production user interfaces will tend be
 *               graphical and/or i18n, so do not rely on the string
 *               in practice.
 */
#if 0
void rwdts_appconf_prepare_complete_fail(rwdts_appconf_t*     ac,
                                         rwdts_xact_t*        xact,
                                         rwdts_query_handle_t query,
                                         rw_status_t          rs,
                                         const char *         errstr);
#endif

/*!
 * Signal the successful treatment of a prepare action.
 * @param ac     The appconf group
 * @param xact   DTS transaction handle from original prepare call.
 * @param query  The query handle from the orginal prepare call.
 */
#if 0
void rwdts_appconf_prepare_complete_ok(rwdts_appconf_t*     ac,
                                       rwdts_xact_t*        xact,
                                       rwdts_query_handle_t query);
#endif
/*!
 * Signal trouble in transaction execution after the prepare phase.
 * While there are individual per-transaction-per-appconf callbacks in
 * these later phases, each phase may still generate any number of
 * issues or errors starting from zero.
 */
#if 0
void rwdts_appconf_xact_add_issue(rwdts_appconf_t*          ac,
                                  rwdts_xact_t*             xact,
                                  rwdts_member_reg_handle_t reg,
                                  /*minikey*/void*          mk,
                                  rw_status_t               rs,
                                  const char*               errstr);
#endif

__END_DECLS

#endif /* __RWDTS_APPCONF_API_H */
