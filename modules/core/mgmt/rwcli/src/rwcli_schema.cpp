
/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwcli_schema.cpp
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 10/14/2015
 * @brief Schema Manager for RW.CLI 
 */

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include "rwyangutil.h"
#include "yangncx.hpp"

#include "rwcli_schema.hpp"
#include "rw_sys.h"

#define RIFT_INSTALL_ENV "RIFT_INSTALL"
#define RIFT_VAR_ROOT_ENV "RIFT_VAR_ROOT"

using namespace rw_yang;
namespace fs = boost::filesystem;


const fs::path YANG_DIR{"yang"};

SchemaManager::SchemaManager()
  : SchemaManager(RW_SCHEMA_ROOT_PATH)
{
}

SchemaManager::SchemaManager(const std::string& schema_path)
    : schema_path_(schema_path),
      last_schema_change_(0)
{
  model_.reset(YangModelNcx::create_model());
  RW_ASSERT(model_);

  model_->load_module(RW_BASE_MODEL);

  rift_install_ = getenv(RIFT_INSTALL_ENV);
  if (!rift_install_.length()) {
    rift_install_ = "/";
  }

  const char* rift_var_root = getenv(RIFT_VAR_ROOT_ENV);
  if (!rift_var_root) {
    // Try reading from env.d/ as backup
    rift_var_root = rw_getenv(RIFT_VAR_ROOT_ENV);
    if (!rift_var_root) rift_var_root = "/";
  }

  rift_var_root_ = rift_var_root;
}

bool SchemaManager::load_all_schemas()
{
  // Load all yang modules present in the latest schema version
  // directory.
  fs::path const schema_path = fs::path(rift_var_root_) / fs::path(RW_SCHEMA_VER_LATEST_NB_PATH);
  fs::path const yang_dir = schema_path / YANG_DIR;

  try{
    // check if the latest directory has changed
    fs::path const version_directory = fs::path(rift_var_root_) / fs::path(RW_SCHEMA_VER_PATH);
    std::time_t const version_directory_age = fs::last_write_time(version_directory);
    if ((last_schema_change_ != 0)
        && (last_schema_change_ < version_directory_age)) {
        
      // nothing has changed so there isn't anything to load
      return true;
    } else {
      last_schema_change_ = version_directory_age;
    }
  } catch(fs::filesystem_error) {
    // the version directory doesn't exist
    return false;
  }

  if (!fs::exists(yang_dir)) {
    return false;
  }

  for ( fs::directory_iterator it(yang_dir); it != fs::directory_iterator(); ++it ) {

    auto& path = it->path();

    if (path.extension().string() == ".yang") {

      std::string module_name = path.stem().string();
      if (load_schema (module_name) != RW_STATUS_SUCCESS) {
        return false;
      }
    }
  }

  return true;
}

rw_status_t SchemaManager::load_schema(const std::string& schema_name)
{
  // Schema already loaded will not be reloaded. A modified schema with the same
  // name will be new revision. Revisions are not yet supported.
  YangModule *module = model_->load_module(schema_name.c_str());
  if (module == nullptr) {
    std::cerr << "Loading yang module " << schema_name << " failed\n";
    return RW_STATUS_FAILURE; 
  }

  loaded_schema_.insert(schema_name);

  return RW_STATUS_SUCCESS;
}

void SchemaManager::show_schemas(std::ostream& os)
{
  for (std::string const & schema_name: loaded_schema_) {
    os << "\t\t" << schema_name << std::endl; 
  }
}

std::string SchemaManager::get_cli_manifest_dir()
{
  // ATTN:- Fix this when the dynamic schema supports cli manifest files
  return rift_var_root_ + "/" + schema_path_ + "/cli";
}


