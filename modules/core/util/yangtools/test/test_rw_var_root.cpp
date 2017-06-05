/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file test_rw_var_root.cpp
 * @date 2016/06/06
 * @brief Google test cases for rift var root
 */

#include <limits.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include "rwut.h"
#include "rwlib.h"
#include "rw_var_root.h"

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

namespace fs = boost::filesystem;

static std::string get_defualt_rvn()
{
  int rc;

  rc = std::system("hostname -s > test.txt");
  EXPECT_EQ ( 0,  rc );

  char hn[256] = {};
  std::ifstream("test.txt").getline(hn, 256);

  rc = std::system("id -ru > test.txt");
  EXPECT_EQ ( 0,  rc );

  char uid[256] = {};
  std::ifstream("test.txt").getline(uid, 256);

  rc = std::system("rm -rf test.txt");
  EXPECT_EQ ( 0,  rc );

  return hn;
}

TEST(RiftVarRoot, GetRiftVarName)
{
  char rvn[1024] = {};

  rw_status_t s = rw_util_get_rift_var_name("12", "tg", "vm1", rvn, 1024);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);
  EXPECT_STREQ (rvn, "vm1");

  setenv("RIFT_VAR_NAME", "100/sg/vm2", 1);
  s = rw_util_get_rift_var_name("12", "tg", "vm1", rvn, 1024);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);
  EXPECT_STREQ (rvn, "100/sg/vm2");

  unsetenv("RIFT_VAR_NAME");

  setenv("RIFT_VAR_ROOT", "/usr/rift/var/rift/0-vm-lead", 1);
  s = rw_util_get_rift_var_name("12", "tg", "vm1", rvn, 1024);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);
  EXPECT_STREQ (rvn, "var/rift/0-vm-lead");

  unsetenv("RIFT_VAR_ROOT");

  s = rw_util_get_rift_var_name(nullptr, nullptr, nullptr, rvn, 1024);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);

  std::string str = get_defualt_rvn();
  EXPECT_STREQ (rvn, str.c_str());

  s = rw_util_get_rift_var_name("12", "ab$&c", "vm@#123", rvn, 1024);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);

  EXPECT_STREQ (rvn, "vm..123");

  setenv("RIFT_VAR_VNF", "nat", 1);
  setenv("RIFT_VAR_VM", "RW_VM_MASTER-9", 1);

  s = rw_util_get_rift_var_name(nullptr, nullptr, nullptr, rvn, 1024);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);

  EXPECT_STREQ (rvn, "RW_VM_MASTER-9");

  unsetenv("RIFT_VAR_VNF");
  unsetenv("RIFT_VAR_VM");
}

TEST(RiftVarRoot, GetRiftVarRoot)
{
  char rvr[512] = {};

  rw_status_t s = rw_util_get_rift_var_root(nullptr, nullptr, nullptr, rvr, 512);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);

  std::string rvn = get_defualt_rvn();

  const char* rift_install = getenv("RIFT_INSTALL");
  ASSERT_TRUE (rift_install);

  std::string rvrs = std::string(rift_install) + "/var/rift/" + rvn;
  EXPECT_STREQ (rvr, rvrs.c_str());


  s = rw_util_get_rift_var_root("110", "tg", "vm3", rvr, 512);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);

  rvrs = std::string(rift_install) + "/var/rift/vm3";
  EXPECT_STREQ (rvr, rvrs.c_str());
}

TEST(RiftVarRoot, CreateRiftVarRoot)
{
  char rvr[512] = {};

  rw_status_t s = rw_util_get_rift_var_root(nullptr, nullptr, nullptr, rvr, 512);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);

  s = rw_util_create_rift_var_root(rvr);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);

  fs::path prvr(rvr);
  EXPECT_TRUE (fs::exists(prvr));
  EXPECT_TRUE (fs::is_directory(prvr));
}
