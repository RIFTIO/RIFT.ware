
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_namespace.cpp
 * @date 2014/09/02
 * @brief RiftWare general namespace utilities
 */

#include <rw_namespace.h>
#include <rw_yang_mutex.hpp>
#include <functional>

using namespace rw_yang;

void NamespaceManager::init()
{
  current_nsid_ = NSID_RUNTIME_BASE;
}

void NamespaceManager::register_fixed(
  rw_namespace_id_t nsid,
  const char* ns)
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  RW_ASSERT(nsid >= current_nsid_);

  // fill in nullptrs to deleted/missing namespaces.  std::vector cannot have holes
  while(current_nsid_ < nsid) {
    id2ns_fixed_.emplace_back(nullptr);
    ++current_nsid_;
  }

  // Save in the vector first; if the map insert blows up, things are safe (although broken).
  RW_ASSERT(id2ns_fixed_.size() == nsid);
  id2ns_fixed_.emplace_back(new Namespace(ns, nullptr/*prefix*/, nsid));
  ++current_nsid_;

  // Now update the map; if this blows up, the namespace leaks, but things otherwise work
  auto status = ns2id_.emplace(ns, nsid);
  RW_ASSERT(status.second);
}

rw_namespace_id_t NamespaceManager::register_runtime_locked(
  const char* ns,
  const char* prefix)
{
  RW_ASSERT(ns);
  RW_ASSERT(ns[0] != '\0');
  UNUSED(prefix);
  // ATTN: Is there a maximum namespace id we should assert on?

  RW_ASSERT((id2ns_runtime_.size() + NSID_RUNTIME_BASE) == current_nsid_);
  rw_namespace_id_t nsid = current_nsid_;

  // Save in the vector first; if the map insert blows up, things are safe (although broken).
  id2ns_runtime_.emplace_back(new Namespace(ns, nullptr/*prefix*/, nsid));
  ++current_nsid_;

  // Now update the map; if this blows up, the namespace leaks, but things otherwise work
  auto status = ns2id_.emplace(ns, nsid);
  RW_ASSERT(status.second);
  return nsid;
}


Namespace* NamespaceManager::get_nsobj_locked(
  rw_namespace_id_t nsid)
{
  Namespace* nsobj = nullptr;
  if (nsid < NSID_RUNTIME_BASE) {
    if (nsid < id2ns_fixed_.size()) {
      nsobj = id2ns_fixed_[nsid].get();
    }
  } else {
    nsid -= NSID_RUNTIME_BASE;
    if (nsid < id2ns_runtime_.size()) {
      nsobj = id2ns_runtime_[nsid].get();
    }
  }
  return nsobj;
}


void NamespaceManager::unittest_runtime_clear()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  for (auto& nsuptr: id2ns_runtime_) {
    auto count = ns2id_.erase(nsuptr->get_ns());
    RW_ASSERT(1 == count); // really should have found it - otherwise bugs lurk elsewhere
  }

  id2ns_runtime_.clear();
  current_nsid_ = NSID_RUNTIME_BASE;
}

const char* NamespaceManager::nsid_to_string(
  rw_namespace_id_t nsid)
{
  RW_ASSERT(nsid > NSID_NULL);
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  Namespace* nsobj = get_nsobj_locked(nsid);
  if (nsobj) {
    return nsobj->get_ns();
  }
  return nullptr;
}

rw_namespace_id_t NamespaceManager::string_to_nsid(
  const char* ns)
{
  RW_ASSERT(ns);
  RW_ASSERT(ns[0] != '\0');
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  auto idi = ns2id_.find(ns);
  if (idi == ns2id_.end()) {
    return NSID_NULL;
  }
  Namespace* nsobj = get_nsobj_locked(idi->second);
  RW_ASSERT(nsobj);
  return nsobj->get_nsid();
}

rw_namespace_id_t NamespaceManager::find_or_register(
  const char* ns,
  const char* prefix)
{
  RW_ASSERT(ns);
  RW_ASSERT(ns[0] != '\0');
  UNUSED(prefix);

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  auto idi = ns2id_.find(ns);
  if (idi == ns2id_.end()) {
    return register_runtime_locked(ns, prefix);
  }
  Namespace* nsobj = get_nsobj_locked(idi->second);
  RW_ASSERT(nsobj);
  return nsobj->get_nsid();
}

Namespace NamespaceManager::get_new_dynamic_schema_ns()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  // ATTN: Check for maximun number of dynamic schemas??
  std::string sn = std::to_string(++runtime_dyns_counter_);
  std::string ns(dyn_schema_ns+sn);

  auto hashi = ns2hash_.find(ns);
  RW_ASSERT(hashi == ns2hash_.end());

  std::string prefix(dyn_schema_prefix+sn);
  rw_namespace_id_t nsid = get_ns_hash(ns.c_str());

  std::string mn(dyn_schema_mn+sn);

  Namespace nsobj(ns.c_str(), nullptr, nsid);
  nsobj.set_module(mn);

  return nsobj;
}

bool NamespaceManager::ns_is_dynamic(const char* ns)
{
  std::string namesp(ns);
  std::size_t pos = namesp.find(dyn_schema_ns);
  if (pos != std::string::npos) {
    return true;
  }
  return false;
}

rw_namespace_id_t NamespaceManager::get_ns_hash(
    const char* ns)
{
  RW_ASSERT(ns);
  RW_ASSERT(ns[0] != '\0');
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  auto hashi = ns2hash_.find(ns);
  if (hashi == ns2hash_.end()) {
    rw_namespace_id_t hashv = NSID_NULL;

    std::size_t hash = std::hash<std::string>()(std::string(ns));
    hashv = hash & 0x1FFFFFFF; // Take lower 29 bits.

    auto ret = ns2hash_.emplace(ns, hashv);
    RW_ASSERT(ret.second);
    hashi = ret.first;

    // Insert in the hash to ns list as well.
    auto ret2 = hash2ns_.emplace(hashv, ns);
    RW_ASSERT(ret2.second);
  }

  return hashi->second;
}

const char* NamespaceManager::nshash_to_string(
    rw_namespace_id_t nsid)
{
  RW_ASSERT(nsid > NSID_NULL);
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  auto nsi = hash2ns_.find(nsid);
  if (nsi == hash2ns_.end()) {
    return nullptr;
  }
  return nsi->second.c_str();
}

void rw_namespace_manager_init(void)
{
  NamespaceManager::get_global();
}

void rw_namespace_manager_unittest_runtime_clear(void)
{
  NamespaceManager::get_global().unittest_runtime_clear();
}

const char* rw_namespace_nsid_to_string(
  rw_namespace_id_t nsid)
{
  return NamespaceManager::get_global().nsid_to_string(nsid);
}

rw_namespace_id_t rw_namespace_string_to_nsid(
  const char* ns)
{
  return NamespaceManager::get_global().string_to_nsid(ns);
}

rw_namespace_id_t rw_namespace_find_or_register(
  const char* ns,
  const char* prefix)
{
  return NamespaceManager::get_global().find_or_register(ns, prefix);
}

rw_namespace_id_t rw_namespace_string_to_hash(
    const char* ns)
{
  return NamespaceManager::get_global().get_ns_hash(ns);
}
