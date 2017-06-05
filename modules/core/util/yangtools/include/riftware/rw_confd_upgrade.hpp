/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file   rw_confd_upgrade.hpp
 * @author Arun Muralidharan
 * @date   21/08/2015
 * @brief  RW Confd in-service upgrade manager
*/

#ifndef RW_CONFD_UPGRADE_HPP_
#define RW_CONFD_UPGRADE_HPP_

//ATTN: Boost bug 10038
// https://svn.boost.org/trac/boost/ticket/10038
// Fixed in 1.57
// TODO: Remove the definition once upgraded to >= 1.57
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <string>
#include <unistd.h>

#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

typedef enum {
  RW_CONFD_LOG_LEVEL_DEBUG = 31022015,
  RW_CONFD_LOG_LEVEL_INFO,
  RW_CONFD_LOG_LEVEL_ERROR
} rw_confd_log_level_e;

typedef void (*rw_confd_upgrade_log_cb)(void *user_data, rw_confd_log_level_e level, const char *fn, const char *log_msg);

#define RW_CONFD_MGR_LOG(confd_mgr, level, fn, log_msg) \
  if (confd_mgr->log_cb_) {                          \
    (confd_mgr->log_cb_)((confd_mgr->log_cb_user_data_), level, fn, log_msg); \
  }

#define RW_CONFD_MGR_LOG_STRING(confd_mgr, level, function, log_str) \
{                                                      \
  RW_CONFD_MGR_LOG(confd_mgr, level, function, log_str); \
}

#define RW_CONFD_MGR_DEBUG(confd_mgr, log_msg)                                \
  RW_CONFD_MGR_LOG(confd_mgr, RW_CONFD_LOG_LEVEL_DEBUG, __FUNCTION__, log_msg.str().c_str())

#define RW_CONFD_MGR_DEBUG_STRING(confd_mgr, log_msg)                                \
  RW_CONFD_MGR_LOG_STRING(confd_mgr, RW_CONFD_LOG_LEVEL_DEBUG, __FUNCTION__, log_msg)

#define RW_CONFD_MGR_INFO(confd_mgr, log_msg)                                 \
  RW_CONFD_MGR_LOG(confd_mgr, RW_CONFD_LOG_LEVEL_INFO, __FUNCTION__, log_msg.str().c_str())

#define RW_CONFD_MGR_INFO_STRING(confd_mgr, log_msg)                                 \
  RW_CONFD_MGR_LOG_STRING(confd_mgr, RW_CONFD_LOG_LEVEL_INFO, __FUNCTION__, log_msg)

#define RW_CONFD_MGR_ERROR(confd_mgr, log_msg)                                \
  RW_CONFD_MGR_LOG(confd_mgr, RW_CONFD_LOG_LEVEL_ERROR, __FUNCTION__, log_msg.str().c_str())

#define RW_CONFD_MGR_ERROR_STRING(confd_mgr, log_msg)                                \
  RW_CONFD_MGR_LOG_STRING(confd_mgr, RW_CONFD_LOG_LEVEL_ERROR, __FUNCTION__, log_msg)

namespace rw_yang {

  /**
   * FSHelper is a thin wrapper around few of the boost::filesystem 
   * API's. The API's wrapped by this class are the once which are most
   * used in rift ware. 
   * This class also provides API's which gives a composite behavior of
   * two or more API's.
   * NOTE: All member functions should be implemented as static.
   */
  class FSHelper {
  public:
    FSHelper() {}
    FSHelper(const FSHelper&) = delete;
    void operator =(const FSHelper&) = delete;

  public:
    static bool 
    create_directory(const std::string& dir_name);

    static bool
    create_directories(const std::string& path);

    static bool 
    rename_directory(const std::string& orig_name, 
                     const std::string& new_name);
    static bool 
    remove_directory(const std::string& dir_name);

    static bool 
    create_sym_link(const std::string& sym_link_name,
                    const std::string& dir_name);

    static bool 
    remove_sym_link(const std::string& sym_link_name);

    static bool 
    recreate_sym_link(const std::string& sym_link_name,
                      const std::string& dir_name);

    static std::string 
    get_sym_link_name(const std::string& sym_link);

    static bool
    create_hardlinks(const std::string& source_dir,
                     const std::string& target_dir);

    static bool
    create_softlinks(const std::string& source_dir,
                     const std::string& target_dir);

    static bool
    compare_link_name_to_directory(const std::string& link_name,
                                   const std::string& dir_name);

    static bool
    copy_all_files(const std::string& source_dir,
                   const std::string& dest_dir);

    static bool
    remove_all_files(const std::string& dir);

  };


  /**
   * ConfdUpgradeMgr provides APIs required for performing
   * confd CDB upgrade.
   * UAgent creates an instance of this class whenever
   * it starts an upgrade transaction.
   */

  class ConfdUpgradeMgr {
  public:
    // Default constructor for accessing
    // basic file system information
    ConfdUpgradeMgr(): ConfdUpgradeMgr(0, nullptr, 0)
    {}

    ConfdUpgradeMgr(uint32_t version,
        const struct sockaddr *addr,
        size_t addr_size);

    ConfdUpgradeMgr(const ConfdUpgradeMgr&) = delete;
    void operator =(const ConfdUpgradeMgr&) = delete;

    ~ConfdUpgradeMgr();

  public:
    /// Starts the upgrade process
    bool start_upgrade();

    /// Commit the upgrade
    bool commit();

    void set_log_cb(rw_confd_upgrade_log_cb cb, void *user_data);

    /// Get the max number of the link farm which is being
    // used currently
    uint32_t get_max_version_linked();

    /// Get the max number of the version not considering
    // the link farm
    uint32_t get_max_version_unlinked();

  private:
    bool start_confd_upgrade();
    int  set_user_session();
    bool maapi_sock_connect();
    void send_progress_msg(std::ostringstream& msg);

  private:
    uint32_t version_ = 0;
    bool ready_for_commit_ = false;

    struct sockaddr *confd_addr_ = nullptr;
    struct sockaddr_in confd_addr_in_;
    size_t confd_addr_size_ = 0;

    int maapi_sock_ = -1;

    std::string usid_env_;
    std::string yang_root_;
    std::string yang_sl_;
    std::string rift_install_;
    std::string rift_var_root_;
    std::string ver_dir_;
    std::string latest_;

    rw_confd_upgrade_log_cb log_cb_ = nullptr;
    void *log_cb_user_data_ = nullptr;
  };

};


#endif
