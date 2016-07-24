
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <getopt.h>
#include <rw-manifest.pb-c.h>
#include <rwlib.h>
#include <rwsched_main.h>
#include <rwtasklet.h>
#include <rwvcs_defs.h>
#include <rwvcs_manifest.h>
#include <rwvcs_rwzk.h>
#include <rwvx.h>

#include "rwmain.h"
#include "rwmain_gi.h"
#include "rw-manifest.pb-c.h"
#include "reaper_client.h"

struct rwmain_gi * rwmain_gi_old(rwpb_gi_RwManifest_Manifest * manifest_box);

static struct rwmain_gi * rwmain_gi_ref(struct rwmain_gi * rwmain_gi) {
  rwmain_gi->_refcnt++;
  return rwmain_gi;
}

static void rwmain_gi_unref(struct rwmain_gi * rwmain_gi) {
  rwmain_gi->_refcnt--;
  if (!rwmain_gi->_refcnt) {
    struct rwmain_tasklet * rt;
    struct rwmain_tasklet * bkup;

    RW_SKLIST_FOREACH_SAFE(rt, struct rwmain_tasklet, &(rwmain_gi->tasklets), _sklist, bkup)
      rwmain_tasklet_free(rt);

    rwtasklet_info_unref(rwmain_gi->tasklet_info);
    rwvx_instance_free(rwmain_gi->rwvx);
    free(rwmain_gi);
  }
}

G_DEFINE_BOXED_TYPE(rwmain_gi_t,
                    rwmain_gi,
                    rwmain_gi_ref,
                    rwmain_gi_unref);


void sanitize_manifest(vcs_manifest * manifest) {
  if (!manifest->init_phase) {
    manifest->init_phase = (vcs_manifest_init *)malloc(sizeof(vcs_manifest_init));
    RW_ASSERT(manifest->init_phase);
    vcs_manifest_init__init(manifest->init_phase);
  }

  if (!manifest->init_phase->settings) {
    manifest->init_phase->settings = (vcs_manifest_settings *)malloc(sizeof(vcs_manifest_settings));
    RW_ASSERT(manifest->init_phase->settings);
    vcs_manifest_settings__init(manifest->init_phase->settings);
  }

  if (!manifest->init_phase->settings->rwmsg) {
    manifest->init_phase->settings->rwmsg = (vcs_manifest_rwmsg *)malloc(sizeof(vcs_manifest_rwmsg));
    RW_ASSERT(manifest->init_phase->settings->rwmsg);
    vcs_manifest_rwmsg__init(manifest->init_phase->settings->rwmsg);
  }

  if (!manifest->init_phase->settings->rwvcs) {
    manifest->init_phase->settings->rwvcs = (vcs_manifest_rwvcs *)malloc(sizeof(vcs_manifest_rwvcs));
    RW_ASSERT(manifest->init_phase->settings->rwvcs);
    vcs_manifest_rwvcs__init(manifest->init_phase->settings->rwvcs);
  }

  if (!manifest->init_phase->settings->rwvcs->has_collapse_each_rwprocess) {
    manifest->init_phase->settings->rwvcs->has_collapse_each_rwprocess = true;
    manifest->init_phase->settings->rwvcs->collapse_each_rwprocess = true;
  }
  if (!manifest->init_phase->settings->rwvcs->has_collapse_each_rwvm) {
    manifest->init_phase->settings->rwvcs->has_collapse_each_rwvm = true;
    manifest->init_phase->settings->rwvcs->collapse_each_rwvm = true;
  }
}

vcs_manifest * g_pb_rwmanifest = NULL; /* unit test hack */
static int rwmain_gi_function(int argc, char ** argv, char ** envp)
{
  rwpb_gi_RwManifest_Manifest manifest_box;

  memset(&manifest_box, sizeof(manifest_box), 0);
  manifest_box.box.message = (struct ProtobufCMessage*)g_pb_rwmanifest;
  if (g_pb_rwmanifest->init_phase 
      && g_pb_rwmanifest->init_phase->environment) {
    char *component_name = NULL;
    char *component_type = NULL;
    uint32_t instance_id = 0;
    uint32_t vm_instance_id = 0;
    char *parent = NULL;
    char *manifest = NULL;
    char * ip_address =  NULL;
    int got_opts = 0;

    optind = 0;
    while (true) {
      int c;
      long int lu;

      static struct option long_options[] = {
        {"manifest",      required_argument,  0, 'm'},
        {"name",          required_argument,  0, 'n'},
        {"instance",      required_argument,  0, 'i'},
        {"type",          required_argument,  0, 't'},
        {"parent",        required_argument,  0, 'p'},
        {"ip_address",    required_argument,  0, 'a'},
        {"vm_instance",   required_argument,  0, 'v'},
        {"help",          no_argument,        0, 'h'},
        {0,               0,                  0,  0},
      };

      c = getopt_long(argc, argv, "m:n:i:t:p:a:v:h", long_options, NULL);
      if (c == -1)
        break;

      switch (c) {
        case 'n':
          component_name = strdup(optarg);
          got_opts++;
          break;

        case 'i':
          errno = 0;
          lu = strtol(optarg, NULL, 10);
          RW_ASSERT(errno == 0);
          RW_ASSERT(lu > 0 && lu < UINT32_MAX);
          instance_id = (uint32_t)lu;
          got_opts++;
          break;

        case 't':
          component_type = strdup(optarg);
          got_opts++;
          break;

        case 'p':
          parent = strdup(optarg);
          got_opts++;
          break;

        case 'v':
          errno = 0;
          lu = strtol(optarg, NULL, 10);
          RW_ASSERT(errno == 0);
          RW_ASSERT(lu > 0 && lu < UINT32_MAX);
          vm_instance_id = (uint32_t)lu;
          got_opts++;
          break;

        case 'm':
          if (optarg) {
            manifest = strdup(optarg);
            RW_ASSERT(manifest);
          }
          break;

        case 'a':
          if (optarg) {
            ip_address = strdup(optarg);
            RW_ASSERT(ip_address);
          }
          break;

        case 'h':
          break;
      }
    }
    if (got_opts) {
      int vars;
      for (vars = 0; 
           vars < g_pb_rwmanifest->init_phase->environment->n_python_variable;
           vars++) {
        RW_FREE(g_pb_rwmanifest->init_phase->environment->python_variable[vars]);
      }
      if (g_pb_rwmanifest->init_phase->environment->n_python_variable != got_opts) {
        RW_FREE(g_pb_rwmanifest->init_phase->environment->python_variable);
        g_pb_rwmanifest->init_phase->environment->n_python_variable = got_opts;
        g_pb_rwmanifest->init_phase->environment->python_variable = RW_MALLOC0(got_opts * sizeof(g_pb_rwmanifest->init_phase->environment->python_variable[0]));
      }
      int r;
      vars = 0;
      if (component_name) {
        if (!g_pb_rwmanifest->init_phase->environment->component_name
            || (g_pb_rwmanifest->init_phase->environment->component_name
                && (!strstr(g_pb_rwmanifest->init_phase->environment->component_name, "$python(rw_component_name)")
                    || strcmp("$python(rw_component_name)", 
                              strstr(g_pb_rwmanifest->init_phase->environment->component_name, "$python(rw_component_name)"))))) {
          RW_FREE(g_pb_rwmanifest->init_phase->environment->component_name);
          g_pb_rwmanifest->init_phase->environment->component_name = strdup("$python(rw_component_name)");
        }
        r = asprintf(&g_pb_rwmanifest->init_phase->environment->python_variable[vars],
                     "rw_component_name = '%s'", component_name);
        vars++;
        RW_ASSERT(r > 0);
        RW_FREE(component_name);
      }
      if (component_type) {
        if (!g_pb_rwmanifest->init_phase->environment->component_type
            || (g_pb_rwmanifest->init_phase->environment->component_type
                && (!strstr(g_pb_rwmanifest->init_phase->environment->component_type, "$python(component_type)")
                    || strcmp("$python(component_type)", 
                              strstr(g_pb_rwmanifest->init_phase->environment->component_type, "$python(component_type)"))))) {
          RW_FREE(g_pb_rwmanifest->init_phase->environment->component_type);
          g_pb_rwmanifest->init_phase->environment->component_type = strdup("$python(component_type)");
        }
        r = asprintf(&g_pb_rwmanifest->init_phase->environment->python_variable[vars],
                     "component_type = '%s'", component_type);
        vars++;
        RW_ASSERT(r > 0);
        RW_FREE(component_type);
      }
      if (instance_id) {
        if (!g_pb_rwmanifest->init_phase->environment->instance_id
            || (g_pb_rwmanifest->init_phase->environment->instance_id
                && (!strstr(g_pb_rwmanifest->init_phase->environment->instance_id, "$python(instance_id)")
                    || strcmp("$python(instance_id)", 
                              strstr(g_pb_rwmanifest->init_phase->environment->instance_id, "$python(instance_id)"))))) {
          RW_FREE(g_pb_rwmanifest->init_phase->environment->instance_id);
          g_pb_rwmanifest->init_phase->environment->instance_id = strdup("$python(instance_id)");
        }
        r = asprintf(&g_pb_rwmanifest->init_phase->environment->python_variable[vars],
                     "instance_id = %u", instance_id);
        vars++;
        RW_ASSERT(r > 0);
      }
      if (vm_instance_id) {
        if (!g_pb_rwmanifest->init_phase->environment->vm_instance_id
            || (g_pb_rwmanifest->init_phase->environment->vm_instance_id
                && (!strstr(g_pb_rwmanifest->init_phase->environment->vm_instance_id, "$python(vm_instance_id)")
                    || strcmp("$python(vm_instance_id)", 
                              strstr(g_pb_rwmanifest->init_phase->environment->vm_instance_id, "$python(vm_instance_id)"))))) {
          RW_FREE(g_pb_rwmanifest->init_phase->environment->vm_instance_id);
          g_pb_rwmanifest->init_phase->environment->vm_instance_id = strdup("$python(vm_instance_id)");
        }
        r = asprintf(&g_pb_rwmanifest->init_phase->environment->python_variable[vars],
                     "vm_instance_id = %u", vm_instance_id);
        vars++;
        RW_ASSERT(r > 0);
      }
      if (parent) {
        if (!g_pb_rwmanifest->init_phase->environment->parent_id
            || (g_pb_rwmanifest->init_phase->environment->parent_id
                && (!strstr(g_pb_rwmanifest->init_phase->environment->parent_id, "$python(parent_id)")
                    || strcmp("$python(parent_id)", 
                              strstr(g_pb_rwmanifest->init_phase->environment->parent_id, "$python(parent_id)"))))) {
          RW_FREE(g_pb_rwmanifest->init_phase->environment->parent_id);
          g_pb_rwmanifest->init_phase->environment->parent_id = strdup("$python(parent_id)");
        }
        r = asprintf(&g_pb_rwmanifest->init_phase->environment->python_variable[vars],
                     "parent_id = '%s'", parent);
        vars++;
        RW_ASSERT(r > 0);
        RW_FREE(parent);
      }
      RW_ASSERT (vars == g_pb_rwmanifest->init_phase->environment->n_python_variable);
    }
    optind = 0;
  }

  manifest_box.box.message  = protobuf_c_message_duplicate(
                    NULL, &g_pb_rwmanifest->base,
                    g_pb_rwmanifest->base.descriptor); /* used for module / unit test hack */

  rwmain_gi_new(&manifest_box);
  return 0;
}

struct rwmain_gi * rwmain_gi_new(rwpb_gi_RwManifest_Manifest * manifest_box) 
{
  rw_status_t status;
  struct rwmain_gi * rwmain;
  struct rwvx_instance_s * rwvx;
  vcs_manifest * manifest;
  extern char **environ;
  char *s;
  int i;
  char * parent = NULL;
  char * ip_address = NULL;

  if (!getenv("TEST_ENVIRON")) {
    return rwmain_gi_old(manifest_box);
  }

  rwvx = rwvx_instance_alloc();
   
  RW_ASSERT(rwvx);

  s = *environ;
  for (i=0; s; i++) {
    rwvx->rwvcs->envp = realloc(rwvx->rwvcs->envp, (i+1)*sizeof(char*));
    rwvx->rwvcs->envp[i] = strdup(s);
    s = *(environ+i+1);
  }
  rwvx->rwvcs->envp = realloc(rwvx->rwvcs->envp, (i+1)*sizeof(char*));
  rwvx->rwvcs->envp[i] = NULL;

 
  RW_ASSERT(manifest_box->box.message->descriptor == RWPB_G_MSG_PBCMD(RwManifest_Manifest));
  manifest = (vcs_manifest *)manifest_box->box.message;
  sanitize_manifest(manifest);
  char *manifest_file = getenv("RW_MANIFEST");
  if (!manifest_file) {
    rwvx->rwvcs->pb_rwmanifest = (vcs_manifest *)protobuf_c_message_duplicate(
      NULL,
      &manifest->base,
      manifest->base.descriptor);
  }
  if (g_pb_rwmanifest) {
    protobuf_c_message_free_unpacked(NULL, &g_pb_rwmanifest->base);
  }
  g_pb_rwmanifest = (vcs_manifest *)protobuf_c_message_duplicate(
                    NULL, &manifest->base,
                    manifest->base.descriptor); /* used for module / unit test hack */

  char cn[1024];

  status = rwvcs_variable_evaluate_str(
      rwvx->rwvcs,
      "$rw_component_name",
      cn,
      sizeof(cn));

  rwvcs_process_manifest_file (rwvx->rwvcs, getenv("RW_MANIFEST"));
  if(!(cn[0])) {
    ip_address = rwvcs_manifest_get_local_mgmt_addr(rwvx->rwvcs->pb_rwmanifest->bootstrap_phase);
    RW_ASSERT(ip_address);
  }
  rwvcs_manifest_setup_mgmt_info (rwvx->rwvcs, !(cn[0]), ip_address);
  if (manifest_file) {
     status = rwvcs_instance_init(rwvx->rwvcs, 
                               getenv("RW_MANIFEST"), 
                               ip_address, 
                               main_function);
  }
  else {
    status = rwvcs_instance_init(rwvx->rwvcs, NULL, ip_address, rwmain_gi_function);
  }
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  char *component_name = NULL;
  char *component_type = NULL;
  uint32_t instance_id = 0;
  uint32_t vm_instance_id = 0;
  rwmain = rwmain_alloc(rwvx, component_name, instance_id, component_type, parent, ip_address, vm_instance_id);
  RW_ASSERT(rwmain);
  {
    rwmain->rwvx->rwvcs->apih = rwmain->dts;
    RW_SKLIST_PARAMS_DECL(
        config_ready_entries_,
        rwvcs_config_ready_entry_t,
        instance_name,
        rw_sklist_comp_charptr,
        config_ready_elem);
    RW_SKLIST_INIT(
        &(rwmain->rwvx->rwvcs->config_ready_entries), 
        &config_ready_entries_);
    rwmain->rwvx->rwvcs->config_ready_fn = rwmain_dts_config_ready_process;
  }
    

  if (rwmain->vm_ip_address) {
    rwmain->rwvx->rwvcs->identity.vm_ip_address = strdup(rwmain->vm_ip_address);
    RW_ASSERT(rwmain->rwvx->rwvcs->identity.vm_ip_address);
  }
  rwmain->rwvx->rwvcs->identity.rwvm_instance_id = rwmain->vm_instance_id;

  if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
    RW_ASSERT(VCS_GET(rwmain)->instance_name);
    rwmain->rwvx->rwvcs->identity.rwvm_name = strdup(VCS_GET(rwmain)->instance_name);
  }
  else if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWPROC) {
    RW_ASSERT(rwmain->parent_id);
    rwmain->rwvx->rwvcs->identity.rwvm_name = strdup(rwmain->parent_id);
  }

  {
    char instance_id_str[256];
    snprintf(instance_id_str, 256, "%u", rwmain->vm_instance_id);
  }

  if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM && parent!=NULL) {
    rwvcs_instance_ptr_t rwvcs = rwvx->rwvcs;
    struct timeval timeout = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };
    rw_component_info rwvm_info;
    char * instance_name = NULL;
    instance_name = to_instance_name(component_name, instance_id);
    RW_ASSERT(instance_name!=NULL);
    // Lock so that the parent can initialize the zk data before the child updates it
    status = rwvcs_rwzk_lock(rwvcs, instance_name, &timeout);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    printf("instance_nameinstance_nameinstance_nameinstance_nameinstance_name=%s\n", instance_name);
    status = rwvcs_rwzk_lookup_component(rwvcs, instance_name, &rwvm_info);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    RW_ASSERT(rwvm_info.vm_info!=NULL);
    rwvm_info.vm_info->has_pid = true;
    rwvm_info.vm_info->pid = getpid();
    status = rwvcs_rwzk_node_update(rwvcs, &rwvm_info);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    status = rwvcs_rwzk_unlock(rwvcs, instance_name);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    free(instance_name);
  } else if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM && parent == NULL) {
  }

  status = rwmain_bootstrap(rwmain);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  init_phase (NULL, (void *)rwmain);

  return rwmain;

}

struct rwmain_gi * rwmain_gi_old(rwpb_gi_RwManifest_Manifest * manifest_box) {
  rw_status_t status;
  struct rwmain_gi * rwmain_gi;
  struct rwvx_instance_s * rwvx;
  struct rwvcs_instance_s * rwvcs;
  struct rwtasklet_info_s * tinfo;
  vcs_manifest * manifest;

  rwmain_gi = (struct rwmain_gi *)malloc(sizeof(struct rwmain_gi));
  if (!rwmain_gi) {
    RW_CRASH();
    return NULL;
  }

  RW_SKLIST_PARAMS_DECL(
      tasklets_sklist_params,
      struct rwmain_tasklet,
      instance_name,
      rw_sklist_comp_charptr,
      _sklist);
  RW_SKLIST_INIT(&(rwmain_gi->tasklets), &tasklets_sklist_params);

  rwvx = rwvx_instance_alloc();
  if (!rwvx) {
    RW_CRASH();
    goto free_gi;
  }
  rwvcs = rwvx->rwvcs;

  RW_ASSERT(manifest_box->box.message->descriptor == RWPB_G_MSG_PBCMD(RwManifest_Manifest));
  manifest = (vcs_manifest *)manifest_box->box.message;
  rwvcs->pb_rwmanifest = (vcs_manifest *)protobuf_c_message_duplicate(
      NULL,
      &manifest->base,
      manifest->base.descriptor);
  if (!rwvcs->pb_rwmanifest) {
    RW_CRASH();
    goto free_rwvx;
  }
  sanitize_manifest(rwvcs->pb_rwmanifest);

  status = rwcal_rwzk_zake_init(rwvx->rwcal_module);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    goto free_rwvx;
  }

  status = rwvcs_rwzk_seed_auto_instance(rwvcs, 1000, NULL);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    goto free_rwvx;
  }

  rwvcs->identity.rwvm_instance_id = 1;
  rwvcs->identity.vm_ip_address = strdup("127.0.0.1");
  if (!rwvcs->identity.vm_ip_address) {
    RW_CRASH();
    goto free_rwvx;
  }

  tinfo = (struct rwtasklet_info_s *)malloc(sizeof(struct rwtasklet_info_s));
  if (!tinfo) {
    RW_CRASH();
    goto free_rwvx;
  }

  tinfo->rwsched_instance = rwvx->rwsched;
  tinfo->rwsched_tasklet_info = rwsched_tasklet_new(rwvx->rwsched);
  tinfo->rwtrace_instance = rwvx->rwtrace;
  tinfo->rwvx = rwvx;
  tinfo->rwvcs = rwvx->rwvcs;
  tinfo->identity.rwtasklet_instance_id = 1;
  tinfo->identity.rwtasklet_name = strdup("rwmain_gi");
  tinfo->rwlog_instance = rwlog_init("rwmain_gi-1");
  tinfo->rwmsg_endpoint = rwmsg_endpoint_create(
      1,
      1,
      1,
      tinfo->rwsched_instance,
      tinfo->rwsched_tasklet_info,
      tinfo->rwtrace_instance,
      rwvcs->pb_rwmanifest->init_phase->settings->rwmsg);
  tinfo->ref_cnt = 1;


  rwmain_gi->_refcnt = 1;
  rwmain_gi->tasklet_info = tinfo;
  rwmain_gi->rwvx = rwvx;
  return rwmain_gi;

free_rwvx:
  if (rwvx->rwvcs->pb_rwmanifest)
    free(rwvx->rwvcs->pb_rwmanifest);
  free(rwvx->rwvcs);
  free(rwvx);

free_gi:
  free(rwmain_gi);

  return NULL;
}

rw_status_t rwmain_gi_add_tasklet(
    struct rwmain_gi * rwmain_gi,
    const char * plugin_dir,
    const char * plugin_name)
{
  rw_status_t status;
  rwvcs_instance_ptr_t rwvcs;
  uint32_t instance_id;
  char * instance_name;
  struct rwmain_tasklet * tasklet;
  rwmain_tasklet_mode_active_t mode_active = {};

  rwvcs = rwmain_gi->rwvx->rwvcs;


  status = rwvcs_rwzk_next_instance_id(rwvcs, &instance_id, NULL);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }

  instance_name = to_instance_name(plugin_name, instance_id);

  tasklet = rwmain_tasklet_alloc(
      instance_name,
      instance_id,
      &mode_active,
      plugin_name,
      plugin_dir,
      rwvcs);
  RW_FREE(instance_name);
  instance_name = NULL;
  if (!tasklet)
    return RW_STATUS_FAILURE;


  tasklet->plugin_interface->instance_start(
      tasklet->plugin_klass,
      tasklet->h_component,
      tasklet->h_instance);

  status = RW_SKLIST_INSERT(&(rwmain_gi->tasklets), tasklet);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    goto free_tasklet;
  }

  return status;

free_tasklet:
  rwmain_tasklet_free(tasklet);

  return RW_STATUS_FAILURE;
}

rwtasklet_info_t * rwmain_gi_get_tasklet_info(rwmain_gi_t * rwmain_gi)
{
  return rwmain_gi->tasklet_info;
}

struct rwtasklet_info_s * rwmain_gi_new_tasklet_info(
    struct rwmain_gi * rwmain_gi,
    const char * name,
    uint32_t id)
{
  struct rwtasklet_info_s * tinfo;
  char * instance_name;

  tinfo = (struct rwtasklet_info_s *)malloc(sizeof(struct rwtasklet_info_s));
  if (!tinfo) {
    RW_CRASH();
    return NULL;
  }

  instance_name = to_instance_name(name, id);

  tinfo->rwsched_instance = rwmain_gi->rwvx->rwsched;
  tinfo->rwsched_tasklet_info = rwsched_tasklet_new(rwmain_gi->rwvx->rwsched);
  tinfo->rwtrace_instance = rwmain_gi->rwvx->rwtrace;
  tinfo->rwvx = rwmain_gi->rwvx;
  tinfo->rwvcs = rwmain_gi->rwvx->rwvcs;
  tinfo->identity.rwtasklet_instance_id = id;
  tinfo->identity.rwtasklet_name = strdup(name);
  tinfo->rwlog_instance = rwlog_init(instance_name);
  tinfo->rwmsg_endpoint = rwmsg_endpoint_create(
      1,
      id,
      1,
      tinfo->rwsched_instance,
      tinfo->rwsched_tasklet_info,
      tinfo->rwtrace_instance,
      rwmain_gi->rwvx->rwvcs->pb_rwmanifest->init_phase->settings->rwmsg);
  tinfo->ref_cnt = 1;
  tinfo->apih = NULL;
  

  free(instance_name);
  return tinfo;
}

rwtasklet_info_t *
rwmain_gi_find_taskletinfo_by_name(struct rwmain_gi * rwmain,
                   const char * tasklet_instance)
{
  if (!(rwmain&&tasklet_instance)) return NULL;

  struct rwmain_tasklet * rt = NULL;
  rw_status_t status = 
    RW_SKLIST_LOOKUP_BY_KEY (&(rwmain->tasklets), &tasklet_instance, &rt);

  if (RW_STATUS_SUCCESS != status) { 
    rwmain_trace_crit(
        rwmain,
        "Unknown tasklet %s: %d",
        tasklet_instance,
        status);
  }
  return rt?rt->tasklet_info:NULL;
}

rwmain_tasklet_mode_active_t *
rwmain_gi_taskletinfo_get_mode_active(rwtasklet_info_t *tinfo)
{
  return &(tinfo->mode);
}


