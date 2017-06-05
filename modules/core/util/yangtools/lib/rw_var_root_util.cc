/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Creation Date: 6/6/16
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#include <rwlib.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rw_var_root.h"

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

namespace fs = boost::filesystem;

const char* RW_VAR_NAME_DEFAULT_VNF = "rift";
const char* RW_BLD_RIFT_INSTALL = "/usr/rift/build/ub16_debug/install/usr/rift";
const char* RW_USR_RIFT_INSTALL = "/usr/rift";
const char* RW_VAR_RIFT = "/var/rift";
const int RW_UTIL_MAX_HOSTNAME_SZ = 64;

static inline
std::string rw_util_mangle_path_name(const char* name)
{
  std::string mn(name);
  const char* good = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_";

  for (unsigned i = 0; i < mn.length(); ++i) {
    if (!strchr(good, mn[i])) {
      mn[i] = '.';
    }
  }

  return mn;
}

rw_status_t rw_util_get_rift_var_name(const char* uid,
                                      const char* vnf,
                                      const char* vm,
                                      char* buf_out,
                                      int buf_len)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  const char* rift_var_name = nullptr;
  std::string rvn;

  do {

    /* Check if the evn variable is already set. */
    rift_var_name = getenv("RIFT_VAR_NAME");
    if (rift_var_name) {
      break;
    }

    /* Get from RIFT_VAR_ROOT if set */
    const char* rift_var_root = getenv("RIFT_VAR_ROOT");
    if (rift_var_root) {
      fs::path rvr(rift_var_root);
      std::vector<std::string> path_elems;

      for (auto& part: rvr) {
        path_elems.push_back(part.string());
      }

      if (path_elems.size() <= 3) {
        status = RW_STATUS_FAILURE;
        break;
      }

      for (unsigned i = path_elems.size()-3; 
                    i < path_elems.size(); ++i)  {
        rvn += path_elems[i];
        if (i != path_elems.size()-1) {
          rvn += "/";
        }
      }

      rift_var_name = rvn.c_str();
      break;
    }

    /* RIFT_VAR_NAME is the 2 directory path frgment: RIFT_VAR_VNF/RIFT_VAR_VM */
    char hostname[RW_UTIL_MAX_HOSTNAME_SZ] = {};

    if (!vm) {
      if (!(vm = getenv("RIFT_VAR_VM"))) {

        if (gethostname(hostname, RW_UTIL_MAX_HOSTNAME_SZ - 2) == -1) {
          status = RW_STATUS_FAILURE;
          break;
        }
        char *hn_short = strchr(hostname, '.');
        if (hn_short) {
          *hn_short = 0;
        }
        vm = hostname;
      }
    }

    rvn += rw_util_mangle_path_name(vm);
    rift_var_name = rvn.c_str();

  } while(0);

  if (rift_var_name) {
    int ret_len = snprintf (buf_out, buf_len, "%s", rift_var_name);
    if (ret_len < 0 || ret_len >= buf_len) {
      status = RW_STATUS_FAILURE;
    }
  }

  return status;
}

rw_status_t rw_util_get_rift_var_root(const char* uid,
                                      const char* vnf,
                                      const char* vm,
                                      char *buf_out,
                                      int buf_len)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  const char* rift_var_root = nullptr;
  std::string rvr;

  do {

    /* Get from RIFT_VAR_ROOT, if already set */
    rift_var_root = getenv("RIFT_VAR_ROOT");
    if (rift_var_root) {
      break;
    }

    char rift_var_name[1024] = {};
    status = rw_util_get_rift_var_name(uid, vnf, vm, rift_var_name, 1024);
    if (status != RW_STATUS_SUCCESS) {
      break;
    }

    /* RIFT_INSTALL/var/rift/RIFT_VAR_NAME, if RIFT_INSTALL is set.*/
    const char* rift_install = getenv("RIFT_INSTALL");
    if (rift_install) {
      if (!strcmp(rift_install, RW_USR_RIFT_INSTALL) ||
          !strcmp(rift_install, RW_BLD_RIFT_INSTALL) ||
          !strcmp(rift_install, "/")) {
        rvr = std::string(rift_install) + std::string(RW_VAR_RIFT);
      } else {
        rvr = std::string(rift_install) + std::string(RW_VAR_RIFT) + std::string("/") + rift_var_name;
      }
      rift_var_root = rvr.c_str();
      break;
    }

    rvr = std::string(RW_VAR_RIFT);
    rift_var_root = rvr.c_str();

  } while(0);

  if (rift_var_root) {
    int ret_len = snprintf (buf_out, buf_len, "%s", rift_var_root);
    if (ret_len < 0 || ret_len >= buf_len) {
      status = RW_STATUS_FAILURE;
    }
  }

  return status;
}

rw_status_t rw_util_create_rift_var_root(const char* rift_var_root)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  fs::path rvr(rift_var_root);

  if (fs::exists(rvr) && fs::is_directory(rvr)) {
    return status;
  }

  try {
    fs::create_directories(rvr);
  } catch (const fs::filesystem_error& e) {
    return RW_STATUS_FAILURE;
  }

  if (fs::is_directory(rvr)) {
    //fs::permissions(rvr, fs::all_all); ATTN:- Fixme
    return status;
  }

  return RW_STATUS_FAILURE;
}

rw_status_t rw_util_remove_rift_var_root(const char* rift_var_root)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  fs::path rvr(rift_var_root);

  if (!fs::exists(rvr)) {
    return status;
  }

  try {
    fs::remove_all(rvr); // ATTN:- Need to remove till RVN?
  } catch (const fs::filesystem_error& e) { 
    // Log error??
  }

  if (!fs::exists(rvr)) {
    return status;
  }

  return RW_STATUS_FAILURE;
}
