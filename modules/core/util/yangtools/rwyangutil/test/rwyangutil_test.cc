
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwyangutil_test.cc
 * @author Arun Muralidharan
 * @date 01/10/2015
 * @brief Google test cases for testing file protocol operations
 * in dynamic schema app library
 *
 */

#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include "gmock/gmock.h"
#include "gtest/rw_gtest.h"
#include "rwut.h"
#include "rwyangutil.h"

//ATTN: Boost bug 10038
// https://svn.boost.org/trac/boost/ticket/10038
// Fixed in 1.57
// TODO: Remove the definition once upgraded to >= 1.57
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

namespace fs = boost::filesystem;

class FileProOpsTestsFixture : public ::testing::Test
{
 public:
  FileProOpsTestsFixture()
  {
    rift_install_ = getenv("RIFT_INSTALL");
    if (!rift_install_.length()) {
      rift_install_ = "/";
    }

    schema_path_ = rift_install_ + "/" + RW_SCHEMA_ROOT_PATH;
    image_spath_ = rift_install_ + "/" + RW_INSTALL_YANG_PATH;
    test_listing_path_ = rift_install_
                         + "/"
                         + RW_INSTALL_MANIFEST_PATH
                         + "/"
                         + "test_northbound_listing.txt";
    test_wide_listing_path_ = rift_install_
                         + "/"
                         + RW_INSTALL_MANIFEST_PATH
                         + "/"
                         + "test_northbound_widening_listing.txt";
  }

  void SetUp()
  {
    if (fs::exists(schema_path_)) {
      TearDown(); // Just for first test case.
    }

    std::string cmd = "rwyangutil --remove-schema-dir --create-schema-dir test_northbound_listing.txt";
#ifdef CONFD_ENABLED
    cmd += " confd_nb_schema_list.txt";
#endif

    // Setup the schema directory.
    auto ret = std::system(cmd.c_str());
    ASSERT_EQ (ret, 0);

    lock_file_ = std::string(rift_install_) + "/" + RW_SCHEMA_LOCK_PATH + "/" + RW_SCHEMA_LOCK_FILENAME;
    if (fs::exists(lock_file_)) {
      std::system("rwyangutil --lock-file-delete");
    }

    schema_all_tmp_loc_ = std::string(rift_install_) + "/" + RW_SCHEMA_TMP_ALL_PATH;
    schema_northbound_tmp_loc_ = std::string(rift_install_) + "/" + RW_SCHEMA_TMP_NB_PATH;
    schema_ver_dir_ = std::string(rift_install_) + "/" + RW_SCHEMA_VER_PATH;
  }

  void TearDown()
  {
    auto ret = std::system("rwyangutil --remove-schema-dir");
    ASSERT_EQ (ret, 0);
  }

  std::string rift_install_;
  std::string schema_path_;
  std::string lock_file_;
  std::string schema_all_tmp_loc_;
  std::string schema_northbound_tmp_loc_;
  std::string schema_ver_dir_;
  std::string image_spath_;
  std::string test_listing_path_;
  std::string test_wide_listing_path_;
};

unsigned get_file_type_count(const std::string& path, const std::string& fext)
{
  unsigned count = 0;

  std::for_each(fs::directory_iterator(path),
                fs::directory_iterator(), [&count, &fext](const fs::directory_entry& et)
                { if (et.path().extension().string() == fext) { count++; } });

  return count;
}

static unsigned get_nb_schema_count(std::vector<std::string> const& nb_list)
{
  unsigned count = 0;

  for (unsigned i = 0; i < nb_list.size(); ++i) {
    {
      std::ifstream nb_listing_file(nb_list[i]);
      std::string line;
      while (std::getline(nb_listing_file, line)) {
        count++;
      }
    }
  }

  return count;
}

TEST_F(FileProOpsTestsFixture, SchemaDirCreationTest)
{
  // Check whether the schema directory is created properly
  ASSERT_TRUE ( fs::exists(schema_path_) ) ;
  ASSERT_TRUE ( fs::exists(schema_path_ + "/yang") );
#ifdef CONFD_ENABLED
  ASSERT_TRUE ( fs::exists(schema_path_ + "/fxs") );
#endif
  ASSERT_TRUE ( fs::exists(schema_path_ + "/xml") );
  ASSERT_TRUE ( fs::exists(schema_path_ + "/lib") );
  ASSERT_TRUE ( fs::exists(schema_path_ + "/lock") );
  ASSERT_TRUE ( fs::exists(schema_path_ + "/tmp") );
  ASSERT_TRUE ( fs::exists(schema_path_ + "/version") );

  // Check whether all the symbolic links are created properly.
  unsigned ycount = 0;
  fs::path ypath(schema_path_ + "/yang");
  for (fs::directory_iterator it(ypath);
       it != fs::directory_iterator();
       ++it) {

    ASSERT_TRUE( fs::is_symlink(it->path()) );
    EXPECT_STREQ ( it->path().extension().string().c_str(), ".yang");

    auto opath = fs::read_symlink( it->path() );
    EXPECT_FALSE (opath.empty());
    EXPECT_TRUE (fs::exists(opath));
    EXPECT_TRUE (fs::is_regular_file(opath));
    ycount++;
  }

  auto yc = get_file_type_count(image_spath_, std::string(".yang"));
  EXPECT_EQ (yc, ycount);
  std::cout << "Total installed yang files " << yc << std::endl;

  unsigned dcount = 0;
  fs::path xpath(schema_path_ + "/xml");
  for (fs::directory_iterator it(xpath);
       it != fs::directory_iterator();
       ++it) {

    ASSERT_TRUE( fs::is_symlink(it->path()) );
    EXPECT_STREQ ( it->path().extension().string().c_str(), ".dsdl");

    auto opath = fs::read_symlink( it->path() );
    EXPECT_FALSE (opath.empty());
    EXPECT_TRUE (fs::exists(opath));
    EXPECT_TRUE (fs::is_regular_file(opath));
    dcount++;
  }

  auto dc = get_file_type_count(image_spath_, std::string(".dsdl"));
  EXPECT_EQ (dc, dcount);
  std::cout << "Total installed dsdl files " << dc << std::endl;

#ifdef CONFD_ENABLED
  unsigned fcount = 0;
  fs::path fpath(schema_path_ + "/fxs");
  for (fs::directory_iterator it(fpath);
       it != fs::directory_iterator();
       ++it) {

    ASSERT_TRUE( fs::is_symlink(it->path()) );
    EXPECT_STREQ ( it->path().extension().string().c_str(), ".fxs");

    auto opath = fs::read_symlink( it->path() );
    EXPECT_FALSE (opath.empty());
    EXPECT_TRUE (fs::exists(opath));
    EXPECT_TRUE (fs::is_regular_file(opath));
    fcount++;
  }

  auto fc = get_file_type_count(image_spath_, std::string(".fxs"));
  EXPECT_EQ (fc, fcount);
  std::cout << "Total installed fxs files " << fc << std::endl;
#endif

  unsigned cmcount = 0, cssicount = 0;
  fs::path cpath(schema_path_ + "/cli");
  for (fs::directory_iterator it(cpath);
       it != fs::directory_iterator();
       ++it) {

    ASSERT_TRUE( fs::is_symlink(it->path()) );
    std::string ext;
    fs::path fn = it->path().filename();

    for (; !fn.extension().empty(); fn = fn.stem()) {
      ext = fn.extension().string() + ext;
    }
    EXPECT_TRUE ( ext == ".cli.xml" || ext == ".cli.ssi.sh" );

    auto opath = fs::read_symlink( it->path() );
    EXPECT_FALSE (opath.empty());
    EXPECT_TRUE (fs::exists(opath));
    EXPECT_TRUE (fs::is_regular_file(opath));

    if (ext == ".cli.xml") {
      cmcount++;
    } else {
      cssicount++;
    }
  }

  EXPECT_TRUE (cmcount);
  EXPECT_TRUE (cssicount);

  auto cm = get_file_type_count(image_spath_, std::string(".xml"));
  EXPECT_EQ (cm, cmcount);
  std::cout << "Total installed cli manifest files " << cm << std::endl;

  auto cs = get_file_type_count(image_spath_, std::string(".sh"));
  EXPECT_EQ (cs, cssicount);
  std::cout << "Total installed cli ssi script files " << cs << std::endl;

  fs::path lpath(schema_path_ + "/lib");
  EXPECT_TRUE ( fs::is_empty(lpath) );

  fs::path lopath(schema_path_ + "/lock");
  EXPECT_TRUE ( fs::is_empty(lopath) );

  unsigned tcount = 0;
  fs::path tpath(schema_path_ + "/tmp");
  for (fs::directory_iterator it(tpath);
       it != fs::directory_iterator();
       ++it) {
    tcount++;
  }
  ASSERT_EQ(tcount, 2);

  fs::path vpath(schema_path_ + "/version");
  int cnt = 0;
  std::for_each(fs::directory_iterator(vpath),
                fs::directory_iterator(), [&cnt](const fs::directory_entry& _){ cnt++; });
  ASSERT_EQ(cnt, 2);

  std::vector<std::string> nb_list;
  nb_list.push_back(test_listing_path_);

#ifdef CONFD_ENABLED
  std::string confd_nb_file = rift_install_ + "/usr/data/manifest/confd_nb_schema_list.txt";
  nb_list.push_back(confd_nb_file);
#endif

  unsigned nb_schema_count = get_nb_schema_count(nb_list);
  fs::path nb_ypath = schema_ver_dir_ + "/latest/northbound/yang";

  cnt = 0;
  std::for_each(fs::directory_iterator(nb_ypath),
                fs::directory_iterator(), [&cnt](const fs::directory_entry& _){ cnt++; });

  EXPECT_EQ(cnt, nb_schema_count);
}

TEST_F(FileProOpsTestsFixture, CreateAndDeleteLockFileTest)
{
  auto ret = std::system("rwyangutil --lock-file-create");
  ASSERT_EQ(ret, 0);
  ASSERT_TRUE(fs::exists(lock_file_));

  ret = std::system("rwyangutil --lock-file-delete");
  ASSERT_EQ(ret, 0);

  ASSERT_FALSE(fs::exists(lock_file_));
}

TEST_F(FileProOpsTestsFixture, CreateNewVersionDir)
{
  int cnt = 0;
  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [&cnt](const fs::directory_entry& _){ cnt++; });

  auto ret = std::system("rwyangutil --version-dir-create");
  ASSERT_EQ(ret, 0);

  int cnt2 = 0;
  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [&cnt2](const fs::directory_entry& _){ cnt2++; });

  ASSERT_EQ((cnt2 - cnt), 0);

  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [](const fs::directory_entry& e){ fs::remove_all(e); });

}

TEST_F(FileProOpsTestsFixture, CascadeTest1)
{
  auto ret = std::system("rwyangutil --lock-file-create --version-dir-create");
  ASSERT_EQ(ret, 0);
  ASSERT_TRUE(fs::exists(lock_file_));

  int cnt = 0;
  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [&cnt](const fs::directory_entry& _){ cnt++; });
  ASSERT_TRUE(cnt > 0);

  ret = std::system("rwyangutil --lock-file-delete");
  ASSERT_EQ(ret, 0);
  ASSERT_FALSE(fs::exists(lock_file_));

  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [](const fs::directory_entry& e){ fs::remove_all(e); });
}

TEST_F(FileProOpsTestsFixture, PruneDirTest)
{
  for (unsigned i = 0; i < 20; ++i) {
    auto ret = std::system("rwyangutil --version-dir-create");
    ASSERT_EQ(ret, 0);
  }

  auto ret = std::system("rwyangutil --prune-schema-dir");
  ASSERT_EQ(ret, 0);

  unsigned cnt = 0;
  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [&cnt](const fs::directory_entry& _){ cnt++; });

  ASSERT_EQ(cnt, 2);

  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [](const fs::directory_entry& et) { fs::remove(et.path().string()+"/stamp");});

  ret = std::system("rwyangutil --prune-schema-dir");
  ASSERT_EQ(ret, 0);

  cnt = 0;
  std::for_each(fs::directory_iterator(schema_ver_dir_),
                fs::directory_iterator(), [&cnt](const fs::directory_entry& _){ cnt++; });
  ASSERT_EQ(cnt, 2);
}

#ifdef CONFD_ENABLED

TEST_F(FileProOpsTestsFixture, RemovePersistConfdWSTest)
{
  bool present = false;
  for (fs::directory_iterator entry(rift_install_);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;
    auto dir_name = entry->path().filename().string();
    auto pos = dir_name.find(RW_SCHEMA_MGMT_PERSIST_PREFIX);

    if (pos != std::string::npos) {
      present = true;
      break;
    }
  }

  if (!present) {
    auto dir = rift_install_ + "/" + std::string(RW_SCHEMA_MGMT_PERSIST_PREFIX) + "tmp1";
    ASSERT_TRUE(fs::create_directory(dir));
  }
  auto ret = std::system("rwyangutil --rm-persist-mgmt-ws");
  EXPECT_EQ (ret, 0);

  std::for_each(fs::directory_iterator(rift_install_),
                fs::directory_iterator(), [](const fs::directory_entry& et) {
                  auto name = et.path().filename().string();
                  EXPECT_EQ (name.find(RW_SCHEMA_MGMT_PERSIST_PREFIX), std::string::npos);
                });
}

TEST_F(FileProOpsTestsFixture, RemoveUniqueConfdWSTest)
{
  bool present = false;
  for (fs::directory_iterator entry(rift_install_);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;
    auto dir_name = entry->path().filename().string();
    auto pos = dir_name.find(RW_SCHEMA_MGMT_TEST_PREFIX);

    if (pos != std::string::npos) {
      present = true;
      break;
    }
  }

  if (!present) {
    auto dir = rift_install_ + "/" + std::string(RW_SCHEMA_MGMT_TEST_PREFIX) + "tmp2";
    ASSERT_TRUE(fs::create_directory(dir));
  }

  auto ret = std::system("rwyangutil --rm-unique-mgmt-ws");
  EXPECT_EQ (ret, 0);

  std::for_each(fs::directory_iterator(rift_install_),
                fs::directory_iterator(), [](const fs::directory_entry& et) {
                  auto name = et.path().filename().string();
                  EXPECT_EQ (name.find(RW_SCHEMA_MGMT_TEST_PREFIX), std::string::npos);
                });
}

TEST_F(FileProOpsTestsFixture, ArchiveConfdPersistWS)
{
  auto dir = rift_install_ + "/" + std::string(RW_SCHEMA_MGMT_PERSIST_PREFIX) + "tm35";
  ASSERT_TRUE(fs::create_directory(dir));

  auto ret = std::system("rwyangutil --archive-mgmt-persist-ws");
  ASSERT_EQ(ret, 0);

  bool found = false;

  for (fs::directory_iterator entry(rift_install_);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;

    auto dir_name = entry->path().filename().string();
    auto pos = dir_name.find(RW_SCHEMA_MGMT_ARCHIVE_PREFIX);

    if (pos != 0) continue;
    found = true;
    fs::remove(fs::path(rift_install_ + "/" + dir_name));
    break;
  }

  ASSERT_TRUE(found);
}
#endif

TEST_F(FileProOpsTestsFixture, OnBoardSchema)
{
  size_t ret = 0;
  // touch southbound files
  std::string sb_cmd = std::string("touch ") + schema_all_tmp_loc_;
  std::string sb_fxs_a = "/bin/true";
#ifdef CONFD_ENABLED
  sb_fxs_a = sb_cmd + "/fxs/a.fxs";
#endif
  auto sb_yang_a = sb_fxs_a + ";" + sb_cmd + "/yang/a.yang";
  auto sb_xml_a = sb_yang_a + ";" + sb_cmd + "/xml/a.dsdl";
  auto sb_so_a = sb_xml_a + ";" + sb_cmd + "/lib/a.so";

  std::string sb_fxs_b = "/bin/true";
#ifdef CONFD_ENABLED
  sb_fxs_b = sb_cmd + "/fxs/b.fxs";
#endif
  auto sb_yang_b = sb_fxs_b + ";" + sb_cmd + "/yang/b.yang";
  auto sb_xml_b = sb_yang_b + ";" + sb_cmd + "/xml/b.dsdl";
  auto sb_so_b = sb_xml_b + ";" + sb_cmd + "/lib/b.so";

  ret = std::system(sb_so_a.c_str());
  ASSERT_EQ(ret, 0);

  ret = std::system(sb_so_b.c_str());
  ASSERT_EQ(ret, 0);

  // touch northbound files
  std::string nb_cmd = std::string("touch ") + schema_northbound_tmp_loc_;
  std::string fxs = "/bin/true";
#ifdef CONFD_ENABLED
  fxs = nb_cmd + "/fxs/a.fxs";
#endif
  auto yang = fxs + ";" + nb_cmd + "/yang/a.yang";
  auto xml = yang + ";" + nb_cmd + "/xml/a.dsdl";
  auto so = xml + ";" + nb_cmd + "/lib/a.so";
  ret = std::system(so.c_str());
  ASSERT_EQ(ret, 0);

  std::string all_cmd = std::string("touch ") + schema_all_tmp_loc_;
  std::string all_fxs = "/bin/true";
#ifdef CONFD_ENABLED
  all_fxs = all_cmd + "/fxs/a.fxs";
#endif
  auto all_yang = all_fxs + ";" + all_cmd + "/yang/a.yang";
  auto all_xml = all_yang + ";" + all_cmd + "/xml/a.dsdl";
  auto all_so = all_xml + ";" + all_cmd + "/lib/a.so";
  ret = std::system(all_so.c_str());
  ASSERT_EQ(ret, 0);


  std::vector<std::string> const paths {
#ifdef CONFD_ENABLED
    "/fxs",
#endif
    "/xml",
    "/lib",
    "/yang"
  };

  // get the count first
  std::map<std::string, size_t> first_all_count;
  std::map<std::string, size_t> first_northbound_count;

  for (std::string const & subpath : paths) {
    std::string const all_subpath = schema_ver_dir_ + "/latest/all/" + subpath;
    std::string const northbound_subpath = schema_ver_dir_ + "/latest/northbound/" + subpath;

    first_all_count[subpath] = 0;
    first_northbound_count[subpath] = 0;

    for (fs::directory_iterator it(all_subpath);
         it != fs::directory_iterator();
         ++it) {
      first_all_count[subpath]++;
    }

    for (fs::directory_iterator it(northbound_subpath);
         it != fs::directory_iterator();
         ++it) {
      first_northbound_count[subpath]++;
    }

  }

  // do update
  ret = std::system("rwyangutil --version-dir-create");
  ASSERT_EQ(ret, 0);

  // get the count after the update
  std::map<std::string, size_t> second_all_count;
  std::map<std::string, size_t> second_northbound_count;

  for (std::string const & subpath : paths) {
    std::string const all_subpath = schema_ver_dir_ + "/latest/all/" + subpath;
    std::string const northbound_subpath = schema_ver_dir_ + "/latest/northbound/" + subpath;

    second_all_count[subpath] = 0;
    second_northbound_count[subpath] = 0;

    for (fs::directory_iterator it(all_subpath);
         it != fs::directory_iterator();
         ++it) {
      second_all_count[subpath]++;
    }

    for (fs::directory_iterator it(northbound_subpath);
         it != fs::directory_iterator();
         ++it) {
      second_northbound_count[subpath]++;
    }

  }

  // do update
  ret = std::system("rwyangutil --version-dir-create");
  ASSERT_EQ(ret, 0);

  // get the count after the third update
  std::map<std::string, size_t> third_all_count;
  std::map<std::string, size_t> third_northbound_count;

  for (std::string const & subpath : paths) {
    std::string const all_subpath = schema_ver_dir_ + "/latest/all/" + subpath;
    std::string const northbound_subpath = schema_ver_dir_ + "/latest/northbound/" + subpath;

    third_all_count[subpath] = 0;
    third_northbound_count[subpath] = 0;

    for (fs::directory_iterator it(all_subpath);
         it != fs::directory_iterator();
         ++it) {
      third_all_count[subpath]++;
    }

    for (fs::directory_iterator it(northbound_subpath);
         it != fs::directory_iterator();
         ++it) {
      third_northbound_count[subpath]++;
    }

  }

  // check the right number of files are found
  ASSERT_EQ(first_all_count.size(), second_all_count.size());
  ASSERT_EQ(first_northbound_count.size(), second_northbound_count.size());
  ASSERT_EQ(first_all_count.size(), third_all_count.size());
  ASSERT_EQ(first_northbound_count.size(), third_northbound_count.size());

  for (std::string const & subpath : paths) {

    // the a and b schema files are added to all
    ASSERT_EQ(first_all_count.at(subpath) + 2, second_all_count.at(subpath));

    // only the a schema file is added to northbound
    ASSERT_EQ(first_northbound_count.at(subpath) + 1, second_northbound_count.at(subpath));

    // second and third should be the same
    ASSERT_EQ(third_all_count.at(subpath), second_all_count.at(subpath));
    ASSERT_EQ(third_northbound_count.at(subpath), second_northbound_count.at(subpath));
  }
}

TEST_F(FileProOpsTestsFixture, SchemaOnboardReload)
{
  size_t ret = 0;
  // touch southbound files
  std::string sb_cmd = std::string("touch ") + schema_all_tmp_loc_;
  std::string sb_fxs_a = "/bin/true";
#ifdef CONFD_ENABLED
  sb_fxs_a = sb_cmd + "/fxs/a.fxs";
#endif
  auto sb_yang_a = sb_fxs_a + ";" + sb_cmd + "/yang/a.yang";
  auto sb_xml_a = sb_yang_a + ";" + sb_cmd + "/xml/a.dsdl";
  auto sb_so_a = sb_xml_a + ";" + sb_cmd + "/lib/a.so";

  std::string sb_fxs_b = "/bin/true";
#ifdef CONFD_ENABLED
  sb_fxs_b = sb_cmd + "/fxs/b.fxs";
#endif
  auto sb_yang_b = sb_fxs_b + ";" + sb_cmd + "/yang/b.yang";
  auto sb_xml_b = sb_yang_b + ";" + sb_cmd + "/xml/b.dsdl";
  auto sb_so_b = sb_xml_b + ";" + sb_cmd + "/lib/b.so";

  ret = std::system(sb_so_a.c_str());
  ASSERT_EQ(ret, 0);

  ret = std::system(sb_so_b.c_str());
  ASSERT_EQ(ret, 0);

  // touch northbound files
  std::string nb_cmd = std::string("touch ") + schema_northbound_tmp_loc_;
  std::string fxs = "/bin/true";
#ifdef CONFD_ENABLED
  fxs = nb_cmd + "/fxs/a.fxs";
#endif
  auto yang = fxs + ";" + nb_cmd + "/yang/a.yang";
  auto xml = yang + ";" + nb_cmd + "/xml/a.dsdl";
  auto nb_so = xml + ";" + nb_cmd + "/lib/a.so";
  ret = std::system(nb_so.c_str());
  ASSERT_EQ(ret, 0);

  // do the update
  ret = std::system("rwyangutil --version-dir-create");
  ASSERT_EQ(ret, 0);

  // touch the files again
  ret = std::system(sb_so_a.c_str());
  ASSERT_EQ(ret, 0);
  ret = std::system(sb_so_b.c_str());
  ASSERT_EQ(ret, 0);
  ret = std::system(nb_so.c_str());
  ASSERT_EQ(ret, 0) << "executing " << nb_so << " failed" << std::endl;

  // do the update again
  ret = std::system("rwyangutil --version-dir-create");
  ASSERT_EQ(ret, 0);
}

TEST(YangUtils, LoadSchemaValidation)
{
  std::string module_name("rw-pbc-stats");
  std::string mangled_name("RwPbcStats");
  std::string upper_to_lower("rw_pbc_stats");
  std::string err_str;

  auto rift_install = rwyangutil::get_rift_install();
  auto ret = std::system("rwyangutil --create-schema-dir test_northbound_listing.txt");
  ASSERT_EQ (ret, 0);

  auto res = rwyangutil::validate_module_consistency(
      module_name,
      mangled_name,
      upper_to_lower,
      err_str);

  EXPECT_TRUE (res);
  if (err_str.length()) {
    std::cerr << err_str << std::endl;
  }

  std::system("rwyangutil --remove-schema-dir");

}

TEST_F(FileProOpsTestsFixture, RemovePersistXMLWSTest)
{
  bool present = false;
  for (fs::directory_iterator entry(rift_install_);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;
    auto dir_name = entry->path().filename().string();
    auto pos = dir_name.find(RW_SCHEMA_MGMT_PERSIST_PREFIX);

    if (pos != std::string::npos) {
      present = true;
      break;
    }
  }

  if (!present) {
    auto dir = rift_install_ + "/" + std::string(RW_SCHEMA_MGMT_PERSIST_PREFIX) + "tmp4";
    ASSERT_TRUE(fs::create_directory(dir));
  }
  auto ret = std::system("rwyangutil --rm-persist-mgmt-ws");
  EXPECT_EQ (ret, 0);

  std::for_each(fs::directory_iterator(rift_install_),
                fs::directory_iterator(), [](const fs::directory_entry& et) {
                  auto name = et.path().filename().string();
                  EXPECT_EQ (name.find(RW_SCHEMA_MGMT_PERSIST_PREFIX), std::string::npos);
                });
}

TEST_F(FileProOpsTestsFixture, RemoveUniqueXMLWSTest)
{
  bool present = false;
  for (fs::directory_iterator entry(rift_install_);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;
    auto dir_name = entry->path().filename().string();
    auto pos = dir_name.find(RW_SCHEMA_MGMT_TEST_PREFIX);

    if (pos != std::string::npos) {
      present = true;
      break;
    }
  }

  if (!present) {
    auto dir = rift_install_ + "/" + std::string(RW_SCHEMA_MGMT_TEST_PREFIX) + "tmp5";
    ASSERT_TRUE(fs::create_directory(dir));
  }

  auto ret = std::system("rwyangutil --rm-unique-mgmt-ws");
  EXPECT_EQ (ret, 0);

  std::for_each(fs::directory_iterator(rift_install_),
                fs::directory_iterator(), [](const fs::directory_entry& et) {
                  auto name = et.path().filename().string();
                  EXPECT_EQ (name.find(RW_SCHEMA_MGMT_TEST_PREFIX), std::string::npos);
                });
}

TEST_F(FileProOpsTestsFixture, ArchiveXMLPersistWS)
{
  auto dir = rift_install_ + "/" + std::string(RW_SCHEMA_MGMT_PERSIST_PREFIX) + "tmp6";
  ASSERT_TRUE(fs::create_directory(dir));

  auto ret = std::system("rwyangutil --archive-mgmt-persist-ws");
  ASSERT_EQ(ret, 0);

  bool found = false;

  for (fs::directory_iterator entry(rift_install_);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;

    auto dir_name = entry->path().filename().string();
    auto pos = dir_name.find(RW_SCHEMA_MGMT_ARCHIVE_PREFIX);

    if (pos != 0) continue;
    found = true;
    fs::remove(fs::path(rift_install_ + "/" + dir_name));
    break;
  }

  ASSERT_TRUE(found);
}

TEST_F(FileProOpsTestsFixture, WidenNorthboundSchema)
{
  // setup initial schema directory
  std::string const cmd = "rwyangutil --remove-schema-dir --create-schema-dir test_northbound_listing.txt";

  auto ret = std::system(cmd.c_str());
  ASSERT_EQ (ret, 0);

  std::vector<std::string> nb_list = {test_listing_path_};

  size_t const nb_schema_count = get_nb_schema_count(nb_list);
  fs::path nb_ypath = schema_ver_dir_ + "/latest/northbound/yang";

  size_t initial_northbound_count = 0;
  std::for_each(fs::directory_iterator(nb_ypath),
                fs::directory_iterator(), // default constructor is end()
                [&initial_northbound_count](const fs::directory_entry& /*unused*/)
                {
                  initial_northbound_count++;
                });

  EXPECT_EQ(initial_northbound_count, nb_schema_count);
  
  // widen the northbound schema
  std::string const wide_cmd = "rwyangutil --create-schema-dir test_northbound_widening_listing.txt";

  auto wide_ret = std::system(wide_cmd.c_str());
  ASSERT_EQ (wide_ret, 0);

  std::vector<std::string> wide_nb_list = {test_wide_listing_path_};

  size_t const wide_nb_schema_count = get_nb_schema_count(wide_nb_list);
  EXPECT_GT(wide_nb_schema_count, 0);

  fs::path wide_nb_ypath = schema_ver_dir_ + "/latest/northbound/yang";

  size_t wide_northbound_count = 0;
  std::for_each(fs::directory_iterator(nb_ypath),
                fs::directory_iterator(), // default constructor is end()
                [&wide_northbound_count](const fs::directory_entry& /*unused*/)
                {
                  wide_northbound_count++;
                });

  EXPECT_EQ(wide_northbound_count, nb_schema_count + wide_nb_schema_count);

  // cleanup schema directory
  std::string const clean_cmd = "rwyangutil --remove-schema-dir --create-schema-dir test_northbound_listing.txt";
  auto clean_ret = std::system(clean_cmd.c_str());
  ASSERT_EQ (clean_ret, 0);

}
