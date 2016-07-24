
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_namespace.h
 * @date 2014/09/02
 * @brief RiftWare namespace management
 */

#ifndef RW_YANGTOOLS_NAMESPACE_H_
#define RW_YANGTOOLS_NAMESPACE_H_

#include <sys/cdefs.h>
#include "rwlib.h"

// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#if __cplusplus

#include <memory>
#include <map>
#include <vector>

#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

/**
 * @defgroup RwKeyspec RiftWare virtual DOM schema path specifiers
 * @{
 */

__BEGIN_DECLS

typedef uint32_t rw_namespace_id_t;
#define RW_NAMESPACE_ID_NULL ((rw_namespace_id_t)0)

// ATTN: Document me
struct rw_namespace_s;
typedef struct rw_namespace_s rw_namespace_t;

// ATTN: Document me
struct rw_namespace_manager_s;
typedef struct rw_namespace_manager_s rw_namespace_manager_t;


__END_DECLS

#if __cplusplus

namespace rw_yang {

/**
   Namespace information.
 */
class Namespace {
 public:
  typedef std::unique_ptr<Namespace> uptr_t;

  /// ATTN: Document me
  Namespace(
    const char* ns,
    const char* prefix,
    rw_namespace_id_t nsid)
  : ns_(ns),
    prefix_(prefix?prefix:""),
    module_(""),
    nsid_(nsid)
  {}

  /// ATTN: Document me
  rw_namespace_id_t get_nsid() const {
    return nsid_;
  }

  /// ATTN: Document me
  const char* get_ns() const {
    return ns_.c_str();
  }

  /// ATTN: Document me
  const char* get_prefix() const {
    return prefix_.c_str();
  }

  /// Get the module name. Used only incase of dynamic schemas
  const char* get_module() const {
    return module_.c_str();
  }
 
  /// Set the module name.
  void set_module(const std::string& mn) {
    module_ = mn;
  }

  // ATTN: Allow setting the prefix if not already set?

 private:

  std::string ns_;
  std::string prefix_;
  std::string module_;
  rw_namespace_id_t nsid_;
};


/**
   Singleton namespace manager.  This class is designed as a singleton
   because an entire RiftWare instance must share the same namespace
   numbering.  Therefore, even in collapsed mode, there is never a need
   for more than one of these objects.  However, the primary motivation
   is to not force the users to pass a namespace manager around in all
   the API calls (if this goal can be achieved without a singleton,
   then feel free to remove it).
 */
class NamespaceManager {
 public:

  static const rw_namespace_id_t NSID_RUNTIME_BASE = 1000;
  static const rw_namespace_id_t NSID_NULL = RW_NAMESPACE_ID_NULL;

  /// Runtime dynamic schema generation support
  static constexpr const char* dyn_schema_mn     = "rw-dynamic";
  static constexpr const char* dyn_schema_ns     = "http://riftio.com/ns/riftware-1.0/rw-dynamic";
  static constexpr const char* dyn_schema_prefix = "dyn";

  /// Get the global manager reference.
  static NamespaceManager& get_global() {
    // N.B. relies on c++11 having fixed static initialization across threads.
    static NamespaceManager mgr;
    return mgr;
  }

  /// Clear the runtime registered namespaces.  Please only use this in
  /// unit tests!
  void unittest_runtime_clear();

  /// Convert a namespace ID to a full namespace string
  const char* nsid_to_string(
    rw_namespace_id_t nsid);

  /// Convert a namespace ID to a namespace prefix string
  const char* nsid_to_prefix(
    rw_namespace_id_t nsid);

  /// Search for a namespace string and return the corresponding ID
  /// ATTN: This operation can be expensive
  rw_namespace_id_t string_to_nsid(
    const char* ns);

  /// Search for a namespace.  If found, return the ID.  If not found,
  /// register it and return the ID.
  rw_namespace_id_t find_or_register(
    const char* ns,
    const char* prefix);

  /// Runtime schema generation support. Get a new unique runtime namespace
  /// for dynamic schema.
  Namespace get_new_dynamic_schema_ns();

  /// Check whether the ns points to a dynamic schema module
  bool ns_is_dynamic(const char* ns);

  /// Get the hash of ns string as nsid
  rw_namespace_id_t get_ns_hash(
      const char* ns);

  /// Given the hash of ns, get the corresponding string.
  const char* nshash_to_string(
      rw_namespace_id_t nsid);

 private:
  NamespaceManager() {
    init();
  }

  void init();

  Namespace* get_nsobj_locked(
    rw_namespace_id_t nsid);

  void register_fixed(
    rw_namespace_id_t nsid,
    const char* ns);

  rw_namespace_id_t register_runtime_locked(
    const char* ns,
    const char* prefix);

 private:

  typedef Namespace::uptr_t ns_uptr_t;
  typedef std::vector<ns_uptr_t> id2ns_t;
  typedef std::map<std::string,rw_namespace_id_t> ns2id_t;
  typedef std::map<rw_namespace_id_t,std::string> hash2ns_t;

  ns2id_t ns2id_;
  ns2id_t ns2hash_; // ns string to ns-hash (as nsid)
  hash2ns_t hash2ns_; // ns hash to ns string

  id2ns_t id2ns_fixed_;
  id2ns_t id2ns_runtime_;

  rw_namespace_id_t current_nsid_ = NSID_NULL;

  /// Counter to keep track of the number of dynamic schemas.
  unsigned runtime_dyns_counter_ = 0;

  // ATTN: the runtime namespaces should use a separate range

  // ATTN: For interim library, should limit the NS lit size - 500 or so?
  // ATTN: In the future, need to reserve protobuf tag numbers for each NS, so maybe 1M?

  /*
     ATTN: prefixes:
      - Need to be able to lookup prefixes?
         - If so, that lookup is context-sensitive, depending on yang module location
      - Need to keep prefixes unique?
   */
};

} /* namespace rw_yang */

#endif /* __cplusplus */

__BEGIN_DECLS


void rw_namespace_manager_init(void);

void rw_namespace_manager_unittest_runtime_clear(void);

const char* rw_namespace_nsid_to_string(
  rw_namespace_id_t nsid);

rw_namespace_id_t rw_namespace_string_to_nsid(
  const char* ns);

rw_namespace_id_t rw_namespace_string_to_hash(
    const char* ns);

rw_namespace_id_t rw_namespace_find_or_register(
  const char* ns,
  const char* prefix);


__END_DECLS


/** @} */

#endif // RW_YANGTOOLS_NAMESPACE_H_
