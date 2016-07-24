/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 * Creation Date: 12/11/2015
 * 
 */

#include <set>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include "rwdts.h"
#include "rwdts_api_gi.h"
#include "rwdts_int.h"
#include "rwdts_member_api.h"
#include "rwdts_query_api.h"
#include "rwyangutil.h"

#include "rwdynschema_load_schema.h"

namespace fs = boost::filesystem;

using namespace rw_dyn_schema;

RWLoadSchema::RWLoadSchema(): memlog_inst_("RWLoadSchema", 200)
                            , memlog_buf_(
                                memlog_inst_,
                                "load_schema",
                                reinterpret_cast<intptr_t>(this))
{
  log_ = rwlog_init("RW.LoadSchema");
  RW_ASSERT (log_);
}


RWLoadSchema::~RWLoadSchema()
{
  rwlog_close(log_, false);
  log_ = nullptr;
}

void RWLoadSchema::collect_yang_modules(const fs::path& directory)
{
  RWMEMLOG_TIME_SCOPE (memlog_buf_, RWMEMLOG_MEM2, "collect yang modules");

  RW_ASSERT_MESSAGE (fs::is_directory(directory),
                     "yang modules directory doesn't exist: %s\n",
                     directory.string().c_str());

  const std::array<const char*, 4> exclusion_list = {
    "yuma-ncx",
    "yangdump",
    "yuma-types",
    "yuma-app-common"
  };

  fs::directory_iterator end_iter; // default construction represents past-the-end
  for (fs::directory_iterator current_file(directory);
       current_file != end_iter;
       ++current_file)
  {
    if (!fs::is_regular_file(current_file->path())) {
      auto log_str= "File does not exist: " + current_file->path().string();
      RW_LOG(log_, Error, log_str.c_str());
      continue;
    }
    auto current_module_name = current_file->path().stem().string();
    auto it = std::find(exclusion_list.begin(), exclusion_list.end(),
                        current_module_name);
    if (it != exclusion_list.end()) {
      continue;
    }

    dyn_module_details_t mod_details;
    mod_details.module_name = current_module_name;
    // module_files_t is default initialized
    dyn_modules_vec_.emplace_back(mod_details);
  }
}

void RWLoadSchema::collect_shared_objects(
    const std::set<std::string>& northbound_schema_set)
{
  RWMEMLOG_TIME_SCOPE (memlog_buf_, RWMEMLOG_MEM2, "collect shared objects");

  std::string rift_install_dir = rwyangutil::get_rift_install();
  const std::string& prefix_directory = rift_install_dir + "/usr";

  if (!(fs::exists(prefix_directory) &&
        fs::is_directory(prefix_directory)))
  {
    auto log_str = "Prefix directory not found: " + prefix_directory;
    RW_LOG(log_, Error, log_str.c_str());
    return;
  }

  for (auto& mod_detail: dyn_modules_vec_)
  {
    const std::string& ymodule = mod_detail.module_name;

    std::string so_path = rwyangutil::get_so_file_for_module(ymodule);
    if (!so_path.length()) {
      std::string log_str = "pkg-config failed: ";
      log_str += std::strerror(errno);
      RW_LOG(log_, Error, log_str.c_str());
      continue;
    }

    if (!fs::is_regular_file(so_path)) {
      auto log_str= "SO path not found: " + so_path;
      RW_LOG(log_, Error, log_str.c_str());
      continue;
    }

    mod_detail.module_files.so_file_name = so_path;
    mod_detail.module_files.metainfo_file_name = rift_install_dir + "/usr/data/yang/" +
      ymodule + RW_SCHEMA_META_FILE_SUFFIX;

    // Mark the module for northbound
    if (northbound_schema_set.count(ymodule)) {
      mod_detail.module_files.exported = 1;
    }
  }
}

void RWLoadSchema::populate_rwdynschema_instance(
    rwdynschema_dynamic_schema_registration_t * reg)
{
  reg->batch_size = 0;

  for (auto& mod_detail : dyn_modules_vec_) {
    if (!mod_detail.module_files.so_file_name.length()) continue;

    rwdynschema_module_t mod_info = {
                  &mod_detail.module_name[0],
                  nullptr, // fxs file
                  &mod_detail.module_files.so_file_name[0],
                  nullptr, // yang file
                  &mod_detail.module_files.metainfo_file_name[0],
                  mod_detail.module_files.exported,
    };

    rwdynschema_add_module_to_batch(reg, &mod_info);
  }
}

void rwdynschema_load_all_schema(void * context)
{
  rwdynschema_dynamic_schema_registration_t * reg =
      reinterpret_cast<rwdynschema_dynamic_schema_registration_t *>(context);

  RW_ASSERT(reg);

  bool const created_schema_directory = rw_create_runtime_schema_dir(reg->northbound_listing,
                                                                     reg->n_northbound_listing);

  while (!created_schema_directory) {
    // try again
    rwsched_dispatch_after_f(reg->dts_handle->tasklet,
                             dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * CREATE_SCHEMA_DIR_RETRY_SECS),
                             reg->load_queue,
                             context,
                             rwdynschema_load_all_schema);
                        
    return;
  }

  // make sure the latest directory exists
  std::string rift_install_dir = rwyangutil::get_rift_install();
  fs::path latest_version_directory(rift_install_dir + "/" + RW_SCHEMA_VER_LATEST_PATH);

  RW_ASSERT_MESSAGE(fs::exists(latest_version_directory)
                    && fs::is_directory(latest_version_directory),
                    "schema directory doesn't exist: %s\n",
                    latest_version_directory.c_str());

  // collect yang module names
  fs::path yang_directory = latest_version_directory / "all" / "yang";
  if (reg->app_type == RWDYNSCHEMA_APP_TYPE_NORTHBOUND) {
    yang_directory = latest_version_directory / "northbound" / "yang";
  }

  std::vector<std::string> nb_listings;
  for (int i = 0; i < reg->n_northbound_listing; ++i) {
    nb_listings.emplace_back(reg->northbound_listing[i]);
  }
  std::set<std::string> const northbound_schema_set
    = rwyangutil::read_northbound_schema_listing(
        rift_install_dir,
        nb_listings);

  RWLoadSchema load_schema {};
  load_schema.collect_yang_modules(yang_directory);
  load_schema.collect_shared_objects(northbound_schema_set);
  load_schema.populate_rwdynschema_instance(reg);

  // set YUMA_MODPATH to something reasonable
  auto yuma_modpath = ( latest_version_directory / "all" / "yang" ).string()
                    + ":" + rift_install_dir + "/usr/data/yang";
  size_t const overwrite_existing = 1;
  setenv("YUMA_MODPATH", yuma_modpath.c_str(), overwrite_existing);

  std::string xsd_path = rift_install_dir + "/usr/data/xsd";
  setenv("RIFT_XSD_PATH", xsd_path.c_str(), overwrite_existing);

  // push data through app callback
  RWMEMLOG_TIME_CODE((
      reg->app_sub_cb(reg->app_instance,
                      reg->batch_size,
                      reg->modules)
                      ),
      &reg->memlog_buffer, RWMEMLOG_MEM2, "app_callback");

  // reset batch
  reg->batch_size = 0;
}
