
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file yangncx_impl.hpp
 *
 * Yang model based on libncx, private implementation.
 */

#ifndef RW_YANGTOOLS_YANGNCX_IMPL_HPP_
#define RW_YANGTOOLS_YANGNCX_IMPL_HPP_

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <atomic>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <initializer_list>
#include <algorithm>

#ifndef RW_YANGTOOLS_YANGNCX_HPP_
#include "yangncx.hpp"
#endif

#include "rw_app_data.hpp"
#include "ncxmod.h"
#include "ncx.h"

namespace rw_yang {

#define YANGPTR_NOT_CACHED(f_) (static_cast<(decltype(this->f_)>((void*)this)))

class YangModelNcxImpl;
class YangNodeNcx;
class YangNodeNcxRoot;
class YangTypeNcx;
class YangTypeNcxAnyxml;
class YangValueNcx;
class YangValueNcxEnum;
class YangValueNcxBits;
class YangValueNcxIdref;
class YangValueAnyxml;
class YangKeyNcx;
class YangAugmentNcx;
class YangUsesNcx;
class YangExtNcx;
class YangExtNcxNode;
class YangExtNcxType;
class YangModuleNcx;
class YangTypedEnumNcx;
class YangGroupingNcx;

class NcxVersion;
class NcxCacheState;


//! Attribute-is-cached indications.
/*!
 * These indications are used to identify cacheable libncx yang model object
 * attributes.  The indications are used as offsets to a std::bitset<>, in
 * conjunction with @ref rw_yang::NcxCacheState and @ref
 * rw_yang::NcxCached<>.  Each object type has an overlapping
 * indication range, because the indications are not shared between
 * types.
 */
enum ncx_cached_t
{
  NCX_CACHED_NULL = 0,

  NCX_CACHED_EXT_base = NCX_CACHED_NULL,
  NCX_CACHED_EXT_NEXT_EXTENSION,
  NCX_CACHED_EXT_end,

  NCX_CACHED_VALUE_base = NCX_CACHED_NULL,
  NCX_CACHED_VALUE_HELP_SHORT,
  NCX_CACHED_VALUE_MAX_LENGTH,
  NCX_CACHED_VALUE_FIRST_EXTENSION,
  NCX_CACHED_VALUE_end,

  NCX_CACHED_TYPE_base = NCX_CACHED_NULL,
  NCX_CACHED_TYPE_FIRST_VALUE,
  NCX_CACHED_TYPE_FIRST_EXTENSION,
  NCX_CACHED_TYPE_end,

  NCX_CACHED_NODE_base = NCX_CACHED_NULL,
  NCX_CACHED_NODE_HELP_SHORT,
  NCX_CACHED_NODE_NEXT_SIBLING,
  NCX_CACHED_NODE_FIRST_CHILD,
  NCX_CACHED_NODE_FIRST_KEY,
  NCX_CACHED_NODE_FIRST_EXTENSION,
  NCX_CACHED_NODE_USES,
  NCX_CACHED_NODE_PB_FIELD_NAME,
  NCX_CACHED_NODE_AUGMENT,
  NCX_CACHED_NODE_LEAFREF_REF,
  NCX_CACHED_NODE_HAS_DEFAULT,
  NCX_CACHED_NODE_end,

  NCX_CACHED_KEY_base = NCX_CACHED_NULL,
  NCX_CACHED_KEY_NEXT_KEY,
  NCX_CACHED_KEY_end,

  NCX_CACHED_AUGMENT_base = NCX_CACHED_NULL,
  NCX_CACHED_AUGMENT_NEXT_AUGMENT,
  NCX_CACHED_AUGMENT_FIRST_EXTENSION,
  NCX_CACHED_AUGMENT_end,

  NCX_CACHED_MODULE_base = NCX_CACHED_NULL,
  NCX_CACHED_MODULE_FIRST_NODE,
  NCX_CACHED_MODULE_LAST_NODE,
  NCX_CACHED_MODULE_NEXT_MODULE,
  NCX_CACHED_MODULE_WAS_LOADED, // Implies load_name_.size()
  NCX_CACHED_MODULE_FIRST_EXTENSION,
  NCX_CACHED_MODULE_FIRST_AUGMENT,
  NCX_CACHED_MODULE_FIRST_TYPEDEF,
  NCX_CACHED_MODULE_FIRST_IMPORT,
  NCX_CACHED_MODULE_FIRST_GROUPING,
  NCX_CACHED_MODULE_LAST_GROUPING,
  NCX_CACHED_MODULE_end,

  NCX_CACHED_TYPEDEF_base = NCX_CACHED_NULL,
  NCX_CACHED_TYPEDEF_NEXT_TYPEDEF,
  NCX_CACHED_TYPEDEF_FIRST_VALUE,
  NCX_CACHED_TYPEDEF_FIRST_EXTENSION,
  NCX_CACHED_TYPEDEF_end,

  NCX_CACHED_GROUPING_base = NCX_CACHED_NULL,
  NCX_CACHED_GROUPING_FIRST_CHILD,
  NCX_CACHED_GROUPING_FIRST_EXTENSION,
  NCX_CACHED_GROUPING_NEXT_GROUPING,
  NCX_CACHED_GROUPING_USES,
};


//! libncx yang model cached attribute indications.
/*!
 * Many attributes of the libncx yang model are lazy-initialized,
 * because they are not needed by all users of the library, and because
 * the attributes are expensive to calculate or consume memory.  The
 * cache indications identify which attributes have been already been
 * cached.
 *
 * This class provides THE central thread safety abstraction.  The
 * library uses the Double-Checked Locking Pattern for all
 * lazy-initialized and version-dependent model attributes.  This class
 * contains the atomic variable that the DCLP depends on.  All accesses
 * to a any such attribute MUST be preceded by an access the atomic
 * member is_cached, in order to ensure proper propagation of cached
 * attribute changes from one thread to another.  Furthermore, changes
 * to the cached attributes AND THESE INDICATIONS, must be protected by
 * the mutex, always ending with a write to is_cached (even if there is
 * no change to the indication).  Failure to do so will cause this
 * library to lose thread safety.
 *
 * Only lazy-initialized and version-dependent model attributes require
 * this protection.  Attributes that are only set by the object's
 * constructor are always safe to access, because they will never be
 * set from different threads, and the constructor is presumed to run
 * under the protection of the model's mutex.
 *
 * ATTN: Set-once fields can be optimized by not keeping the cached
 * value in a std::atomic<>.  In such cases, the cached value is
 * published by the write to the indiciations.
 */
class NcxCacheState
{
public:
  NcxCacheState()
  : is_cached_(0)
  {}

  // Cannot copy
  NcxCacheState(const NcxCacheState&) = delete;
  NcxCacheState& operator=(const NcxCacheState&) = delete;

public:
  typedef std::atomic_uint_fast32_t atomic_t;
  typedef std::uint_fast32_t fast_t;

  /*!
   * @brief Check if a value is cached.
   *
   * @return true if the value is cached, false otherwise.
   */
  bool is_cached(
    ncx_cached_t attribute //!< [in] The attribute
  ) const
  {
    return is_cached_ & (1<<(attribute-1));
  }

  /*!
   * @brief Check if a flag is cached.
   *
   * @return true if the flag is cached, false otherwise.
   */
  bool is_cached_and_get_flag(
    ncx_cached_t is_cached_attribute, //!< [in] The is-cached attribute
    ncx_cached_t flag_attribute, //!< [in] The flag attribute
    bool* flag_value //!< [out] The value of the flag, if it was cached
  ) const
  {
    NcxCacheState::fast_t copy = is_cached_;
    if (copy & (1<<(is_cached_attribute-1))) {
      RW_ASSERT(flag_value);
      *flag_value = static_cast<bool>(copy & (1<<(flag_attribute-1)));
      return true;
    }
    return false;
  }


  /*!
   * Set a cached flag.  Only use this for large, set-once, objects!
   */
  void locked_cache_set_flag_only(
    ncx_cached_t attribute //!< [in] The attribute
  )
  {
    NcxCacheState::fast_t before = is_cached_;
    NcxCacheState::fast_t after = before | (1<<(attribute-1));
    if (before == after) {
      RW_ASSERT_NOT_REACHED();
    }
    bool okay = is_cached_.compare_exchange_strong(before, after);
    RW_ASSERT(okay); // Assumed to be under lock, therefore CAS MUST succeed.
  }

  //! The atomic is-cached indications
  atomic_t is_cached_;
};


//! Utility class for write-once cached values.
struct ncx_cached_write_once
{
  //! Write-once semantics - must crash.
  static void already_cached() { RW_ASSERT_NOT_REACHED(); }

  //! Value can be raw type because it is never modified again
  //! after being set in the original setting critical section.
  template <typename value_t>
  using cached_value_t = value_t;
};


//! Utility class for write-many cached values.
struct ncx_cached_write_many
{
  //! Write-many is okay.
  static void already_cached() {}

  //! Value must be atomic because it can be set multiple times.
  template <typename value_t>
  using cached_value_t = std::atomic<value_t>;
};


//! A libncx cached attribute class template.
/*!
 * This class template encapsulates a cached attribute of the libncx
 * yang model, enforcing correct, thread safe, caching behavior.
 */
template <
  typename value_t_,
  ncx_cached_t attribute_,
  typename behavior_t_ = ncx_cached_write_once >
class NcxCached
{
public:
  typedef value_t_ value_t;
  typedef behavior_t_ behavior_t;
  typedef typename behavior_t::template cached_value_t<value_t> cache_value_t;
  static const ncx_cached_t attribute = attribute_;

  /*!
   * @brief Default constructor, just 0-initializes the cached value.
   * Assumes the value is uncached.
   */
  NcxCached()
  : cache_()
  {}

  // Cannot copy
  NcxCached(const NcxCached&) = delete;
  NcxCached& operator=(const NcxCached&) = delete;

  /*!
   * @brief Check if the value is cached.
   *
   * @return true if the value is cached, false otherwise.
   */
  bool is_cached(
    const NcxCacheState& indications //!< [in] The attribute cached indications
  ) const
  {
    return indications.is_cached_ & (1<<(attribute-1));
  }

  /*!
   * @brief Check if the value is cached, and return a copy of the
   * value if it is cached.
   *
   * @return true if the value is cached, false otherwise.
   */
  bool is_cached_and_get(
    const NcxCacheState& indications, //!< [in] The attribute cached indications
    value_t* cached_value //!< [in,out] Pointer to save a copy of the cached value, if cached
  ) const
  {
    RW_ASSERT(cached_value);
    if (!is_cached(indications)) {
      return false;
    }
    *cached_value = cache_;
    return true;
  }

  /*!
   * @brief Set the cached value.  Automatically sets the
   * cached-indication.
   *
   * @return The newly set value
   */
  value_t locked_cache_set(
    NcxCacheState* indications, //!< [in,out] The attribute cached indications
    value_t new_value //!< [in] The new cache value
  )
  {
    cache_ = new_value;
    NcxCacheState::fast_t before = indications->is_cached_;
    NcxCacheState::fast_t after = before | (1<<(attribute-1));
    if (before == after) {
      behavior_t::already_cached();
    }
    bool okay = indications->is_cached_.compare_exchange_strong(before, after);
    RW_ASSERT(okay); // Assumed to be under lock, therefore CAS MUST succeed.
    return new_value;
  }

private:
  //! The cached value.
  cache_value_t cache_;
};


//! A libncx cached attribute flag class template.
/*!
 * This class template encapsulates a cached boolean attribute of the
 * libncx yang model, enforcing correct, thread safe, caching behavior
 * for boolean flags that are stored in a NcxCacheState along with
 * their iss-cached indication.
 */
template <
  ncx_cached_t flag_attribute_,
  ncx_cached_t is_cached_attribute_,
  typename behavior_t_ = ncx_cached_write_once >
class NcxCachedFlag
{
public:
  typedef behavior_t_ behavior_t;
  static const ncx_cached_t flag_attribute = flag_attribute_;
  static const ncx_cached_t is_cached_attribute = is_cached_attribute_;

  /*!
   * @brief Check if the flag is cached.
   *
   * @return true if the flag is cached, false otherwise.
   */
  static bool is_cached(
    const NcxCacheState& indications //!< [in] The attribute cached indications
  )
  {
    return indications.is_cached(is_cached_attribute);
  }

  /*!
   * @brief Check if the flag is cached, and return a copy of the
   * flag if it is cached.
   *
   * @return true if the flag is cached, false otherwise.
   */
  static bool is_cached_and_get(
    const NcxCacheState& indications, //!< [in] The attribute cached indications
    bool* cached_value //!< [in,out] Pointer to save a copy of the cached value, if cached
  )
  {
    RW_ASSERT(cached_value);
    return indications.is_cached_and_get_flag(is_cached_attribute,flag_attribute,cached_value);
  }

  /*!
   * @brief Set the cached flag.  Automatically sets the
   * cached-indication.
   *
   * @return The newly set value
   */
  static bool locked_cache_set(
    NcxCacheState* indications, //!< [in,out] The attribute cached indications
    bool new_value //!< [in] The new cache value
  )
  {
    NcxCacheState::fast_t before = indications->is_cached_;
    if (before & (1<<(is_cached_attribute-1))) {
      behavior_t::already_cached();
    }
    NcxCacheState::fast_t after = before | (1<<(is_cached_attribute-1)) | ((new_value?1:0)<<(flag_attribute-1));
    bool okay = indications->is_cached_.compare_exchange_strong(before, after);
    RW_ASSERT(okay); // Assumed to be under lock, therefore CAS MUST succeed.
    return new_value;
  }
};


//! A libncx model version number
/*!
 * This class keeps track of the model version that was last used to
 * update cached attributes of an object.  It provides methods for
 * detecting a version change and updating after a change.
 */
class NcxVersion
{
public:
  NcxVersion(YangModelNcxImpl& ncxmodel);

  // Cannot copy
  NcxVersion(const NcxVersion&) = delete;
  NcxVersion& operator=(const NcxVersion&) = delete;

  bool is_up_to_date(YangModelNcxImpl& ncxmodel);
  void updated(YangModelNcxImpl& ncxmodel);

  //! The last version used for a cached attribute.
  std::atomic<unsigned> version_ = {};
};

/*!
 * Yang extension descriptor for libncx.  This class describes a single
 * extension associated with another object in the YANG schema model.
 * These descriptors are owned by the model.
 */
class YangExtNcx
: public YangExtension
{
public:
  YangExtNcx(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo);
  ~YangExtNcx() {}

  // Cannot copy
  YangExtNcx(const YangExtNcx&) = delete;
  YangExtNcx& operator=(const YangExtNcx&) = delete;

public:
  std::string get_location();
  std::string get_location_ext();
  const char* get_value();
  const char* get_description_ext();
  const char* get_name();
  const char* get_prefix();
  const char* get_module_ext();
  const char* get_ns();
  YangExtNcx* get_next_extension();

public:
  virtual YangExtNcx* locked_update_next_extension();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx extension data.
  ncx_appinfo_t* const appinfo_;

  //! The true, complete, namespace of the extension definition.  Owned
  //! by libncx.
  const char* ns_;

  //! The next extension in the list.  Initialization delayed until
  //! first use.
  NcxCached<YangExtNcx*,NCX_CACHED_EXT_NEXT_EXTENSION> next_extension_;
};


/*!
 * Yang extension descriptor for a libncx node.  This class exists
 * because relevant extensions may also be found in the uses and
 * grouping statements, and YangExtNcx does not have the ability to
 * find them.
 */
class YangExtNcxNode
: public YangExtNcx
{
public:
  YangExtNcxNode(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo, obj_template_t* obj);
  ~YangExtNcxNode() {}

  // Cannot copy
  YangExtNcxNode(const YangExtNcxNode&) = delete;
  YangExtNcxNode& operator=(const YangExtNcxNode&) = delete;

public:
  YangExtNcx* locked_update_next_extension();

public:
  //! The enclosing object
  obj_template_t* const obj_;
};


/*!
 * Yang extension descriptor for a libncx grouping.
 */
class YangExtNcxGrouping
: public YangExtNcx
{
public:
  YangExtNcxGrouping(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo, grp_template_t* grp);
  ~YangExtNcxGrouping() {}

  // Cannot copy
  YangExtNcxGrouping(const YangExtNcxGrouping&) = delete;
  YangExtNcxGrouping& operator=(const YangExtNcxGrouping&) = delete;

public:
  YangExtNcx* locked_update_next_extension();

public:
  //! The enclosing object
  grp_template_t* const grp_;
};

/*!
 * Yang extension descriptor for a libncx type. This class exists
 * because relevant extensions may also be found in the types
 * type-definition.
 */
class YangExtNcxType
: public YangExtNcx
{
 public:
  YangExtNcxType(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo, typ_def_t_ *typ);
  ~YangExtNcxType() {}

 // Cannot copy
  YangExtNcxType(const YangExtNcxType&) = delete;
  YangExtNcxType& operator=(const YangExtNcxType&) = delete;

public:
 YangExtNcx* locked_update_next_extension();

public:
  /// The enclosing object
  typ_def_t* const typ_;
};


/*!
 * Yang value descriptor for libncx.  This class describes a single
 * accepted value for a type in the YANG schema model.  The value
 * descriptor may be shared across multiple types in the model.  These
 * descriptors are owned by the model.
 */
class YangValueNcx
: public YangValue
{
public:
  YangValueNcx(YangModelNcxImpl& ncxmodel, YangTypeNcx* owning_type, typ_def_t* typ);
  ~YangValueNcx();

  // Cannot copy
  YangValueNcx(const YangValueNcx&) = delete;
  YangValueNcx& operator=(const YangValueNcx&) = delete;

public:
  const char* get_name();
  std::string get_location();
  const char* get_description();
  const char* get_help_short();
  const char* get_help_full();
  const char* get_prefix();
  const char* get_ns();
  rw_yang_leaf_type_t get_leaf_type();
  uint64_t get_max_length();
  YangValueNcx* get_next_value();
  rw_status_t parse_value(const char* value_string);
  rw_status_t parse_partial(const char* value_string) { return parse_value(value_string); }
  YangExtNcx* get_first_extension();
  virtual uint32_t get_position() {return 0;};

public:
  virtual YangExtNcx* locked_update_first_extension();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx type definition.
  typ_def_t* const typ_;

  //! The value typename.  May be yang type, or the name of the closest
  //! typedef.  Owned by libncx.
  const char* name_;

  //! The value description.  Owned by libncx.
  const char* description_;

  //! The value short help.  Derived from description.  Created upon
  //! first use.  Protected by NCX_CACHED_VALUE_HELP_SHORT.
  std::string help_short_;

  //! The namespace prefix used (if any) in the defining yang file.
  //! Possibly empty.  Owned by libncx.
  const char* prefix_;

  //! The true, complete, namespace of the value.  Owned by libncx.
  const char* ns_;

  //! Cached leaf type.
  rw_yang_leaf_type_t leaf_type_;

  //! Cached maximum length.  Initialization deferred until needed.
  NcxCached<uint64_t,NCX_CACHED_VALUE_MAX_LENGTH> max_length_;

  //! The owning type.
  YangTypeNcx* const owning_type_;

  //! The next value in the list.  nullptr means last.  Initialized during
  //! YangTypeNcx construction, so this is always valid.
  //! ATTN: What about when an import adds a new identity, or enum value?
  YangValueNcx* next_value_;

  //! First extension child.   nullptr means none.  Initialization
  //! deferred until needed.
  NcxCached<YangExtNcx*,NCX_CACHED_VALUE_FIRST_EXTENSION> first_extension_;
};


/*!
 * Yang keyword-based value descriptor for libncx.  This is used as a
 * base class for all keyword-based values, to share implementation.
 */
class YangValueNcxKeyword
: public YangValueNcx
{
public:
  YangValueNcxKeyword(YangModelNcxImpl& ncxmodel, YangTypeNcx* owning_type, typ_def_t* typ);
  ~YangValueNcxKeyword() {}

  // Cannot copy
  YangValueNcxKeyword(const YangValueNcxKeyword&) = delete;
  YangValueNcxKeyword& operator=(const YangValueNcxKeyword&) = delete;

public:
  rw_status_t parse_value(const char* value_string);
  rw_status_t parse_partial(const char* value_string);
  virtual bool is_keyword() { return true; }
};

/*!
 * Yang boolean value descriptor for libncx.  This class is used to describe
 * either a true or false boolean value in the YANG schema model.  The value
 * descriptor is meant to be shared across multiple types within the model.
 * Each is owned by the model itself.
 */
class YangValueNcxBool
: public YangValueNcxKeyword
{
public:
  YangValueNcxBool(YangModelNcxImpl& ncxmodel, YangTypeNcx* owning_type, typ_def_t *typ, bool val);
  ~YangValueNcxBool() {}

  // Cannot copy
  YangValueNcxBool(const YangValueNcxBool&) = delete;
  YangValueNcxBool& operator=(const YangValueNcxBool&) = delete;

public:
  const char * get_name();
  rw_status_t parse_value(const char* value_string);
  rw_status_t parse_partial(const char* value_string);

private:
  std::vector<std::string> vals_;
};

/*!
 * Yang enum value descriptor for libncx.  This class describes a
 * single accepted enum value in the YANG schema model.  The value
 * descriptor may be shared across multiple types in the model.  These
 * descriptors are owned by the model.
 */
class YangValueNcxEnum
: public YangValueNcxKeyword
{
public:
  YangValueNcxEnum(YangModelNcxImpl& ncxmodel, YangTypeNcx* owning_type, typ_def_t* typ, typ_enum_t* typ_enum);
  ~YangValueNcxEnum() {}

  // Cannot copy
  YangValueNcxEnum(const YangValueNcxEnum&) = delete;
  YangValueNcxEnum& operator=(const YangValueNcxEnum&) = delete;

public:
  // ATTN: const char* get_syntax();
  virtual uint32_t get_position();

protected:
  virtual YangExtNcx* locked_update_first_extension();

public:
  //! The libncx enum value definition.
  typ_enum_t* const typ_enum_;
};

/*!
 * Yang IdRef value descriptor for libncx.  This class describes a
 * single accepted IdRef value in the YANG schema model.  The value
 * descriptor may be shared across multiple types in the model.  These
 * descriptors are owned by the model.
 */
class YangValueNcxIdRef
: public YangValueNcxKeyword
{
public:
  YangValueNcxIdRef(YangModelNcxImpl& ncxmodel, YangTypeNcx* owning_type, typ_def_t* typ, ncx_identity_t_* typ_idref);
  ~YangValueNcxIdRef() {}

  // Cannot copy
  YangValueNcxIdRef(const YangValueNcxIdRef&) = delete;
  YangValueNcxIdRef& operator=(const YangValueNcxIdRef&) = delete;

public:
  // ATTN: const char* get_syntax();

public:
  //! The libncx identity ref  definition.
  ncx_identity_t_* const typ_idref_;
  virtual bool is_keyword() { return true; }
};

/*!
 * Yang bits value descriptor for libncx.  This class describes a
 * single accepted bits value in the YANG schema model.  The value
 * descriptor may be shared across multiple types in the model.  These
 * descriptors are owned by the model.
 */
class YangValueNcxBits
: public YangValueNcxKeyword
{
public:
  YangValueNcxBits(YangModelNcxImpl& ncxmodel, YangTypeNcx* owning_type, typ_def_t* typ, typ_enum_t* typ_enum);
  ~YangValueNcxBits() {}

  // Cannot copy
  YangValueNcxBits(const YangValueNcxBits&) = delete;
  YangValueNcxBits& operator=(const YangValueNcxBits&) = delete;

public:
  // ATTN: const char* get_syntax();
  uint32_t get_position() override;

public:
  //! The libncx enum value definition.
  typ_enum_t* const typ_enum_;
  int32_t position_ = 0;
};


/*!
 * Yang type descriptor for libncx.  This class describes a single leaf
 * type in the YANG schema model.  The type may be shared across
 * multiple leaf or leaf-list entries in the model.  These objects are
 * owned by the model.
 */
class YangTypeNcx
: public YangType
{
public:
  YangTypeNcx(YangModelNcxImpl& ncxmodel, typ_def_t* typ);
  ~YangTypeNcx() {}

  // Cannot copy
  YangTypeNcx(const YangTypeNcx&) = delete;
  YangTypeNcx& operator=(const YangTypeNcx&) = delete;

public:
  typedef std::unique_ptr<YangTypeNcx> ptr_t;
  typedef typ_def_t* key_t;
  typedef std::unordered_map<key_t,ptr_t> map_t;

  typedef std::pair<YangValueNcx*,YangValueNcx*> expand_typ_t;

public:
  std::string get_location();
  const char* get_prefix() { RW_ASSERT_NOT_REACHED(); }
  const char* get_ns() { return ns_; }
  rw_yang_leaf_type_t get_leaf_type();
  YangValue* get_first_value();
  YangExtNcx* get_first_extension();
  virtual bool is_imported();
  virtual YangTypedEnum* get_typed_enum();
  uint8_t get_dec64_fraction_digits();
protected:
  virtual YangValue* locked_update_first_value();
  virtual YangExtNcx* locked_update_first_extension();

  virtual expand_typ_t locked_expand_typ(typ_def_t* top_typ, typ_def_t* cur_typ, bool top);
  virtual expand_typ_t locked_expand_boolean(typ_def_t* typ);
  virtual expand_typ_t locked_expand_union(typ_def_t* top_typ, typ_def_t* typ);
  virtual expand_typ_t locked_expand_enum(typ_def_t* top_typ, typ_def_t* typ);
  virtual expand_typ_t locked_expand_bits(typ_def_t* top_typ, typ_def_t* typ);
  virtual expand_typ_t locked_expand_idref(typ_def_t* top_typ, typ_def_t* typ);

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx type information.  Owned by libncx.
  typ_def_t* const typ_;

  //! For typedefed named enumerations, the typedefinition that defines this.
  YangTypedEnumNcx *typed_enum_;
  //! First value child.  nullptr if no known values (ATTN: such as empty?)
  // ATTN: What if new identities get added?  Allowed to find them?  Need version check?

  NcxCached<YangValue*,NCX_CACHED_TYPE_FIRST_VALUE> first_value_;

  //! First extension child.  nullptr if no extensions.
  NcxCached<YangExtNcx*,NCX_CACHED_TYPE_FIRST_EXTENSION> first_extension_;

  //! The namespace
  const char* ns_;
};


/*!
 * Yang type descriptor for anyxml for libncx.  This class exists
 * because anyxml is treated as leafy by the rift yang model, but
 * libncx does not treat anyxml as similarly leafy.  There is only one
 * of these objects, owned by the model.  It is not stored in
 * ncxmodel_.types_.
 */
class YangTypeNcxAnyxml
: public YangType
{
public:
  YangTypeNcxAnyxml(YangModelNcxImpl& ncxmodel)
  : ncxmodel_(ncxmodel)
  {
    /* ATTN */
  }

  ~YangTypeNcxAnyxml() {}

  // Cannot copy
  YangTypeNcxAnyxml(const YangTypeNcxAnyxml&) = delete;
  YangTypeNcxAnyxml& operator=(const YangTypeNcxAnyxml&) = delete;

public:
  std::string get_location() { return "<internal anyxml>"; }
  YangModule* get_module() { return nullptr; /* ATTN */ }
  const char* get_prefix() { return ""; /* ATTN */ }
  const char* get_ns() { return ""; /* ATTN */ }
  rw_yang_leaf_type_t get_leaf_type() { return RW_YANG_LEAF_TYPE_ANYXML; }
  YangValue* get_first_value() { return nullptr; /* ATTN */ }
  rw_status_t parse_value(const char* value, YangValue** matched)
    { (void)value; (void)matched; return RW_STATUS_FAILURE; /* ATTN */ }

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;
};


/*!
 * Yang model node for libncx.  This class describes a single node in
 * the YANG schema model.
 */
class YangNodeNcx
: public YangNode
{
 public:
  /*!
   * Construct a non-top-level yang model node for libncx.
   */
  YangNodeNcx(
    YangModelNcxImpl& ncxmodel, //!< [in] The owning model
    YangNode* parent, //!< [in] The parent node.  May be nullptr for root.
    obj_template_t* obj //!< [in] The libncx object definition
  );

  /*!
   * Construct a top-level yang model node for libncx.
   */
  YangNodeNcx(
    YangModelNcxImpl& ncxmodel, //!< [in] The owning model
    YangModuleNcx* module_top, //!< [in] The owning module
    obj_template_t* obj //!< [in] The libncx object definition
  );

 private:
  /*!
   * Delegated constructor.
   */
  YangNodeNcx(
    YangModelNcxImpl& ncxmodel, //!< [in] The owning model
    YangNode* parent, //!< [in] The parent node.  May be nullptr for root.
    YangModuleNcx* module_top, //!< [in] The owning module.  May be nullptr for non-top
    obj_template_t* obj //!< [in] The libncx object definition
  );

 public:
  //! Destroy a yang model node for libncx.
  ~YangNodeNcx();

  // Cannot copy
  YangNodeNcx(const YangNodeNcx&) = delete;
  YangNodeNcx& operator=(const YangNodeNcx&) = delete;

 public:

  std::string get_location() override;
  const char* get_description() override;
  const char* get_help_short() override;
  const char* get_help_full() override;
  const char* get_name() override;
  const char* get_prefix() override;
  const char* get_ns() override;
  const char* get_default_value() override;
  uint32_t get_max_elements() override;
  rw_yang_stmt_type_t get_stmt_type() override;
  bool is_config() override;
  bool is_key() override;
  bool is_presence() override;
  bool is_mandatory() override;
  bool has_default() override;
  YangNode* get_parent() override;
  YangNodeNcx* get_first_child() override;
  YangNodeNcx* get_next_sibling() override;
  YangType* get_type() override;
  YangValue* get_first_value() override;
  YangKey* get_first_key() override;
  YangExtNcx* get_first_extension() override;
  YangModule* get_module() override;
  YangModule* get_module_orig() override;
  YangAugment* get_augment() override;
  YangUses* get_uses() override;
  YangNode* get_reusable_grouping() override;
  YangNodeNcx* get_case() override;
  YangNodeNcx* get_choice() override;
  YangNodeNcx* get_default_case() override;
  virtual bool is_conflicting_node(YangNode *other) override;
  const char* get_pbc_field_name() override;
  void set_mode_path() override;
  bool is_mode_path() override;
  bool is_rpc() override;
  bool is_rpc_input() override;
  bool is_rpc_output() override;
  bool is_notification() override;
  std::string get_enum_string(uint32_t value) override;
  YangNode* get_leafref_ref() override;
  std::string get_leafref_path_str() override;
  bool app_data_is_cached(const AppDataTokenBase* token) const noexcept override;
  bool app_data_is_looked_up(const AppDataTokenBase* token) const noexcept override;
  bool app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept override;
  bool app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data) override;
  void app_data_set_looked_up(const AppDataTokenBase* token) override;
  intptr_t app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data) override;
  intptr_t app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data) override;
  virtual YangModel* get_model() override;
  virtual uint32_t get_node_tag() override;

 protected:
  /*!
   * Update the node pointers that may have changed due to a version
   * change.  Only update those nodes that have already been cached.
   */
  virtual void locked_update_version();

  /*!
   * Update the model node for the first deep child.  ASSUMES THE MUTEX
   * IS LOCKED.
   */
  virtual YangNodeNcx* locked_update_first_child();

  /*!
   * Update the model node for the next sibling node.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual YangNodeNcx* locked_update_next_sibling();

  /*!
   * Update the model node for the fnext sibling node; top-level-node
   * version.  ASSUMES THE MUTEX IS LOCKED.
   */
  virtual YangNodeNcx* locked_update_next_sibling_top();

  /*!
   * Update the model node for the first list key.  ASSUMES THE MUTEX
   * IS LOCKED.
   */
  virtual YangKeyNcx* locked_update_first_key();

  /*!
   * Update the model node for the first list extension.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual YangExtNcx* locked_update_first_extension();

  /*!
   * Update the uses for the node for the first time.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual YangUsesNcx* locked_update_uses();

  /*!
   * Update the augment for the node for the first time.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual YangAugmentNcx* locked_update_augment();


 public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! Increments by one for each complete load
  NcxVersion version_;

  //! The libncx node information.
  obj_template_t* const obj_ = nullptr;

  //! The module the node most immediately belongs to.
  YangModuleNcx* module_ = nullptr;

   //! The module the node was originally defined in: grouping, augment, or module_.
  YangModuleNcx* module_orig_ = nullptr;

  //! (Module top-level nodes only) the module.  ATTN: Same as module_?
  YangModuleNcx* module_top_ = nullptr;

  //! The node description.  nullptr util cached.  Owned by libncx.
  const char* description_ = nullptr;

  //! The value short help.  Derived from description.
  //! Protected by NCX_CACHED_NODE_HELP_SHORT.
  std::string help_short_;

  //! The node name.  nullptr util cached.  Owned by libncx.
  const char* name_ = nullptr;

  //! The namespace prefix used (if any) in the defining yang file.
  //! Possibly empty.  nullptr util cached.  Owned by libncx.
  const char* prefix_ = nullptr;

  //! The true, complete, namespace of node.  nullptr util cached.
  //! Owned by libncx.
  const char* ns_ = nullptr;

  //! Cached statement type.  Assumed to not change, even across versions.
  rw_yang_stmt_type_t stmt_type_ = RW_YANG_STMT_TYPE_NULL;

  //! Is-a-key indication.  Always valid.
  bool is_key_ = false;

  //! Is-config indication.  Always valid.
  bool is_config_ = false;

  //! Is-presence-container indication.  Always valid.
  bool is_presence_ = false;

  //! Is this node in the path to a mode? ATTN: SHOULD BE APP DATA
  bool is_mode_path_ = false;

  //! Is this node mandatory? Cached from ncx
  bool is_mandatory_ = false;

  //! Is this node the input node to an RPC?  ATTN: Should have owenership category
  bool is_rpc_input_ = false;

  //! Is this node the output node to an RPC?  ATTN: Should have ownership category
  bool is_rpc_output_ = false;

  //! Is this node a descendant of a notification node?  ATTN: Should have owenership category
  bool is_notification_ = false;

  //! Parent node.  nullptr is not possible.
  YangNode* parent_ = nullptr;

  //! Parent case/choice node.  nullptr is possible.
  YangNodeNcx* choice_case_parent_ = nullptr;

  //! Next sibling node.  nullptr means last.
  NcxCached<YangNodeNcx*,NCX_CACHED_NODE_NEXT_SIBLING,ncx_cached_write_many> next_sibling_;

  //! First child node.  nullptr means leaf.
  NcxCached<YangNodeNcx*,NCX_CACHED_NODE_FIRST_CHILD,ncx_cached_write_many> first_child_;

  //! Type definition for leafy types.  Always valid.  Owned by model.
  YangType* type_ = nullptr;

  //! First key, if a list node, if any keys.  nullptr if no keys or not list.
  NcxCached<YangKeyNcx*,NCX_CACHED_NODE_FIRST_KEY> first_key_;

  //! First extension child.  nullptr if no extensions.
  NcxCached<YangExtNcx*,NCX_CACHED_NODE_FIRST_EXTENSION> first_extension_;

  //! uses defined.  nullptr if not from a uses
  NcxCached<YangUsesNcx*,NCX_CACHED_NODE_USES> uses_;

  /// Does the node has desendant node that has a default value
  NcxCached<bool, NCX_CACHED_NODE_HAS_DEFAULT> has_default_;

  //! Protobuf-c field name.  Derived from name or the
  //! RW_YANG_PB_EXT_FLD_NAME extension.  Protected by
  //! NCX_CACHED_NODE_PB_FIELD_NAME.
  std::string pbc_field_name_;

  //! Cached application data.
  AppDataCache app_data_;

  //! Unique tag number
  uint32_t node_tag_ = 0;

  //! The augment descriptor, if any.
  NcxCached<YangAugmentNcx*,NCX_CACHED_NODE_AUGMENT> augment_;

  //! The leafref target node, if any.
  NcxCached<YangNodeNcx*,NCX_CACHED_NODE_LEAFREF_REF> leafref_ref_;
};


/*!
 * Root yang model node for libncx.  This node represents all modules.
 * Its children are all the children of all the modules.  It has no
 * namespace, no parent, and no siblings.  It always exists.
 */
class YangNodeNcxRoot
: public YangNode
{
public:
  YangNodeNcxRoot(YangModelNcxImpl& ncxmodel);

  //! Destroy a libncx root model node.
  ~YangNodeNcxRoot() {}

  // Cannot copy
  YangNodeNcxRoot(const YangNodeNcxRoot&) = delete;
  YangNodeNcxRoot& operator=(const YangNodeNcxRoot&) = delete;

public:
  std::string get_location() { return "internal-root"; }
  const char* get_description() { return "Root node"; }
  const char* get_name() { return "data"; }
  const char* get_prefix() { return ""; }
  const char* get_ns() { return ""; }
  rw_yang_stmt_type_t get_stmt_type() { return RW_YANG_STMT_TYPE_ROOT; }
  bool is_config() { return false; }
  bool is_root() { return true;}
  YangNode* get_parent() { return nullptr; }
  YangNodeNcx* get_first_child();
  YangNode* get_next_sibling() { return nullptr; }
  YangKey* get_first_key() { return nullptr; }
  YangModule* get_module() { return nullptr; }
  YangUses* get_uses() { return nullptr; }
  YangModel* get_model();
  uint32_t get_node_tag() {RW_CRASH(); return 0; } /* ATTN */

  //! @see YangNode::app_data_is_cached(const AppDataTokenBase* token)
  bool app_data_is_cached(const AppDataTokenBase* token) const noexcept override;

  //! @see YangNode::app_data_is_looked_up(const AppDataTokenBase* token)
  bool app_data_is_looked_up(const AppDataTokenBase* token) const noexcept override;

  //! @see YangNode::app_data_check_and_get(AppDataTokenBase* token, intptr_t* data)
  bool app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept override;

  //! @see YangNode::app_data_check_lookup_and_get(AppDataTokenBase* token, intptr_t* data)
  bool app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data) override;

  //! @see YangNode::app_data_set_looked_up(AppDataTokenBase* token)
  void app_data_set_looked_up(const AppDataTokenBase* token) override;

  //! @see YangNode::app_data_set_and_give_ownership(AppDataTokenBase* token, intptr_t data)
  intptr_t app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data);

  //! @see YangNode::app_data_set_and_keep_ownership(AppDataTokenBase* token, intptr_t data)
  intptr_t app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data);
protected:
  virtual void locked_update_version();
  virtual YangNodeNcx* locked_update_first_child();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! Increments by one for each complete load
  NcxVersion version_;

  //! The first known child
  NcxCached<YangNodeNcx*,NCX_CACHED_NODE_FIRST_CHILD,ncx_cached_write_many> first_child_;

  //! Cached application data.
  AppDataCache app_data_;
};


/*!
 * Yang key descriptor for libncx.  This descriptor defines the owning
 * list and the ultimate leaf of one key of the list.
 */
class YangKeyNcx
: public YangKey
{
public:
  YangKeyNcx(YangModelNcxImpl& ncxmodel, YangNodeNcx* list_node, YangNodeNcx* key_node, obj_key_t* key);
  ~YangKeyNcx() {}

  // Cannot copy
  YangKeyNcx(const YangKeyNcx&) = delete;
  YangKeyNcx& operator=(const YangKeyNcx&) = delete;

public:
  YangNodeNcx* get_list_node();
  YangNodeNcx* get_key_node();
  YangKeyNcx* get_next_key();

protected:
  virtual YangKeyNcx* locked_update_next_key();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx key descriptor.
  obj_key_t* key_;

  //! The owning list node.
  YangNodeNcx* list_node_;

  //! The ultimate key node.
  YangNodeNcx* key_node_;

  //! The next key in the list of keys.
  NcxCached<YangKeyNcx*,NCX_CACHED_KEY_NEXT_KEY> next_key_;
};


/*!
 * Yang augment descriptor for libncx.  This descriptor defines the
 * owning module and the ultimate augmented node (likely in another
 * module).
 */
class YangAugmentNcx
: public YangAugment
{
public:
  /*!
   * Construct augment descriptor for a single libncx module augment
   * entry.
   */
  YangAugmentNcx(
    YangModelNcxImpl& ncxmodel,
    /*!
     * The module that defined the augment.
     */
    YangModuleNcx* module,

    /*!
     *  The (deep) augmented node.  MIGHT NOT BE THE ACTUAL AUGMENT
     *  TARGET!  If the actual augment target is a choice or a case,
     *  all the children of the target will be considered augmented in
     *  of themselves.  We do it that way because most yang model
     *  traversals do not visit choice or case nodes.
     */
    YangNodeNcx* node,

    /*!
     * The libncx augment object.  This may map to a choice or a case.
     */
    obj_template_t* obj_augment
  );
  ~YangAugmentNcx() {}

  // Cannot copy
  YangAugmentNcx(const YangAugmentNcx&) = delete;
  YangAugmentNcx& operator=(const YangAugmentNcx&) = delete;

public:
  std::string get_location() override;
  YangModule* get_module() override;
  YangNodeNcx* get_target_node() override;
  YangAugmentNcx* get_next_augment() override;
  YangExtNcx* get_first_extension() override;

protected:
  /*!
   * Update the augment descriptor for the next augment.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual YangAugmentNcx* locked_update_next_augment();

  /*!
   * Update the first extension.  ASSUMES THE MUTEX IS LOCKED.
   */
  virtual YangExtNcx* locked_update_first_extension();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx augment descriptor.
  obj_template_t* obj_augment_ = nullptr;

  //! The owning module (where the augment occurs).
  YangModuleNcx* module_ = nullptr;

  //! The effective augmented node (likely in another module).  MAY BE choice/case!
  YangNodeNcx* target_node_ = nullptr;

  //! The next augment in the list of augments.
  NcxCached<YangAugmentNcx*,NCX_CACHED_AUGMENT_NEXT_AUGMENT> next_augment_;

  //! First extension child.  nullptr if no extensions.
  NcxCached<YangExtNcx*,NCX_CACHED_AUGMENT_FIRST_EXTENSION> first_extension_;
};


/*!
 * Yang module for libncx.  This class describes a single module in the
 * yang schema model, whether loaded explicitly or not.
 */
class YangModuleNcx
: public YangModule
{
public:
  YangModuleNcx(YangModelNcxImpl& ncxmodel, ncx_module_t* mod, std::string load_name);
  YangModuleNcx(YangModelNcxImpl& ncxmodel, ncx_module_t* mod);
  ~YangModuleNcx() {}

  // Cannot copy
  YangModuleNcx(const YangModuleNcx&) = delete;
  YangModuleNcx& operator=(const YangModuleNcx&) = delete;

public:

  std::string get_location() override;
  const char* get_description() override;
  const char* get_name() override;
  const char* get_prefix() override;
  const char* get_ns() override;
  YangModuleNcx* get_next_module() override;
  bool was_loaded_explicitly() override;
  YangNodeNcx* get_first_node() override;
  YangNodeIter node_end() override;
  YangExtNcx* get_first_extension() override;
  YangAugmentNcx* get_first_augment() override;
  virtual YangTypedEnum *get_first_typed_enum() override;
  YangNode* get_first_grouping() override;

  YangModule** get_first_import() override;
  void mark_imports_explicit() override;
  YangModel* get_model() override;

  bool app_data_is_cached(const AppDataTokenBase* token) const noexcept override;
  bool app_data_is_looked_up(const AppDataTokenBase* token) const noexcept override;
  bool app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept override;
  bool app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data) override;
  void app_data_set_looked_up(const AppDataTokenBase* token) override;
  intptr_t app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data) override;
  intptr_t app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data) override;

public:
  /*!
   * Update the node pointers that may have changed due to a version
   * change.  Only update those nodes that have already been cached.
   */
  virtual void locked_update_version();

  /*!
   * Set the module loaded name.  This gets called after the module was
   * created due to an import when the module gets laded explicitly.
   * ASSUMES THE MUTEX IS LOCKED.
   */
  virtual void locked_set_load_name(const std::string& load_name);

  /*!
   * Change the last top-level node of the module.  This is not
   * necessarily the real last node, and it can change later due to
   * augments.  This gets called during the construction of a new top
   * node in the module.  ASSUMES THE MUTEX IS LOCKED.
   */
  virtual void locked_set_last_node(YangNodeNcx* node);

  /*!
   * Change the next module pointer.  This gets called when the model
   * is updating the list of loaded modules (both implicitly and
   * explicitly), after explicitly loading one module.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual void locked_set_next_module(YangModuleNcx* next_module);

  /*!
   * Update the first top-level child of this module.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual YangNodeNcx* locked_update_first_node();

  /*!
   * Update the first extension applied to this module (as opposed to
   * the extensions DEFINED by this module).  ASSUMES THE MUTEX IS
   * LOCKED.
   */
  virtual YangExtNcx* locked_update_first_extension();

  /*!
   * Update the first augment applied to this module (as opposed to the
   * augments DEFINED by this module).  ASSUMES THE MUTEX IS LOCKED.
   */
  virtual YangAugmentNcx* locked_update_first_augment();

  /*!
   * Update the first typedef.  Check the module typedef Q to see if
   * there is any ASSUMES THE MUTEX IS LOCKED.
   */
  virtual YangTypedEnumNcx *locked_update_first_typedef();

  /*!
   * Update the first import.  Check the module import Q to see if
   * there is any ASSUMES THE MUTEX IS LOCKED.
   */
  virtual YangModule** locked_update_first_import();

  /*!
   * Update the model node for the first grouping.  ASSUMES THE MUTEX
   * IS LOCKED.
   */
  virtual YangGroupingNcx *locked_update_first_grouping();

  /*!
   * Change the last top-level grouping of the module.  ASSUMES THE
   * MUTEX IS LOCKED.
   */
  virtual void locked_set_last_grouping(YangGroupingNcx* grp);


public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! Increments by one for each complete load
  NcxVersion version_;

  //! The module
  ncx_module_t* mod_;

  //! The name used to load the module.  May be populated after the
  //! fact if the module is loaded explicitly after being loaded
  //! implicitly.  Protected by NCX_CACHED_MODULE_WAS_LOADED.
  std::string load_name_;

  //! The first known node
  NcxCached<YangNodeNcx*,NCX_CACHED_MODULE_FIRST_NODE,ncx_cached_write_many> first_node_;

  //! The last known node
  NcxCached<YangNodeNcx*,NCX_CACHED_MODULE_LAST_NODE,ncx_cached_write_many> last_node_;

  //! Next module.  nullptr until set in populate(), which is done in the
  //! same critical section that created the module.  May be nullptr, even
  //! after being set, for the last module.
  //! ATTN: Note about cachedness.
  std::atomic<YangModuleNcx*> next_module_;

  //! First extension child.  May be nullptr if no extensions.
  NcxCached<YangExtNcx*,NCX_CACHED_MODULE_FIRST_EXTENSION> first_extension_;

  //! First augment child.  May be nullptr if no augments.
  NcxCached<YangAugmentNcx*,NCX_CACHED_MODULE_FIRST_AUGMENT> first_augment_;

    //! The first typed enum
  NcxCached<YangTypedEnumNcx*,NCX_CACHED_MODULE_FIRST_TYPEDEF> first_typedef_;

 //! The list of imports
  NcxCached<YangModule**,NCX_CACHED_MODULE_FIRST_IMPORT> imported_modules_;

 //! The list of imports
  unsigned int import_count_;

  //! First top level grouping defined.  nullptr if no groupings
  NcxCached<YangGroupingNcx*,NCX_CACHED_MODULE_FIRST_GROUPING> first_grouping_;

  //! The last know grouping
  NcxCached<YangGroupingNcx*,NCX_CACHED_MODULE_LAST_GROUPING, ncx_cached_write_many> last_grouping_;

  //! Cached application data.
  AppDataCache app_data_;
};


/*!
 * Extension information associated with flags
 */
typedef struct _rw_yang_extension_element {
  xmlChar    *module_name;     //! module that this extension was originally defined in
  xmlChar    *extension_name;  //! name of the extension
  ext_template_t *extension;   //! cached extension. nullptr till resolved
}rw_yang_extension_element_t;


/*!
 * Yang extension descriptor for libncx.  This class describes a single
 * extension associated with another object in the YANG schema model.
 * These descriptors are owned by the model.
 */
class YangTypedEnumNcx
: public YangTypedEnum
{
public:
  YangTypedEnumNcx(YangModelNcxImpl& model, typ_template_t *typ);
  ~YangTypedEnumNcx();

  // Cannot copy
  YangTypedEnumNcx(const YangTypedEnumNcx&) = delete;
  YangTypedEnumNcx& operator=(const YangTypedEnumNcx&) = delete;

public:
  const char* get_description();
  const char* get_name();
  std::string get_location();

  const char* get_ns();
  YangTypedEnumNcx* get_next_typed_enum();
  YangValue* get_first_enum_value();
  YangExtNcx* get_first_extension();

public:
  virtual YangTypedEnumNcx* locked_update_next_typedef();
  virtual YangValue *locked_update_first_enum_value();
  virtual YangExtNcx *locked_update_first_extension();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx type data
  typ_template_t* const typedef_;

  //! The true, complete, namespace of the typedef definition.  Owned
  //! by libncx.
  const char* ns_;

  //! name cached from ncx. Owned by libncx
  const char* name_;

  //! The typedef description.  nullptr util cached.  Owned by libncx.
  const char* description_;

  //! The next typedef in the list.  Initialization delayed until
  //! first use.
  NcxCached<YangTypedEnumNcx*,NCX_CACHED_TYPEDEF_NEXT_TYPEDEF> next_typed_enum_;

  //! First value in enum.  nullptr if no known values (Is this possible?)
  NcxCached<YangValue*,NCX_CACHED_TYPEDEF_FIRST_VALUE> first_enum_value_;

  //! First extension child.  nullptr if no extensions.
  NcxCached<YangExtNcx*, NCX_CACHED_TYPEDEF_FIRST_EXTENSION> first_extension_;
};


/*!
 * Yang model implementation based on Yuma libncx.
 *
 * Most accesses to the model are assumed to be thread safe.  The only
 * thread-unsafe actions are model updates to the model itself.
 */
class YangModelNcxImpl
: public YangModelNcx
{
 public:
  /*!
   * Create a libncx-based yang model.
   */
  YangModelNcxImpl(
    //! Trace context.  ATTN: Should not be void - should at least be struct tag qualified!
    void *trace_ctx = nullptr
  );

  /*!
   * Destroy a libncx-based yang model.  If possible, will destroy all
   * the libncx tracking data structures.
   */
  ~YangModelNcxImpl();

  // Cannot copy
  YangModelNcxImpl(const YangModelNcxImpl&) = delete;
  YangModelNcxImpl& operator=(const YangModelNcxImpl&) = delete;


 public:
  //! Counter for deciding when to destroy libncx
  static unsigned g_users;

  //! Model version number
  std::atomic<unsigned> version_ = {};

  //! The libncx instance pointer.
  ncx_instance_t* ncx_instance_;

 public:
  typedef std::unique_ptr<YangModuleNcx> module_ptr_t;
  typedef std::vector<module_ptr_t> modules_t;
  typedef std::unordered_map<ncx_module_t*,YangModuleNcx*> module_map_t;

  //! The first known module.  ATTN: Needs cache!!
  YangModuleNcx* first_module_;

  //! The list of loaded modules.
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  modules_t modules_;

  //! A map of loaded modules for fast lookup by libncx handle.
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  module_map_t modules_map_ncx_;

  YangModuleNcx* load_module(const char* module_name) override;
  YangModuleNcx* get_first_module() override;

  /*!
   * Search for a module by libncx pointer.  ASSUMES THE MUTEX IS
   * LOCKED.
   */
  YangModuleNcx* locked_search_module(ncx_module_t* mod);

  /*!
   * Search for a module by name.  ASSUMES THE MUTEX IS LOCKED.  THIS
   * IS A LINEAR SEARCH!  If performance of this function is important,
   * we should reimplement the lookup in YangModleNcxImpl to build a
   * hash on module name (and revision).
   */
  YangModuleNcx* locked_search_module(const char* module_name);

  /*!
   * Create a new Yang model module, for a module that was loaded
   * explicitly.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangModuleNcx* locked_new_module(ncx_module_t* mod, const char* load_name);

  /*!
   * Create a new Yang model module, for a module that was loaded
   * explicitly.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangModuleNcx* locked_new_module(ncx_module_t* mod);

  /*!
   * Save a newly created module.  ASSUMES THE MUTEX IS LOCKED.
   */
  void locked_save_module(ncx_module_t* mod, YangModuleNcx* module);

  /*!
   * Populate the complete module list.  This function must be run
   * every time a module load is attempted, whether or not the load
   * succeeds (because even a failed load may cause imports to
   * succeed).  This function populates all the modules the model
   * didn't already know about, so that they can be referenced by the
   * module during lookups.
   *
   * ASSUMES THE MUTEX IS LOCKED.
   *
   * THIS O(n) IN THE NUMBER OF MODULES!  If performance of this
   * function becomes important, we should reimplement to be more
   * efficient, perhaps by obtaining callbacks from libncx.
   */
  void locked_populate_import_modules();


 public:
  typedef std::unique_ptr<YangNodeNcx> node_ptr_t;
  typedef std::unordered_map<obj_template_t*,node_ptr_t> nodes_t;

  //! The root node.
  YangNodeNcxRoot root_node_;

  //! The list of owned cases (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  nodes_t cases_;

  //! The list of owned choices (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  nodes_t choices_;

  //! The list of owned nodes (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  nodes_t nodes_;

  YangNodeNcxRoot* get_root_node() override;

  /*!
   * Create a top schema node and add it to the list of model-owned
   * nodes, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangNodeNcx* locked_new_node(YangNode* parent, obj_template_t* obj);

  /*!
   * Create a regular schema node and add it to the list of model-owned
   * nodes, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangNodeNcx* locked_new_node(YangModuleNcx* module, obj_template_t* obj);

  /*!
   * Search for a specified libncx node.  This can be used for cases
   * where you have a libncx node and don't know which YangNode it maps
   * to.  ASSUMES THE MUTEX IS LOCKED.
   *
   * @return The node, or nullptr if the node is not found.  Will NOT
   *   return choice or case nodes.
   */
  YangNodeNcx* locked_search_node(
    //! [in] The node to search for
    obj_template_t* obj
  );

  /*!
   * Search for a specified libncx node, including choice or case
   * nodes.  This can be used for cases where you have a libncx node
   * and don't know which YangNode it maps to.  ASSUMES THE MUTEX IS
   * LOCKED.
   *
   * @return The node, or nullptr if the node is not found.
   */
  YangNodeNcx* locked_search_node_case_choice(
    //! [in] The node to search for
    obj_template_t* obj
  );

  /*!
   * Given a libncx augment or leafref target node, attempt to find
   * and/or populate the proper, module-rooted, node tree that leads to
   * the target node.  ASSUMES THE MUTEX IS LOCKED.
   *
   * @return The parent node, if able to build the tree.  May be choice
   *   or case node if the target is an augment!
   */
  YangNodeNcx* locked_populate_target_node(
    //! [in] The target libncx node. May be choice/case.
    obj_template_t* targobj
  );


 public:
  /*!
   * Create a case schema node and add it to the list of model-owned
   * cases, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangNodeNcx* locked_new_choice(
    //! [in] The XML parent (not the enclosing case, if any).  nullptr if top-level.
    YangNode* xml_parent,
    //! [in] The owning module (needed if top-level).
    YangModuleNcx* module,
    //! [in] ncx library object for this choice
    obj_template_t* obj
  );

  /*!
   * Search for a specified libncx choice.  ASSUMES THE MUTEX IS LOCKED.
   *
   * @return The node, or nullptr if the node is not found.
   */
  YangNodeNcx* locked_search_choice(obj_template_t* obj);

  /*!
   * Create a case schema node and add it to the list of model-owned
   * cases, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangNodeNcx* locked_new_case(
    //! [in] The XML parent (not the choice).  nullptr if top-level.
    YangNode* xml_parent,
    //! [in] The owning module (needed if top-level).
    YangModuleNcx* module,
    //! [in] ncx library object for this case
    obj_template_t* obj
  );

  /*!
   * Search for a specified libncx case.  ASSUMES THE MUTEX IS LOCKED.
   *
   * @return The node, or nullptr if the node is not found.
   */
  YangNodeNcx* locked_search_case(obj_template_t* obj);


 public:
  typedef YangTypeNcx::ptr_t type_ptr_t;
  typedef YangTypeNcx::map_t type_map_t;

  //! The list of owned type descriptors (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  type_map_t types_;

  //! Shared object instance for anyxml "type".
  YangTypeNcxAnyxml type_anyxml_;

  /*!
   * Get/Create a type descriptor.  If created, add it to the list of
   * model-owned type descriptors, for future cleanup.  ASSUMES THE MUTEX
   * IS LOCKED.
   */
  YangType* locked_get_type(obj_template_t* obj);


 public:
  typedef std::unique_ptr<YangTypedEnumNcx> typed_enum_ptr_t;
  typedef std::vector<typed_enum_ptr_t> typed_enums_t;
  typedef std::unordered_map<typ_template_t*,YangTypedEnumNcx*> typed_enums_map_t;

  //! A list of typed enums. ACCESS MUST BE PROTECTED BY MUTEX
  typed_enums_t typed_enums_;

  //! A map of typed enums. ACCESS MUST BE PROTECTED BY MUTEX
  typed_enums_map_t typed_enums_map_;

  /*!
   * Search for a named enumeration by ncx ptr. ASSUMES MUTEX IS LOCKED
   */
  YangTypedEnumNcx* locked_search_typed_enum(typ_template_t* enum_typedef);

  /*!
   * Create a typedef enum descriptor, owned by the model.
   */
  YangTypedEnumNcx* locked_new_typed_enum(typ_template_t* enum_typedef);


 public:
  typedef std::unique_ptr<YangValue> value_ptr_t;
  typedef std::vector<value_ptr_t> values_t;

  //! The list of owned value descriptors (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  values_t values_;

  /*!
   * Create a value descriptor and add it to the list of model-owned
   * values, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangValueNcx* locked_new_value(
    YangTypeNcx* owning_type,
    typ_def_t* typ
  );

  /*!
   * Create a boolean value and add it to the list of model-owned values for future cleanup.
   *
   * ASSUMES THE MUTEX IS LOCKED.
   */
  YangValueNcxBool* locked_new_value_boolean(
    YangTypeNcx* owning_type,
    typ_def_t* typ,
    bool val
  );

  /*!
   * Create an enum value and add it to the list of model-owned
   * values, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangValueNcxEnum* locked_new_value_enum(
    YangTypeNcx* owning_type,
    typ_def_t* typ,
    typ_enum_t* typ_enum
  );

  /*!
   * Create an enum value and add it to the list of model-owned
   * values, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangValueNcxIdRef* locked_new_value_idref(
    YangTypeNcx* owning_type,
    typ_def_t* typ,
    ncx_identity_t_* typ_idref
  );

  /*!
   * Create a bits value and add it to the list of model-owned
   * values, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangValueNcxBits* locked_new_value_bits(
    YangTypeNcx* owning_type,
    typ_def_t* typ,
    typ_enum_t* typ_enum
  );


 public:
  typedef std::unique_ptr<YangKeyNcx> key_ptr_t;
  typedef std::vector<key_ptr_t> keys_t;

  //! The list of owned key descriptors (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  keys_t keys_;

  /*!
   * Create a key descriptor and add it to the list of model-owned keys,
   * for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangKeyNcx* locked_new_key(
    YangNodeNcx* list_node,
    YangNodeNcx* key_node,
    obj_key_t* key
  );


 public:
  typedef std::unique_ptr<YangExtNcx> extension_ptr_t;
  typedef std::vector<extension_ptr_t> extensions_t;

  //! The list of owned extension descriptors (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  extensions_t extensions_;

  /*!
   * Create an extension descriptor and add it to the list of model-owned
   * extensions, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangExtNcx* locked_new_ext(
    ncx_appinfo_t* appinfo
  );

  /*!
   * Create an extension descriptor and add it to the list of model-owned
   * extensions, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangExtNcxNode* locked_new_ext(
    ncx_appinfo_t* appinfo,
    obj_template_t* obj
  );

  /*!
   * Create an extension descriptor and add it to the list of model-owned
   * extensions, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangExtNcxGrouping* locked_new_ext(
    ncx_appinfo_t* appinfo,
    grp_template_t* grp
  );

  /*!
   * Create an extension descriptor and add it to the list of model-owned
   * extensions, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangExtNcxType* locked_new_ext(
    ncx_appinfo_t* appinfo,
    typ_def_t* typ
  );


 public:
  typedef std::unique_ptr<YangAugmentNcx> augment_ptr_t;
  typedef std::map<obj_template_t*,augment_ptr_t> augments_t;

  //! The list of owned augment descriptors (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  augments_t augments_;

  /*!
   * Search for an existing augment.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangAugmentNcx* locked_search_augment(
    obj_template_t* obj_augment
  );

  /*!
   * Create an augment descriptor and add it to the list of model-owned
   * augments, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangAugmentNcx* locked_new_augment(
    YangModuleNcx* module,
    YangNodeNcx* node,
    obj_template_t* obj_augment
  );


 public:
  typedef std::unique_ptr<YangGroupingNcx> grp_ptr_t;
  typedef std::unordered_map<grp_template_t*,grp_ptr_t> groupings_t;

  //! The list of owned groupings (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  groupings_t groupings_;

  /*!
   * Search for a specified libncx grouping.  This can be used for cases
   * where you have a libncx node and don't know which YangNode it maps
   * to.  ASSUMES THE MUTEX IS LOCKED.
   *
   * @return The node, or nullptr if the node is not found.
   */
  YangGroupingNcx* locked_search_grouping(
    grp_template_t* grp
  );

  /*!
   * Create a top grouping and add it to the list of model-owned
   * groupings, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangGroupingNcx* locked_new_grouping(
    YangModuleNcx* module,
    grp_template_t* grp
  );


 public:
  typedef std::unique_ptr<YangUsesNcx> uses_ptr_t;
  typedef std::unordered_map<obj_template_t*,uses_ptr_t> uses_t;

  //! The list of owned uses (auto-freed upon destroy).
  //! ACCESSES MUST BE PROTECTED BY MUTEX.
  uses_t uses_;

  /*!
   * Search for a uses by libncx pointer.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangUsesNcx* locked_search_uses(
    obj_template_t* uses
  );

  /*!
   * Create a new uses object and add it to the list of model-owned
   * uses, for future cleanup.  ASSUMES THE MUTEX IS LOCKED.
   */
  YangUsesNcx* locked_new_uses(
    YangNode* node,
    obj_template_t* uses
  );


 public:
  typedef AppDataTokenBase app_data_value_t;
  typedef AppDataTokenBase::deleter_f app_data_deleter_t;

  // Generic AppDataManager Class for registering AppData tokens.
  AppDataManager app_data_mgr_;

  bool app_data_get_token(
    const char* ns,
    const char* ext,
    app_data_deleter_t deleter,
    app_data_value_t* token) override;


 public:
  void set_log_level(
    rw_yang_log_level_t level
  ) override;

  rw_yang_log_level_t log_level() const override;
};


/*!
 * Yang model grouping for libncx.  This class describes a single top level grouping in
 * the YANG schema model.
 */
class YangGroupingNcx
: public YangNode
{
 public:
  YangGroupingNcx(YangModelNcxImpl& ncxmodel, YangModuleNcx* module, grp_template_t* grp);
  /*! Destructor for grouping object */
  ~YangGroupingNcx() {}

  // Cannot copy
  YangGroupingNcx(const YangGroupingNcx&) = delete;
  YangGroupingNcx& operator=(const YangGroupingNcx&) = delete;

 public:

  std::string get_location();
  const char* get_description();
  const char* get_name();
  const char* get_prefix();
  const char* get_ns();
  uint32_t get_node_tag();

  /*! Get the yang statement type.
   * Returns RW_YANG_STMT_TYPE_GROUPING always for groupings
   */
  rw_yang_stmt_type_t get_stmt_type() { return RW_YANG_STMT_TYPE_GROUPING; }

  /*! Returns the groupings parent. Node could be NULL.
   *  Node is owner by the library. The caller may use
   *  node until model is destroyed.
   */
  YangNode* get_parent() { return parent_; }

  YangNodeNcx* get_first_child();

  /*! Does not apply for groupings, return null pointer */
  YangNodeNcx* get_next_sibling() { return nullptr; }

  /*! Get the first key descriptor - Returns nullptr */
  YangKey* get_first_key() { return nullptr; }

  /*! Get the module the grouping belongs to. The module is owned by the
   * model.  Caller may use the module until model is destroyed.
   */
  YangModule* get_module() { return module_; }

  /*!
   * Get the next grouping descriptor in the module.  Descriptor owned by
   * the library.  Caller may use descriptor until model is destroyed.
   */
  virtual YangNode* get_next_grouping();

  YangModel* get_model() {return &ncxmodel_;}

  // ATTN: Should support App Data?

protected:

  virtual YangNodeNcx* locked_update_first_child();
  virtual YangExtNcx* locked_update_first_extension();
  virtual YangNode* locked_update_next_grouping();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx node information.
  grp_template_t* const grp_template_;

  //! The module the node most immediately belongs to.
  YangModuleNcx* module_;

  //! The node description.  nullptr util cached.  Owned by libncx.
  const char* description_;

  //! The node name.  nullptr util cached.  Owned by libncx.
  const char* name_;

  //! The namespace prefix used (if any) in the defining yang file.
  //! Possibly empty.  nullptr util cached.  Owned by libncx.
  const char* prefix_;

  //! The true, complete, namespace of node.  nullptr util cached.
  //! Owned by libncx.
  const char* ns_;

  //! Parent node.  nullptr is not possible.
  YangNode* parent_;

  //! Unique tag number
  uint32_t node_tag_;

  //! First child node.  nullptr means leaf.
  NcxCached<YangNodeNcx*,NCX_CACHED_GROUPING_FIRST_CHILD,ncx_cached_write_many> first_child_;

  //! First extension child.  nullptr if no extensions.
  NcxCached<YangExtNcxGrouping*,NCX_CACHED_GROUPING_FIRST_EXTENSION> first_extension_;

  //! First top level grouping defined.  nullptr if no groupings
  NcxCached<YangGroupingNcx*,NCX_CACHED_GROUPING_NEXT_GROUPING> next_grouping_;

  //! uses defined.  nullptr if not from a uses
  NcxCached<YangUsesNcx*,NCX_CACHED_GROUPING_USES> uses_;
};

/*!
 * Yang uses descriptor for libncx.
 */
class YangUsesNcx
: public YangUses
{
public:
  YangUsesNcx(YangModelNcxImpl& ncxmodel, YangNode* node, obj_template_t* obj);
  ~YangUsesNcx() {}

  // Cannot copy
  YangUsesNcx(const YangUsesNcx&) = delete;
  YangUsesNcx& operator=(const YangUsesNcx&) = delete;

public:

  /*!
   * Get the source file location of the uses statement
   */
  std::string get_location();

  //!   Returns the grouping
  YangGroupingNcx* get_grouping();

  //!  true if the uses augments the grouping
  bool is_augmented();

  //!  true if the uses refines the grouping
  bool is_refined();

public:
  //! The owning model
  YangModelNcxImpl& ncxmodel_;

  //! The cached attribute indications
  NcxCacheState cache_state_;

  //! The libncx uses information.
  const char *name_;

  //! The libncx uses information.
  obj_template_t* const obj_;

  //! The namespace prefix used (if any) in the defining yang file.
  //! Possibly empty.  nullptr util cached.  Owned by libncx.
  const char* prefix_;

  //! whether the uses has refine statements
  bool refined_;

  //! whether the uses has augment statements
  bool augmented_;

  //! The  grouping that this uses uses. Null until cached or if the grouping is not toplevel
  YangGroupingNcx *grouping_;
};

/*!
 * Construct from the model.
 */
inline NcxVersion::NcxVersion(YangModelNcxImpl& ncxmodel)
: version_(ncxmodel.version_.load())
{}

//! Is the version up to date?
inline bool NcxVersion::is_up_to_date(YangModelNcxImpl& ncxmodel)
{
  return version_ == ncxmodel.version_;
}

//! Mark the object as up to date.
inline void NcxVersion::updated(YangModelNcxImpl& ncxmodel)
{
  version_.store(ncxmodel.version_.load());
}


} // namespace rw_yang

#endif // RW_YANGTOOLS_YANGNCX_IMPL_HPP_
