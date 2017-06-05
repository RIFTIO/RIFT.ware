/* STANDARD_RW_IO_COPYRIGHT */

/**
 * @file rwcli_schema_test.cpp
 * @author Balaji Rajappa (balaji.rajappa@riftio.com) 
 * @date 2014/03/16
 * @brief Tests for RwCLI
 */

// Boost filesystem has some ABI issues with C++11 particularly related to
// scoped enums. Use this hack to make it work.
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include "rwlib.h"
#include "rwcli_schema.hpp"
#include "rwut.h"
#include <poll.h>
#include "rwyangutil.h"
#include "rw_sys.h"

namespace fs = boost::filesystem;
using namespace rw_yang;

const std::string TEST_SCHEMA_PATH {"schema"};

class SchemaTestFixture: public ::testing::Test
{
public:
  static void SetUpTestCase() {
    const int overwrite = 1;
    setenv("RIFT_VAR_ROOT", getenv("RIFT_INSTALL"), overwrite);
    rw_setenv("RIFT_VAR_ROOT", getenv("RIFT_INSTALL"));
    // Add the test schema directory to the yang mod path
    fs::path mod_path1 = fs::path(getenv("RIFT_VAR_ROOT")) /
                         fs::path(TEST_SCHEMA_PATH) /
                         fs::path("yang");
                         
    fs::path mod_path2 = fs::path(getenv("RIFT_VAR_ROOT")) / 
                         fs::path(TEST_SCHEMA_PATH) /
                         fs::path("version") /
                         "confd_yang";
    std::string mod_path_env(getenv("YUMA_MODPATH"));
    std::string mod_path_str = mod_path1.native() + ":" +
                               mod_path2.native() + ":";
    mod_path_env.insert(0, mod_path_str.c_str());
    setenv("YUMA_MODPATH", mod_path_env.c_str(), overwrite);
  }

  static void TearDownTestCase() {
  }

  SchemaTestFixture() {
    // dynamic schema paths
    rift_root_ = fs::path(getenv("RIFT_VAR_ROOT"));
    latest_path_ = rift_root_ / std::string(RW_SCHEMA_VER_LATEST_NB_PATH);

    schema_path_ = fs::path(getenv("RIFT_VAR_ROOT")) / fs::path(TEST_SCHEMA_PATH);
    yang_path_   = schema_path_ / "yang";
    ver_path_    = schema_path_ / "version";

    test_yang_src_path_ = fs::path(getenv("RIFT_ROOT")) / 
                          "modules/core/mgmt/rwcli/test/yang_src";
  }

  ~SchemaTestFixture() {
    fs::remove_all(schema_path_);
  }

  virtual void SetUp() {
    // Setup the schema directory.
    auto ret = std::system("rwyangutil --remove-schema-dir --create-schema-dir test_northbound_listing.txt");
    ASSERT_EQ (ret, 0);
  }

  virtual void TearDown(){
    auto ret = std::system("rwyangutil --remove-schema-dir");
    ASSERT_EQ (ret, 0);
  }

  void copy_yang_file(const std::string& filename) {
    // The src files doesn't have yang extension. This is to prevent the
    // rift-shell from adding the src path to the YUMA_MOD_PATH
    fs::path const dest_file = latest_path_ / "yang" / filename;
    fs::path const src_file = test_yang_src_path_ / filename;

    fs::copy_file(src_file, dest_file, fs::copy_option::fail_if_exists);
  }

  void copy_yang_to_ver(const std::string& filename, const std::string& ver_dir) {
    fs::path src_file = test_yang_src_path_ / filename;
    fs::path dest_file = ver_path_ / ver_dir / filename;
    dest_file.replace_extension("yang");
    fs::copy_file(src_file, dest_file, fs::copy_option::fail_if_exists);
  }

  void copy_yang_to_path(const std::string& filename, const fs::path& dest_path) {
    fs::path src_file = test_yang_src_path_ / filename;
    fs::path dest_file = dest_path / filename;
    dest_file.replace_extension("yang");
    fs::copy_file(src_file, dest_file, fs::copy_option::fail_if_exists);
  }

  void create_version_link(const std::string& ver_dir) {
    fs::path src = ver_path_ / ver_dir;
    fs::path link = ver_path_ / "confd_yang";
    fs::create_symlink(src, link);
  }

  fs::path schema_path_;
  fs::path yang_path_;
  fs::path ver_path_;
  fs::path rift_root_;
  fs::path latest_path_;

  fs::path test_yang_src_path_;
};

TEST_F(SchemaTestFixture, StartupSchema) {
  TEST_DESCRIPTION("Test the loading of all schemas during startup");

  SchemaManager mgr(latest_path_.string());
  mgr.load_all_schemas();
  size_t const initial_size = mgr.loaded_schema_.size();

  // widen the northbound schema
  std::string const wide_cmd = "rwyangutil --create-schema-dir test_northbound_widening_listing.txt";

  auto wide_ret = std::system(wide_cmd.c_str());
  ASSERT_EQ (wide_ret, 0);
  bool const did_update = mgr.load_all_schemas();

  EXPECT_TRUE(did_update) << "schema manager didn't do an update";

  EXPECT_LT(initial_size, mgr.loaded_schema_.size());
}

TEST_F(SchemaTestFixture, LoadSchema) {
  TEST_DESCRIPTION("Test Loading a sample schema using SchemaManager");

  const char* test_schema = "rwcli_test";
  SchemaManager mgr(TEST_SCHEMA_PATH);
  rw_status_t rc = mgr.load_schema(test_schema);

  ASSERT_EQ(RW_STATUS_SUCCESS, rc);
  EXPECT_EQ(1, mgr.loaded_schema_.size());

  EXPECT_EQ(mgr.loaded_schema_.count(test_schema), 1);
}
