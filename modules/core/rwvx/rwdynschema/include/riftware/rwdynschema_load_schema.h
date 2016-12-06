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
 * Creation Date: 12/10/2015
 * 
 */

#ifndef RWDYNSCHEMA_LOAD_SCHEMA_H__
#define RWDYNSCHEMA_LOAD_SCHEMA_H__

#  if defined(__cplusplus)
extern "C" {
#  endif

  static const int MEMLOG_MAX_PATH_SIZE = 128;

  void rwdynschema_load_all_schema(void * context);
#  if defined(__cplusplus)
}
#  endif

# if defined (__cplusplus)

#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include "rwlog.h"
#include "rwmemlog.h"
#include "rwdynschema.h"
#include "rw-dynschema-log.pb-c.h"

#define RW_DYNSCH_Paste3(a,b,c) a##_##b##_##c

#define RW_DYNSCH_INST_LOG_Step1(__instance__, evvtt, ...)                  \
  RWLOG_EVENT(__instance__, evvtt, (uint64_t)__instance__, __VA_ARGS__)

#define RW_LOG(__instance__, evvtt, ...) \
  RW_DYNSCH_INST_LOG_Step1(__instance__, RW_DYNSCH_Paste3(RwDynschemaLog,notif,evvtt), __VA_ARGS__)

static const int CREATE_SCHEMA_DIR_RETRY_SECS = 2;

namespace fs = boost::filesystem;

namespace rw_dyn_schema {

/**
 * RWLoadSchema
 * Provides APIs to load yang modules from filesystem
 * and in creating DTS transaction for dynamic loading
 * of the schema by the intrested apps.
 */
class RWLoadSchema
{
public:
  RWLoadSchema();
  RWLoadSchema(const RWLoadSchema&) = delete;
  void operator=(const RWLoadSchema&) = delete;
  ~RWLoadSchema();

public:
  /**
   * Finds the yang modules for 'all' schema
   * and initializes them in a container.
   */
  void collect_yang_modules(const fs::path&);

  /**
   * Stores the shared libraries to the corresponding
   * yang modules and sets the exported flag based on whether the
   * module was part of schema listing file.
   */
  void collect_shared_objects(const std::set<std::string>&);

  /**
   * Prepares the DTS transaction data for dynamic schema
   * loading by intrested apps.
   */
  void populate_rwdynschema_instance(
      rwdynschema_dynamic_schema_registration_t*);

private:
  struct module_files_t {
    std::string so_file_name;       // Name of shared object file
    std::string metainfo_file_name; // Name of the file having schema files hash values
    size_t exported = 0;            // If set, module is exported to northbound
  };

  struct dyn_module_details_t {
    std::string module_name;
    module_files_t module_files;
  };
  std::vector<dyn_module_details_t> dyn_modules_vec_;

  RwMemlogInstance memlog_inst_;
  RwMemlogBuffer memlog_buf_;

  rwlog_ctx_t* log_ = nullptr;

};

};

#endif
#endif

