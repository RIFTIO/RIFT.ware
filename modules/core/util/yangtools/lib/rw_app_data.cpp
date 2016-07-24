
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_app_data.cpp
 * @date 01/09/2014
 * @brief Rift App Data Management and caching
 */

#include <rw_app_data.hpp>
#include <rw_yang_mutex.hpp>
#include <ctype.h>
#include <iostream>

namespace rw_yang {

bool AppDataManager::app_data_get_token(
  const char* ns,
  const char* ext,
  AppDataTokenBase::deleter_f deleter,
  AppDataTokenBase* token)
{
  RW_ASSERT(ns);
  RW_ASSERT(ext);
  RW_ASSERT(deleter);
  RW_ASSERT(token);

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  // Search for existing entry.
  app_data_key_t key(ns, ext);
  auto ei = app_data_tokens_.find(key);
  if (ei != app_data_tokens_.end()) {
    *token = ei->second;
    return true;
  }

  // Cannot allocate another one?
  if (app_data_next_index_ >= AppDataCacheLevel::MAX_ATTRIBUTES) {
    return false;
  }

  app_data_value_t value(ns, ext, app_data_next_index_, deleter);
  auto status = app_data_tokens_.emplace(key, value);
  RW_ASSERT(status.second);

  app_data_deleters_.push_back(deleter);
  ++app_data_next_index_;
  *token = value;
  return true;
}

void AppDataManager::locked_app_data_delete(
  unsigned index,
  intptr_t data)
{
  RW_ASSERT(index < app_data_next_index_);
  app_data_deleters_[index](data);
}



/*****************************************************************************/

inline unsigned AppDataCacheLevel::make_base_index(
  unsigned index,
  unsigned child_levels) const
{
  // Mask of all upper-level bits, not includes any bits for my cache_ or below.
  return index & (MAX_ATTRIBUTES-1) & ((MAX_ATTRIBUTES-1) << ((child_levels+1) * LEVEL_SHIFT));
}

inline bool AppDataCacheLevel::is_valid_index(
  unsigned index,
  unsigned child_levels,
  unsigned base_index) const
{
  // Index is valid if, after masking, it equals my base_index.  If it
  // is not equal, generally means a bug or more levels are needed.
  return (index & make_base_index(index,child_levels)) == base_index;
}

inline unsigned AppDataCacheLevel::get_base_index(
  state_t temp_state) const
{
  return static_cast<unsigned>((temp_state & ST_BASE_INDEX_MASK) >> ST_BASE_INDEX_SHIFT);
}

inline unsigned AppDataCacheLevel::get_child_levels(
  state_t temp_state) const
{
  return static_cast<unsigned>((temp_state & ST_CHILD_LEVELS_MASK) >> ST_CHILD_LEVELS_SHIFT);
}

inline unsigned AppDataCacheLevel::get_cache_index(
  unsigned index,
  unsigned child_levels) const
{
  return (index >> (child_levels * LEVEL_SHIFT)) & LEVEL_MASK;
}

inline AppDataCacheLevel::state_t AppDataCacheLevel::get_is_cached_mask(
  unsigned cache_index) const
{
  return (state_t)1 << (ST_P_IS_CACHED_SHIFT+cache_index);
}

inline AppDataCacheLevel::state_t AppDataCacheLevel::get_is_owned_mask(
  unsigned cache_index) const
{
  return (state_t)1 << (ST_P_IS_OWNED_SHIFT+cache_index);
}

inline AppDataCacheLevel::state_t AppDataCacheLevel::get_is_looked_up_mask(
  unsigned cache_index) const
{
  return (state_t)1 << (ST_P_IS_LOOKED_UP_SHIFT+cache_index);
}

AppDataCacheLevel::AppDataCacheLevel(
  AppDataManager& app_data_mgr,
  unsigned base_index,
  unsigned child_levels,
  AppDataCacheLevel* first_child)
: app_data_mgr_(app_data_mgr)
{
  RW_ASSERT(child_levels < MAX_LEVELS);
  RW_ASSERT(child_levels || nullptr == first_child);
  RW_ASSERT(base_index < MAX_ATTRIBUTES);

  // Build the initial state: child levels, my index, and first-pointer ownership
  state_t temp_state = (state_t)child_levels << ST_CHILD_LEVELS_SHIFT;
  temp_state |= (state_t)base_index << ST_BASE_INDEX_SHIFT;
  if (first_child) {
    temp_state |= get_is_cached_mask(0) | get_is_owned_mask(0);
    cache_[0] = reinterpret_cast<intptr_t>(first_child);
  }
  state_ = temp_state;
}

AppDataCacheLevel::~AppDataCacheLevel()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  state_t temp_state = state_;
  state_t is_cached_and_owned = get_is_cached_mask(0) | get_is_owned_mask(0);
  unsigned base_index = get_base_index(temp_state);
  bool child_is_buffer = (temp_state & ST_CHILD_LEVELS_MASK) ? true : false;

  // Destroy all the pointers that this object owns.
  for( unsigned cache_index = 0; cache_index < CACHE_ARRAY_SIZE; (++cache_index), (is_cached_and_owned <<= 1)) {
    if ((temp_state & is_cached_and_owned) == is_cached_and_owned) {
      // How to destroy depends on whether the pointer is another buffer, or the application's pointer
      if (child_is_buffer) {
        delete reinterpret_cast<AppDataCacheLevel*>(cache_[cache_index].load());
      } else {
        app_data_mgr_.locked_app_data_delete(base_index + cache_index, cache_[cache_index].load());
      }
    }
  }
}

bool AppDataCacheLevel::is_cached(const AppDataTokenBase* token) const noexcept
{
  unsigned index = token->index_;
  state_t temp_state = state_;
  unsigned child_levels = get_child_levels(temp_state);
  unsigned base_index = get_base_index(temp_state);

  // Tree not tall enough?
  if (!is_valid_index(index, child_levels, base_index)) {
    return false;
  }

  unsigned cache_index = get_cache_index(index, child_levels);

  // Descend a level?
  if (child_levels) {
    AppDataCacheLevel* next_buf = reinterpret_cast<AppDataCacheLevel*>(cache_[cache_index].load());
    if (next_buf) {
      return next_buf->is_cached(token);
    }
    return false;
  }
  return (temp_state & get_is_cached_mask(cache_index)) ? true : false;
}

bool AppDataCacheLevel::is_looked_up(const AppDataTokenBase* token) const noexcept
{
  unsigned index = token->index_;
  state_t temp_state = state_;
  unsigned child_levels = get_child_levels(temp_state);
  unsigned base_index = get_base_index(temp_state);

  // Tree not tall enough?
  if (!is_valid_index(index, child_levels, base_index)) {
    return false;
  }

  unsigned cache_index = get_cache_index(index, child_levels);

  // Descend a level?
  if (child_levels) {
    AppDataCacheLevel* next_buf = reinterpret_cast<AppDataCacheLevel*>(cache_[cache_index].load());
    if (next_buf) {
      return next_buf->is_looked_up(token);
    }
    return false;
  }
  return (temp_state & get_is_looked_up_mask(cache_index)) ? true : false;
}

bool AppDataCacheLevel::check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept
{
  unsigned index = token->index_;
  state_t temp_state = state_;
  unsigned child_levels = get_child_levels(temp_state);
  unsigned base_index = get_base_index(temp_state);

  // Tree not tall enough?
  if (!is_valid_index(index, child_levels, base_index)) {
    return false;
  }

  unsigned cache_index = get_cache_index(index, child_levels);

  if (temp_state & get_is_cached_mask(cache_index)) {
    // Descend a level?
    if (child_levels) {
      AppDataCacheLevel* next_buf = reinterpret_cast<AppDataCacheLevel*>(cache_[cache_index].load());
      if (next_buf) {
        return next_buf->check_and_get(token, data);
      }
      return false;
    }
    RW_ASSERT(data);
    *data = cache_[cache_index];
    return true;
  }
  return false;
}

void AppDataCacheLevel::set_looked_up(const AppDataTokenBase* token, owner_ptr_t* top)
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (is_looked_up(token)) {
    return;
  }

  // Set a fake value...
  intptr_t junk_data = top->load()->set_impl(token, 0, top, (state_t)0);
  RW_ASSERT(0 == junk_data);

  // Then uncache it...
  junk_data = top->load()->set_uncached(token);
  RW_ASSERT(0 == junk_data);
}

intptr_t AppDataCacheLevel::set_impl(const AppDataTokenBase* token, intptr_t new_data, owner_ptr_t* top, state_t new_owned)
{
  RW_ASSERT(top);
  unsigned index = token->index_;
  state_t temp_state = state_;
  unsigned child_levels = get_child_levels(temp_state);
  unsigned base_index = get_base_index(temp_state);

  // Tree not tall enough?
  RW_ASSERT(top);
  if (*top == this && !is_valid_index(index, child_levels, base_index)) {
    AppDataCacheLevel* new_top = new AppDataCacheLevel(
      app_data_mgr_,
      make_base_index(base_index, child_levels+1),
      child_levels+1,
      this);
    *top = new_top;
    return new_top->set_impl(token, new_data, top, new_owned);
  }

  unsigned cache_index = get_cache_index(index, child_levels);
  state_t cached_mask = get_is_cached_mask(cache_index);
  state_t owned_mask = get_is_owned_mask(cache_index);
  state_t looked_up_mask = get_is_looked_up_mask(cache_index);
  bool is_cached = (temp_state & cached_mask) ? true : false;

  // Descend a level?
  if (child_levels) {
    AppDataCacheLevel* next_buf;
    if (is_cached) {
      // Just get the next level
      next_buf = reinterpret_cast<AppDataCacheLevel*>(cache_[cache_index].load());
    } else {
      // Next level doesn't exist.  Need to create it.
      next_buf = new AppDataCacheLevel(
        app_data_mgr_,
        make_base_index(index, child_levels-1),
        child_levels - 1);
      cache_[cache_index] = reinterpret_cast<intptr_t>(next_buf);

      // We own the new level.
      temp_state |= cached_mask | owned_mask;
      state_ = temp_state;
    }

    // Forward to the next level.
    return next_buf->set_impl(token, new_data, top, new_owned);
  }

  // Create the return value before updating
  bool is_owned = (temp_state & owned_mask) ? true : false;
  intptr_t old_data = 0;
  if (is_cached && is_owned) {
    old_data = cache_[cache_index];
  }

  // Update...
  cache_[cache_index] = new_data;
  temp_state |= cached_mask | looked_up_mask;
  if (new_owned) {
    temp_state |= owned_mask;
  }
  state_ = temp_state;

  return old_data;
}

intptr_t AppDataCacheLevel::set_and_give_ownership(const AppDataTokenBase* token, intptr_t data, owner_ptr_t* top)
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return top->load()->set_impl(token, data, top, (state_t)1);
}

intptr_t AppDataCacheLevel::set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data, owner_ptr_t* top)
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return top->load()->set_impl(token, data, top, (state_t)0);
}

intptr_t AppDataCacheLevel::set_uncached(const AppDataTokenBase* token)
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  unsigned index = token->index_;
  state_t temp_state = state_;
  unsigned child_levels = get_child_levels(temp_state);
  unsigned base_index = get_base_index(temp_state);

  // Tree not tall enough?
  if (!is_valid_index(index, child_levels, base_index)) {
    return 0;
  }

  unsigned cache_index = get_cache_index(index, child_levels);

  // Descend a level?
  if (child_levels) {
    AppDataCacheLevel* next_buf = reinterpret_cast<AppDataCacheLevel*>(cache_[cache_index].load());
    if (next_buf) {
      return next_buf->set_uncached(token);
    }
    return 0;
  }

  // Create the return value before updating
  state_t cached_mask = get_is_cached_mask(cache_index);
  state_t owned_mask = get_is_owned_mask(cache_index);
  bool is_cached = (temp_state & cached_mask) ? true : false;
  bool is_owned = (temp_state & owned_mask) ? true : false;
  intptr_t old_data = 0;
  if (is_cached && is_owned) {
    old_data = cache_[cache_index];
  }

  // Update...  STAYS LOOKED UP!
  temp_state &= ~(cached_mask | owned_mask);
  state_ = temp_state;

  return old_data;
}



/*****************************************************************************/
AppDataCache::AppDataCache(
  AppDataManager& app_data_mgr)
: app_data_mgr_(app_data_mgr),
  top_level_(nullptr)
{
}

AppDataCache::~AppDataCache()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  AppDataCacheLevel* temp_top = top_level_;
  if (temp_top) {
    delete temp_top;
  }
}

bool AppDataCache::is_cached(const AppDataTokenBase* token) const noexcept
{
  AppDataCacheLevel* temp_top = top_level_;
  if (temp_top) {
    return temp_top->is_cached(token);
  }
  return false;
}

bool AppDataCache::is_looked_up(const AppDataTokenBase* token) const noexcept
{
  AppDataCacheLevel* temp_top = top_level_;
  if (temp_top) {
    return temp_top->is_looked_up(token);
  }
  return false;
}

bool AppDataCache::check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept
{
  AppDataCacheLevel* temp_top = top_level_;
  if (temp_top) {
    return temp_top->check_and_get(token, data);
  }
  return false;
}

void AppDataCache::set_looked_up(const AppDataTokenBase* token)
{
  AppDataCacheLevel* temp_top = top_level_;
  if (!temp_top) {
    GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
    temp_top = top_level_;
    if (!temp_top) {
      temp_top = new AppDataCacheLevel(app_data_mgr_, 0);
      top_level_ = temp_top;
    }
  }
  temp_top->set_looked_up(token, &top_level_);
}

intptr_t AppDataCache::set_and_give_ownership(const AppDataTokenBase* token, intptr_t data)
{
  AppDataCacheLevel* temp_top = top_level_;
  if (!temp_top) {
    GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
    temp_top = top_level_;
    if (!temp_top) {
      temp_top = new AppDataCacheLevel(app_data_mgr_, 0);
      top_level_ = temp_top;
    }
  }
  return temp_top->set_and_give_ownership(token, data, &top_level_);
}

intptr_t AppDataCache::set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data)
{
  AppDataCacheLevel* temp_top = top_level_;
  if (!temp_top) {
    GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
    temp_top = top_level_;
    if (!temp_top) {
      temp_top = new AppDataCacheLevel(app_data_mgr_, 0);
      top_level_ = temp_top;
    }
  }
  return temp_top->set_and_keep_ownership(token, data, &top_level_);
}

intptr_t AppDataCache::set_uncached(const AppDataTokenBase* token)
{
  AppDataCacheLevel* temp_top = top_level_;
  if (temp_top) {
    return temp_top->set_uncached(token);
  }
  return 0;
}

} // namespace yang
