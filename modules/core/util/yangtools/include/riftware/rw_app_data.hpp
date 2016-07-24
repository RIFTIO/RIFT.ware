
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_app_data.hpp
 * @date 2014/09/01
 * @brief Include for AppDataCache.
 */

#ifndef RW_APP_DATA_HPP_
#define RW_APP_DATA_HPP_

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <sys/cdefs.h>
#include "rwlib.h"
#include <string>
#include <iterator>
#include <memory>
#include <map>
#include <vector>
#include <atomic>
#include <unordered_map>

namespace rw_yang {

/// A base class for Application Data Tokens.
/**
 * Application Data Tokens are abstract references to data that the
 * applications wants to store in YangNodes.  This class is a common
 * base class for use in basic interfaces.
 *
 * The application does not use this class raw - there are no public
 * interfaces in YangModel or YangNode that take these types.  Instead,
 * the application uses a template that derives from this class,
 * AppDataToken<>.  The template provides a measure of type safety,
 * and allows the application author to define how to destroy the data
 * when the model is destroyed.
 */
class AppDataTokenBase
{
 public:
  /// Deleter function type
  typedef void (*deleter_f)(intptr_t data);

  /// Default constructor
  AppDataTokenBase()
  : index_(0),
    deleter_(nullptr),
    ns_(""),
    ext_("")
  {}

  /// Basic namespace + extension Constructor
  AppDataTokenBase(
    /// [in] The namespace of the extension.
    const char* ns,

    /// [in] The name of the extension.
    const char* ext,

    /// [in] The deleter function.  May be nullptr.
    deleter_f deleter = nullptr)
  : index_(0),
    deleter_(nullptr),
    ns_(ns),
    ext_(ext)
  {}

  /// Constructor
  AppDataTokenBase(
    /// [in] The namespace of the extension.
    const char* ns,

    /// [in] The name of the extension.
    const char* ext,

    /// [in] The index in the model's list of know application data extensions.
    unsigned index,

    /// [in] The deleter function.  May be nullptr.
    deleter_f deleter)
  : index_(index),
    deleter_(deleter),
    ns_(ns),
    ext_(ext)
  {}

  // Is copyable.
  AppDataTokenBase(const AppDataTokenBase&) = default;

  /// The index of the application data cache.
  unsigned index_;

  /// The deleter
  deleter_f deleter_;

  /// The namespace of the extension.
  std::string ns_;

  /// The name of the extension.
  std::string ext_;
};


/// Application Data Deleter.
/**
 * Default Application Data deleter.  Defines a special-purpose deleter
 * function, which is used to delete application data when it is no
 * longer needed.
 */
template <class data_type>
class AppDataTokenDeleter
{
 public:
  typedef data_type data_t;

  static void deleter(intptr_t data)
  {
    delete reinterpret_cast<data_t>(data);
  }
};


/// Null Application Data Deleter.
/**
 * Null Application Data deleter.  Useful for things like function
 * pointers.
 */
class AppDataTokenDeleterNull
{
 public:
  typedef void data_t;

  static void deleter(intptr_t data)
  {
    UNUSED(data);
  }
};


/// Application Data Tokens.
/**
 * Application Data Tokens are abstract references to data that the
 * application wants to store in YangNodes.  The Token stores a slot
 * index, that uniquely identifies a piece of Application Data stored
 * in a YangNode.  The token is generated at runtime, which allows for
 * any application to define its own Data without requiring any custom
 * interfaces in YangNode.  The implementation is defined in terms of a
 * slot index, instead of the namespace and extention-name strings, for
 * effeciency; to lookup any piece of Application Data in a YangNode,
 * only a few array lookups are required.
 *
 * Any particular YangNode may have multiple pieces of Application Data
 * or none.  There is limited capacity for Application Data in the
 * YangNode, and the implementation expects reads to vastly outnumber
 * writes.  Do not use this mechanism for frequently modified data; if
 * an Application requires such data, then the application should build
 * its own tree.
 */
template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
class AppDataToken
: public AppDataTokenBase
{
 public:
  typedef data_type data_t;
  typedef deleter_type deleter_t;

  /// Default constructor
  AppDataToken()
  : AppDataTokenBase("", "", &deleter_t::deleter)
  {}

  /// Basic namespace + extension constructor.
  AppDataToken(
    /// [in] The namespace of the extension.
    const char* ns,

    /// [in] The name of the extension.
    const char* ext
  )
  : AppDataTokenBase(ns, ext, &deleter_t::deleter)
  {}

  /// Constructor for a particular slot.
  explicit AppDataToken(
    /// [in] The namespace of the extension.
    const char* ns,

    /// [in] The name of the extension.
    const char* ext,

    /// [in] The index in the model's list of know application data extensions.
    unsigned index)
  : AppDataTokenBase(ns, ext, index, &deleter_t::deleter)
  {}

  // Is copyable.
  AppDataToken(const AppDataToken&) = default;
};

class AppDataManager
{
 public:

  // Constructor.
  AppDataManager()
  {}

  /**
  * Public interface to registering or looking up Application Data
  * Tokens.  If there is no slot allocated for the Application Data,
  * one will be created.
   *
   * @return
   *  - true if the token could be initialized
   *  - false if the token could not be initialized
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  bool app_data_get_token(
    /// [in] The extension name space.  This name space doesn't have to
    /// map to a yang module, but if it does, then some additional
    /// helper functions become available in YangNode.  Cannot be
    /// nullptr.
    const char* ns,

    /// [in] The extension name.  If ns and ext map to an actual yang
    /// module extension, then you can lookup and cache an extension
    /// with a single member function call to a YangNode.  Otherwise,
    /// this does not necessarily need to map to any yang extension
    /// name.
    const char* ext,

    /// [out] The token.  Only valid on success.
    AppDataToken<data_type,deleter_type>* token);

   /**
   * Base interface to registering or looking up Application Data Base
   * Tokens.  Used by app_data_get_token<>().
   *
   * @return true if the token was successfully initialized
   */
  bool app_data_get_token(
    const char* ns, ///< [in] The extension name space.
    const char* ext, ///< [in] The extension name.
    AppDataTokenBase::deleter_f deleter, ///< [in] the deleter function
    AppDataTokenBase* token ///< [out] The token, if successful
  );

  void locked_app_data_delete(unsigned index, intptr_t data);

 private:
  typedef std::pair<std::string,std::string> app_data_key_t;
  typedef AppDataTokenBase app_data_value_t;
  typedef std::map<app_data_key_t,app_data_value_t> app_data_map_t;
  typedef AppDataTokenBase::deleter_f app_data_deleter_t;
  typedef std::vector<app_data_deleter_t> app_data_deleters_t;

  /// The next application data index to give out.
  /// ACCESSES MUST BE PROTECTED BY MUTEX.
  unsigned app_data_next_index_ = 0;

  /// The list of registered application data tokens.
  /// ACCESSES MUST BE PROTECTED BY MUTEX.
  app_data_map_t app_data_tokens_;

  /// The list of registered application data deleter functions.
  /// ACCESSES MUST BE PROTECTED BY MUTEX.
  app_data_deleters_t app_data_deleters_;
};


/// An extensible application data set
/**
 * This class defines an extensible, thread-safe, application data set.
 * The design assumes that relatively few attributes will ever exist in
 * any one set, but allows for all of them to exist.  Furthermore, it
 * heavily favors readers, and like all libncx-based cache variables,
 * requires an update to occur inside the model's mutex.  (Strictly
 * speaking, the mutex-locked update could be replaced by appropriate
 * usage of ext/util/urcu, but that would require changing the entire
 * library to remove the mutex and I do not want to do that right now -
 * TGS 2014/05/30).
 *
 * The cache defines a hierarchy of small buffers.  If there are only 16
 * total attributes that may be cached, a single-layer structure will
 * be created with one of these objects.  Each pointer in the object
 * will point directly at the cached value.  In this case, up to 3
 * atomic reads are required to check the status and obtain the pointer
 * value.  On x86, with std::atomic<>, each of these reads is a simple
 * read instruction without barriers.  The reads are:
 *  - Owner's atomic<AppDataCacheLevel*> pointer.
 *  - AppDataCacheLevel's state_ bit-vector.
 *  - AppDataCacheLevel's pointers_[] read.
 *
 * If more than 1 level exists, then for each additional level, 2 more
 * reads are required, per level, to check status:
 *  - Child AppDataCacheLevel's state_ bit-vector.
 *  - Child AppDataCacheLevel's pointers_[] read.
 *
 * Cache values may be changed, and levels may be added to the
 * attribute set, when protected by the model's global mutex.  Updates
 * do not require any reader synchronization other than the atomic
 * reads already described (although, see below for an important caveat
 * for the pointed-to data!).
 *
 * The following algorithm changes a pointer:
 *  - Obtain the mutex
 *  - If setting the cache and was not set before, or when changing a
 *    cached value:
 *     - Set the pointer
 *     - Set state_
 *  - Else, when clearing a cached value:
 *     - Set state_ to mark the value uncached
 *  - Release the mutex
 *
 * The following algorithm adds a level:
 *  - Obtain the mutex
 *  - Allocate as many levels as needed, and fill them in completely:
 *  - Save the pointer to the old top AppDataCacheLevel in the appropriate
 *    place (pointer[0]) in one of the new NcxCacheBuffers.
 *  - Update the owner's atomic<AppDataCacheLevel*> pointer.
 *  - Release the mutex
 *
 * There is an assumption that cache values change very rarely -
 * typically only when loading a new yang module, or when parsing a
 * manifest that changes behavior.  This class aims to make reading as
 * inexpensive as possible, while deferring all the hard work to
 * updates.  Also, the attribute delete operation is expected to be
 * very rare, so once the cost has been paid for an attribute
 * (AppDataCacheLevel allocation, multiple levels), it will never be
 * returned.  Although cached attributes can be forgotten, levels are
 * never removed.
 *
 * The cache buffer may, optionally, own each pointer that it contains.
 * The following rules determine ownership:
 *  - When the number of child levels is non-0, then all pointers are
 *    NcxCacheBuffers, and therefore they are all owned.  When the
 *    AppDataCacheLevel is destroyed, all of its children will also be
 *    destroyed.
 *  - When the number of child levels is 0, then the pointers are
 *    optionally owned by the AppDataCacheLevel.  The pointers are handled
 *    as follows:
 *     - If the pointer is not cached, do nothing.
 *     - If the pointer's pointer's is-owned flag is clear, do nothing.
 *     - Otherwise, both is-cached and is-owned are set, and the
 *       pointer must be deleted.  The Model contains the deleter
 *       function, in the attribute registration.
 */
class AppDataCacheLevel
{
 public:
  typedef std::atomic<AppDataCacheLevel*> owner_ptr_t;
  typedef uint64_t state_t;

 public:
  /**
   * Construct a cache buffer, with an associated model, number of
   * child levels, and specified first child.
   */
  AppDataCacheLevel(
    /// [in] The reference to the AppDataManager Class. Used for accessing the deleter function.
    AppDataManager& app_data_mgr,

    /// [in] The base index of this level.
    unsigned base_index,

    /// [in] The number of child levels. May be 0.
    unsigned child_levels = 0,

    /// [in] The first child.  May be nullptr.
    /// If not nullptr, then child_levels cannot be 0!
    AppDataCacheLevel* first_child = nullptr
  );

  /// Destructor.  Possibly destroys all child pointers.
  ~AppDataCacheLevel();

  // Cannot copy
  AppDataCacheLevel(const AppDataCacheLevel&) = delete;
  AppDataCacheLevel& operator=(const AppDataCacheLevel&) = delete;

 public:

  /// @see AppDataCache::is_cached(const AppDataTokenBase* token)
  bool is_cached(const AppDataTokenBase* token) const noexcept;

  /// @see AppDataCache::is_looked_up(const AppDataTokenBase* token)
  bool is_looked_up(const AppDataTokenBase* token) const noexcept;

  /// @see AppDataCache::check_and_get(const AppDataTokenBase* token, intptr_t* data)
  bool check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept;

  /// @see AppDataCache::set_looked_up(const AppDataTokenBase* token)
  void set_looked_up(const AppDataTokenBase* token, owner_ptr_t* top);

  /// @see AppDataCache::set_and_give_ownership(const AppDataTokenBase* token, intptr_t data);
  intptr_t set_and_give_ownership(const AppDataTokenBase* token, intptr_t data, owner_ptr_t* top);

  /// @see AppDataCache::set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data);
  intptr_t set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data, owner_ptr_t* top);

  /// @see AppDataCache::set_and_keep_ownership(const AppDataTokenBase* token);
  intptr_t set_uncached(const AppDataTokenBase* token);

  /// Constants
  enum: uint64_t {
    CACHE_ARRAY_SIZE = 16, ///< The number of pointers in one object
    MAX_ATTRIBUTES = 16*16*16, ///< The number of pointers in one object
    MAX_LEVELS = 3, ///< The maximum number of levels allowed
    LEVEL_SHIFT = 4, ///< The bit shift per level
    LEVEL_MASK = 0xF, ///< The mask for a level's worth of pointers
    ST_P_IS_CACHED_SHIFT = 0, ///< Offset for per-pointer is-cached bit, 16 bits
    ST_P_IS_OWNED_SHIFT = 16, ///< Offset for per-pointer is-owned bit, 16 bits
    ST_P_IS_LOOKED_UP_SHIFT = 32, ///< Offset for per-pointer is-looked-up bit, 16 bits
    ST_BASE_INDEX_SHIFT = 48, ///< Shift for index-base field, 12 bits
    ST_BASE_INDEX_MASK = 0xFFF000000000000ull, ///< Mask for index-base field, 12 bits
    ST_CHILD_LEVELS_SHIFT = 60, ///< Shift for child-levels field, 2 bits
    ST_CHILD_LEVELS_MASK = 0x3000000000000000ull, ///< Mask for child-levels field, 2 bits
    // remaining 2 bits are currently unused.
  };

 private:

  unsigned make_base_index(unsigned index, unsigned child_levels) const;
  bool is_valid_index(unsigned index, unsigned child_levels, unsigned base_index) const;
  unsigned get_base_index(state_t temp_state) const;
  unsigned get_child_levels(state_t temp_state) const;
  unsigned get_cache_index(unsigned child_levels, unsigned index) const;
  state_t get_is_cached_mask(unsigned cache_index) const;
  state_t get_is_owned_mask(unsigned cache_index) const;
  state_t get_is_looked_up_mask(unsigned cache_index) const;

  intptr_t set_impl(const AppDataTokenBase* token, intptr_t data, owner_ptr_t* top, state_t new_owned);

  /// The AppDataManager class associated with the model.
  AppDataManager& app_data_mgr_;

  /// The cache state
  std::atomic<state_t> state_;

  /// The cached pointers
  std::atomic<intptr_t> cache_[CACHE_ARRAY_SIZE];
};


/// Application Data Cache.
/**
 * This class defines a complete Application Data Cache.  It basically
 * is just a container for the top-level pointer, plus forwardnig
 * methods that hide the need to pass the top-level pointer to the
 * level objects.
 */
class AppDataCache
{
 public:
  /// Construct a cache.
  AppDataCache(
    /// [in] The AppDataManager Class associated with the model.
    AppDataManager& app_data_mgr
  );

  /// Destructor
  ~AppDataCache();

  // Cannot copy
  AppDataCache(const AppDataCache&) = delete;
  AppDataCache& operator=(const AppDataCache&) = delete;

 public:
  /**
   * Check if an attribute is cached
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  bool is_cached(
    /// [in] Token that defines the attribute.
    const AppDataTokenBase* token
  ) const noexcept;

  /**
   * Check if an attribute has been looked-up already.  Explicitly
   * setting the cached value counts as being looked up.
   *
   * @return
   *  - true: The value was looked-up or explicitly cached.
   *  - false: The value was not looked-up or explicitly cached.
   */
  bool is_looked_up(
    /// [in] Token that defines the attribute.
    const AppDataTokenBase* token
  ) const noexcept;

   /**
   * Check if an attribute is cached, and if it is cached, return the
   * pointer.
   *
   * @return
   *  - true: The value was cached.  *value contains the cached value.
   *  - false: The value was not cached.  *value was not modified.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  bool check_and_get(
    const AppDataToken<data_type,deleter_type>* token, ///< [in] Token that defines the attribute.
    data_type* data ///< [out] The data value, if cached.
  ) const noexcept;

  /**
   * Check if an attribute is cached, and if it is cached, returns the
   * attribute.
   *
   * @return
   *  - true: The value was cached.  *data contains the cached value.
   *  - false: The value was not cached.  *data was not modified.
   */
  bool check_and_get(
    /// [in] Token that defines the attribute.
    const AppDataTokenBase* token,

    /// [out] The cached value, if cached.
    intptr_t* data
  ) const noexcept;

  /**
   * Mark an attribute as having been looked-up already.  Explicitly
   * setting the cached value implicitly marks it as looked up; you
   * might do this when you tried to look it up and didn't find it.
   */
  void set_looked_up(
    /// [in] Token that defines the attribute.
    const AppDataTokenBase* token
  );

  /**
   * Cache an attribute, giving ownership of the pointer to the
   * YangModule.  If the pointer is still valid when the YangModule gets
   * deleted, the pointer will also be destroyed.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  data_type set_and_give_ownership(
    const AppDataToken<data_type,deleter_type>* token, ///< [in] the token
    data_type value ///< [in] The data to set
  );


  /**
   * Cache an attribute, giving ownership of the pointer to the
   * AppDataCacheLevel.  If the data is still valid when the
   * AppDataCacheLevel gets deleted, the data will also be
   * destroyed.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the AppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    AppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  intptr_t set_and_give_ownership(
    /// [in] Token that defines the attribute.
    const AppDataTokenBase* token,

    /// [in] The data to cache.  The AppDataCacheLevel takes ownership.
    intptr_t data
  );

   /**
   * Cache an attribute, with the application (or some other entity)
   * retaining ownership of the pointer.  When the YangModule gets
   * deleted, nothing will be done with the pointer.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  data_type set_and_keep_ownership(
    const AppDataToken<data_type,deleter_type>* token, ///< [in] the token
    data_type value ///< [in] The app data item to set
  );

  /**
   * Cache an attribute, with the application (or some other entity)
   * retaining ownership of the data.  When the AppDataCacheLevel
   * gets deleted, nothing will be done with the data.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the AppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    AppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  intptr_t set_and_keep_ownership(
    /// [in] Token that defines the attribute.
    const AppDataTokenBase* token,

    /// [in] The data to cache.  The AppDataCacheLevel takes ownership.
    intptr_t data
  );

  /**
   * Uncache an attribute.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the AppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    AppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  data_type set_uncached(
    const AppDataToken<data_type,deleter_type>* token ///< [in] the token
  );

  /**
   * Uncache an attribute.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the AppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    AppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  intptr_t set_uncached(
    /// [in] Token that defines the attribute.
    const AppDataTokenBase* token
  );

 private:
  /// The manager the cache came from.
  AppDataManager& app_data_mgr_;

  /// The current top level.
  AppDataCacheLevel::owner_ptr_t top_level_;
};

template <class data_type, class deleter_type>
bool AppDataCache::check_and_get(
  const AppDataToken<data_type,deleter_type>* token,
  data_type* data
) const noexcept
{
  intptr_t intptr_data = 0;
  bool retval = this->check_and_get(static_cast<const AppDataTokenBase*>(token), &intptr_data);
  if (retval) {
   *data = reinterpret_cast<data_type>(intptr_data);
  }
  return retval;
}

template <class data_type, class deleter_type>
data_type AppDataCache::set_and_give_ownership(
  const AppDataToken<data_type,deleter_type>* token,
  data_type data
)
{
  intptr_t intptr_data = this->set_and_give_ownership(
    static_cast<const AppDataTokenBase*>(token),
    reinterpret_cast<intptr_t>(data));
  return reinterpret_cast<data_type>(intptr_data);
}

template <class data_type, class deleter_type>
data_type AppDataCache::set_and_keep_ownership(
  const AppDataToken<data_type,deleter_type>* token,
  data_type data
)
{
  intptr_t intptr_data = this->set_and_keep_ownership(
    static_cast<const AppDataTokenBase*>(token),
    reinterpret_cast<intptr_t>(data));
  return reinterpret_cast<data_type>(intptr_data);
}

template <class data_type, class deleter_type>
data_type AppDataCache::set_uncached(
  const AppDataToken<data_type,deleter_type>* token
)
{
  intptr_t intptr_data = this->set_uncached(
    static_cast<const AppDataTokenBase*>(token));
  return reinterpret_cast<data_type>(intptr_data);
}

template <class data_type, class deleter_type>
inline bool AppDataManager::app_data_get_token(
  const char* ns,
  const char* ext,
  AppDataToken<data_type,deleter_type>* token)
{
  RW_STATIC_ASSERT(sizeof(data_type) == sizeof(intptr_t));
  AppDataTokenBase base(ns,ext);
  bool retval = this->app_data_get_token(
    ns,
    ext,
    static_cast<AppDataTokenBase::deleter_f>(&deleter_type::deleter),
    &base);
  *static_cast<AppDataTokenBase*>(token) = base;
  return retval;
}

} //namespace rw_yang

#endif // RW_APP_DATA_HPP_
