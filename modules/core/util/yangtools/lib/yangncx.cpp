/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file yangncx.cpp
 *
 * Yang model based on libncx.
 */

#include "yangncx_impl.hpp"

#include <rw_yang_mutex.hpp>
#include <yangncx.hpp>
#include <rwtrace.h>

#include <stdio.h>
#include <stdarg.h>
#include <set>
#include <atomic>
#include <thread>
#include <sstream>
#include <list>
using namespace rw_yang;


static void rw_ncx_logging_routine(struct ncx_instance_t_ *instance,
                                   log_debug_t log_level,
                                   const char *fstr, va_list args)
{
  rwtrace_ctx_t *ctx;
  char log_message[1024];

  RW_ASSERT(instance->custom != NULL);

  // ATTN: Need to check if the trace need to be gernerated based on the current level
  // before doing all the work with printing

  ctx = (rwtrace_ctx_t*)instance->custom->logging_context;
  if (ctx == NULL)  {
    return;
  }

  vsnprintf(log_message, sizeof(log_message), fstr, args);

  switch(log_level) {
    case  LOG_DEBUG_OFF:
      break;
    case LOG_DEBUG_ERROR:
      RWTRACE_ERROR(ctx, RWTRACE_CATEGORY_RWTASKLET, "%s", log_message);
      break;
    case LOG_DEBUG_WARN:
      RWTRACE_WARN(ctx, RWTRACE_CATEGORY_RWTASKLET, "%s", log_message);
      break;
    case LOG_DEBUG_INFO:
      RWTRACE_INFO(ctx, RWTRACE_CATEGORY_RWTASKLET, "%s", log_message);
      break;
    case LOG_DEBUG_DEBUG:
    case LOG_DEBUG_DEBUG2:
    case LOG_DEBUG_DEBUG3:
    case LOG_DEBUG_DEBUG4:
      RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_RWTASKLET, "%s", log_message);
      break;
    default:
      /*  Invalid param -- Crash for now */
      RW_ASSERT_NOT_REACHED();
  }
}

/*****************************************************************************/
YangNodeNcx::YangNodeNcx(
  YangModelNcxImpl& ncxmodel,
  YangModuleNcx* module_top,
  obj_template_t* obj)
: YangNodeNcx(ncxmodel, &ncxmodel.root_node_, module_top, obj)
{
}

YangNodeNcx::YangNodeNcx(
  YangModelNcxImpl& ncxmodel,
  YangNode* parent,
  obj_template_t* obj)
: YangNodeNcx(ncxmodel, parent, nullptr, obj)
{
}

YangNodeNcx::YangNodeNcx(
  YangModelNcxImpl& ncxmodel,
  YangNode* parent,
  YangModuleNcx* module_top,
  obj_template_t* obj)
: ncxmodel_(ncxmodel),
  version_(ncxmodel),
  obj_(obj),
  module_top_(module_top),
  parent_(parent),
  app_data_(ncxmodel.app_data_mgr_)
{
  RW_ASSERT(parent_);
  RW_ASSERT(obj_);

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  name_ = (const char*)obj_get_name(ncxmodel_.ncx_instance_, obj_);
  RW_ASSERT(name_);
  RW_ASSERT(name_[0]);

  description_ = (const char*)obj_get_description(ncxmodel_.ncx_instance_, obj_);
  if (nullptr == description_) {
    description_ = "";
  }

#if 0
  // ATTN: This is the WRONG prefix for uses that cross modules!!!
  prefix_ = (const char*)obj_get_mod_prefix(obj_);
  RW_ASSERT(prefix_);
  RW_ASSERT(prefix_[0]);
#endif

  xmlns_id_t nsid = obj_get_nsid(ncxmodel_.ncx_instance_, obj_);
  RW_ASSERT(nsid);
  ns_ = (const char*)xmlns_get_ns_name(ncxmodel_.ncx_instance_, nsid);
  RW_ASSERT(ns_);
  RW_ASSERT(ns_[0]);

  prefix_ = (const char*)xmlns_get_ns_prefix(ncxmodel_.ncx_instance_, nsid);
  RW_ASSERT(prefix_);
  RW_ASSERT(prefix_[0]);

  node_tag_ = obj_get_obj_tag(obj_);

  // Assume statement type cannot change.
  switch (obj_->objtype) {
    case OBJ_TYP_ANYXML:    stmt_type_ = RW_YANG_STMT_TYPE_ANYXML;    break;
    case OBJ_TYP_CONTAINER: {
      stmt_type_ = RW_YANG_STMT_TYPE_CONTAINER;
      if (obj_get_presence_string(ncxmodel_.ncx_instance_, obj_)) {
        is_presence_ = true;
      } else {
        is_presence_ = false;
      }
      break;
    }
    case OBJ_TYP_LEAF: {
      stmt_type_ = RW_YANG_STMT_TYPE_LEAF;
      is_key_ = obj_is_key(obj_);
      break;
    }
    case OBJ_TYP_LEAF_LIST: stmt_type_ = RW_YANG_STMT_TYPE_LEAF_LIST; break;
    case OBJ_TYP_LIST:      stmt_type_ = RW_YANG_STMT_TYPE_LIST;      break;
    case OBJ_TYP_CHOICE:    stmt_type_ = RW_YANG_STMT_TYPE_CHOICE;    break;
    case OBJ_TYP_CASE:      stmt_type_ = RW_YANG_STMT_TYPE_CASE;      break;
    case OBJ_TYP_RPC:       stmt_type_ = RW_YANG_STMT_TYPE_RPC;       break;
    case OBJ_TYP_RPCIO:     stmt_type_ = RW_YANG_STMT_TYPE_RPCIO;     break;
    case OBJ_TYP_NOTIF:     stmt_type_ = RW_YANG_STMT_TYPE_NOTIF;     break;

    case OBJ_TYP_NONE:
    case OBJ_TYP_USES: // ATTN: Needs to become a Node
    case OBJ_TYP_REFINE:
    case OBJ_TYP_AUGMENT: // ATTN: Needs to become a Node
      // an augment should be one of the above
      // a refine should just tweak one of the above
      // a uses should have been replaced by one of the above
      RW_ASSERT_NOT_REACHED();
      break;

    default:
      // bogus value, blow up
      RW_ASSERT_NOT_REACHED();
      break;
  }

  is_mandatory_ = obj_is_mandatory(ncxmodel_.ncx_instance_, obj_);

  if (is_leafy()) {
    type_ = ncxmodel_.locked_get_type(obj_);
  }

  // Lookup my module.
  ncx_module_t* mod = obj_get_mod(ncxmodel_.ncx_instance_, obj_);
  RW_ASSERT(mod);
  module_ = ncxmodel_.locked_search_module(mod);
  RW_ASSERT(module_);

  // Lookup my original module.
  ncx_module_t* orig_mod = obj_->tkerr.mod;
  if (orig_mod != mod) {
    module_orig_ = ncxmodel_.locked_search_module(orig_mod);
    RW_ASSERT(module_orig_);
  } else {
    obj_template_t* uses_obj = obj_->usesobj;
    if (uses_obj) {
      obj_uses_t* uses = obj_->usesobj->def.uses;
      if (uses) {
        grp_template_t* grp = uses->grp;
        if (grp) {
          mod = grp->tkerr.mod;
          module_orig_ = ncxmodel_.locked_search_module(mod);
          // ATTN: What about recursive group-uses?!
        }
      }
    } else {
      // ATTN: augments
    }
  }
  if (nullptr == module_orig_) {
     module_orig_ = module_;
  }

  if (   (RW_YANG_STMT_TYPE_RPC == stmt_type_)
      || (RW_YANG_STMT_TYPE_RPCIO == stmt_type_)) {
    is_config_ = false;
  } else {
    is_config_ = obj_get_config_flag_deep(ncxmodel_.ncx_instance_, obj_) ? true : false;
  }

  is_rpc_input_ = obj_in_rpc(ncxmodel_.ncx_instance_, obj_);
  is_rpc_output_ = obj_in_rpc_reply(ncxmodel_.ncx_instance_, obj_);
  is_notification_ = obj_in_notif(obj_);

  /*
   * Parent nodes cannot be choice or case.  When a node has a choice
   * or case parent, the choice_case_parent_ should be used to walk
   * back up the schema.
   */
  if (parent_) {
    switch (parent_->get_stmt_type()) {
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
        RW_ASSERT_NOT_REACHED();
        break;
      default:
        break;
    }
  }

  // Find choice/case parent?
  if (obj_->parent) {
    switch (obj_->parent->objtype) {
      case OBJ_TYP_CASE:
        choice_case_parent_ = ncxmodel_.locked_search_case(obj_->parent);
        if (nullptr == choice_case_parent_) {
          choice_case_parent_ = ncxmodel_.locked_new_case(parent, module_top, obj_->parent);
          RW_ASSERT(choice_case_parent_);
        }
        break;
      case OBJ_TYP_CHOICE:
        choice_case_parent_ = ncxmodel_.locked_search_choice(obj_->parent);
        if (nullptr == choice_case_parent_) {
          choice_case_parent_ = ncxmodel_.locked_new_choice(parent, module_top, obj_->parent);
          RW_ASSERT(choice_case_parent_);
        }
        break;
      default:
        break;
    }
  }

}

YangNodeNcx::~YangNodeNcx()
{
}

std::string YangNodeNcx::get_location()
{
  RW_ASSERT(obj_);
  RW_ASSERT(obj_->tkerr.mod);
  RW_ASSERT(obj_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << obj_->tkerr.mod->sourcefn << ':' << obj_->tkerr.linenum << ':' << obj_->tkerr.linepos;
  return oss.str();
}

const char* YangNodeNcx::get_description()
{
  RW_ASSERT(description_);
  return description_;
}

const char* YangNodeNcx::get_help_short()
{
  if (cache_state_.is_cached(NCX_CACHED_NODE_HELP_SHORT)) {
    return help_short_.c_str();
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (cache_state_.is_cached(NCX_CACHED_NODE_HELP_SHORT)) {
    return help_short_.c_str();
  }

  char buf[RW_YANG_SHORT_DESC_BUFLEN];
  rw_yang_make_help_short(get_description(),buf);
  if (buf[0] == '\0') {
    rw_yang_make_help_short(get_name(),buf);
  }
  help_short_ = buf;

  if (is_rpc_input() && is_mandatory()) {
    help_short_ += std::string(" [mandatory]");
  }

  cache_state_.locked_cache_set_flag_only(NCX_CACHED_NODE_HELP_SHORT);
  return help_short_.c_str();
}

const char* YangNodeNcx::get_help_full()
{
  RW_ASSERT(description_);
  return description_;
}

const char* YangNodeNcx::get_name()
{
  RW_ASSERT(name_);
  return name_;
}

const char* YangNodeNcx::get_prefix()
{
  return prefix_;
}

const char* YangNodeNcx::get_ns()
{
  return ns_;
}

uint32_t YangNodeNcx::get_node_tag()
{
  return node_tag_;
}

const char* YangNodeNcx::get_default_value()
{
  RW_ASSERT(obj_);
  // ATTN: Cache this?
  // ATTN: Caching might be useful, because it would allow us to change it at runtime.
  return (const char*)obj_get_default(ncxmodel_.ncx_instance_, obj_);
}

uint32_t YangNodeNcx::get_max_elements()
{
  RW_ASSERT(obj_);
  switch (stmt_type_) {
    case RW_YANG_STMT_TYPE_LIST:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      break;

    default:
      return 1;
  }

  uint32 maxelems;
  boolean was_set = obj_get_max_elements(ncxmodel_.ncx_instance_, obj_, &maxelems);
  if (was_set) {
    RW_ASSERT(maxelems >= 1);
    return maxelems;
  }

  return UINT32_MAX;
}

rw_yang_stmt_type_t YangNodeNcx::get_stmt_type() 
{
  return stmt_type_;
}

bool YangNodeNcx::is_config()
{
  return is_config_;
}

bool YangNodeNcx::is_key()
{
  return is_key_;
}

bool YangNodeNcx::is_presence()
{
  return is_presence_;
}

bool YangNodeNcx::is_mandatory()
{
  return is_mandatory_;
}

bool YangNodeNcx::has_default()
{
  if (get_stmt_type() != RW_YANG_STMT_TYPE_CONTAINER) {
    return (get_default_value() != nullptr);
  }

  bool has_default = false;
  // Check the child cached status
  if (has_default_.is_cached_and_get(cache_state_, &has_default)) {
    return has_default;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  // Has Default not cached. Go deep through the descendants, 
  // to find any default leaf. Cache the finding.
  for (YangNodeIter it = child_begin();
       it != child_end(); ++it) {
    YangNode* ychild = &(*it);
    if (ychild->has_default()) {
      has_default = true;
      break;
    }
  }

  has_default_.locked_cache_set(&cache_state_, has_default);

  return has_default;
}

YangNode* YangNodeNcx::get_parent()
{
  RW_ASSERT(parent_);
  return parent_;
}

YangNodeNcx* YangNodeNcx::get_first_child()
{
  YangNodeNcx* retval;
  if (   version_.is_up_to_date(ncxmodel_)
      && first_child_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (!version_.is_up_to_date(ncxmodel_)) {
    locked_update_version();
  }
  if (first_child_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  return locked_update_first_child();
}

YangNodeNcx* YangNodeNcx::get_next_sibling()
{
  YangNodeNcx* retval;
  if (   version_.is_up_to_date(ncxmodel_)
      && next_sibling_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (!version_.is_up_to_date(ncxmodel_)) {
    locked_update_version();
  }
  if (next_sibling_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  return locked_update_next_sibling();
}

YangType* YangNodeNcx::get_type()
{
  return type_;
}

YangValue* YangNodeNcx::get_first_value()
{
  RW_ASSERT(   stmt_type_ == RW_YANG_STMT_TYPE_LEAF
            || stmt_type_ == RW_YANG_STMT_TYPE_LEAF_LIST
            || stmt_type_ == RW_YANG_STMT_TYPE_ANYXML);
  if (nullptr == type_) {
    return nullptr;
  }
  return type_->get_first_value();
}

YangKey* YangNodeNcx::get_first_key()
{
  YangKeyNcx* retval = nullptr;

  if (   version_.is_up_to_date(ncxmodel_)
         && first_key_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (!version_.is_up_to_date(ncxmodel_)) {
    locked_update_version();
  }

  if (first_key_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  return locked_update_first_key();
}

YangExtNcx* YangNodeNcx::get_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_extension();
}

YangModule* YangNodeNcx::get_module()
{
  if (module_top_) {
    return module_top_;
  }
  return module_;
}

YangModule* YangNodeNcx::get_module_orig()
{
  return module_orig_;
}

YangAugment* YangNodeNcx::get_augment()
{
  YangAugmentNcx* retval = nullptr;
  if (augment_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_augment();
}

YangUses* YangNodeNcx::get_uses()
{
  YangUsesNcx* retval = nullptr;
  if (uses_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_uses();
}

YangNode* YangNodeNcx::get_reusable_grouping()
{
  YangUses *uses = get_uses();

  if (uses != nullptr && !uses->is_refined() && !uses->is_augmented()) {
    return uses->get_grouping();
  }
  return nullptr;
}

YangNodeNcx* YangNodeNcx::get_case()
{
  if (choice_case_parent_) {
    switch (stmt_type_) {
      case RW_YANG_STMT_TYPE_CASE:
        // The choice/case parent must be a choice. Its parent is a case, if any.
        return choice_case_parent_->choice_case_parent_;

      case RW_YANG_STMT_TYPE_CHOICE:
      default:
        // The choice/case parent must be a case.
        return choice_case_parent_;
    }
  }
  return nullptr;
}

YangNodeNcx* YangNodeNcx::get_choice()
{
  if (choice_case_parent_) {
    switch (stmt_type_) {
      case RW_YANG_STMT_TYPE_CASE:
        // The choice/case parent must be a choice.
        return choice_case_parent_;

      case RW_YANG_STMT_TYPE_CHOICE:
      default: {
        // The choice/case parent must be a case, so need to use the case to get the choice.
        YangNodeNcx* the_choice = choice_case_parent_->choice_case_parent_;
        RW_ASSERT(the_choice); // must be one
        return the_choice;
      }
    }
  }
  return nullptr;
}

YangNodeNcx* YangNodeNcx::get_default_case()
{
  if (stmt_type_ != RW_YANG_STMT_TYPE_CHOICE) {
    return nullptr;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  obj_template_t* case_obj = obj_get_default_case(ncxmodel_.ncx_instance_, obj_);
  if (!case_obj) {
    return nullptr;
  }
  return ncxmodel_.locked_search_case(case_obj);
}

bool YangNodeNcx::is_conflicting_node(YangNode *other)
{
  RW_ASSERT(other);
  YangNode* this_case = get_case();
  YangNode* other_case = other->get_case();
  YangNode* this_top_choice = nullptr;
  YangNode* other_top_choice = nullptr;

  std::list<YangNode*> this_path;
  std::list<YangNode*> other_path;
  if (!this_case || !other_case || (this_case == other_case)) {
    // either one is not a case, or both belong to the same case
    return false;
  }

  // Build a case path to the root.
  while (this_case) {
    this_path.push_front(this_case);
    this_top_choice = this_case->get_choice();
    this_case = this_top_choice->get_case();
  }

  while (other_case) {
    other_path.push_front(other_case);
    other_top_choice = other_case->get_choice();
    other_case = other_top_choice->get_case();
  }

  // If the top choice parents are not the same, the nodes do not
  // conflict
  if (this_top_choice != other_top_choice) {
    return false;
  }

  // If the paths are the same length, they DO conflict (because the
  // deepest cases have already been compared)
  if (this_path.size() == other_path.size()) {
    return true;
  }

  /* walk both paths, if one path is a sub path of the other, then the
   * nodes do not conflict. Otherwise, they do
   */
  std::list<YangNode *>::const_iterator ti = this_path.begin();
  std::list<YangNode *>::const_iterator oi = other_path.begin();
  while (   (ti != this_path.end())
         && (oi != other_path.end())
         && (*oi == *ti)) {
    ++oi;
    ++ti;
  }

  if ((ti == this_path.end()) || (oi == other_path.end())) {
    // One path is the subpath of the other
    return false;
  }

  return true;
}


const char* YangNodeNcx::get_pbc_field_name()
{
  if (cache_state_.is_cached(NCX_CACHED_NODE_PB_FIELD_NAME)) {
    return pbc_field_name_.c_str();
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (cache_state_.is_cached(NCX_CACHED_NODE_PB_FIELD_NAME)) {
    return pbc_field_name_.c_str();
  }

  for (YangExtensionIter xi = extension_begin(); xi !=extension_end(); ++xi) {
    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_NAME)) {
      const char* id = xi->get_value();
      if (!YangModel::mangle_is_c_identifier(id)) {
        // ATTN: How to handle this error?
        continue;
      }

      // Found a valid extension.  Use its value as the field name.
      pbc_field_name_ = id;
      cache_state_.locked_cache_set_flag_only(NCX_CACHED_NODE_PB_FIELD_NAME);
      pbc_field_name_.c_str();
    }
  }

  // Did not find an extension.  Mangle the node name to create a C identifier.
  pbc_field_name_ = YangModel::mangle_to_c_identifier(get_name());
  cache_state_.locked_cache_set_flag_only(NCX_CACHED_NODE_PB_FIELD_NAME);
  return pbc_field_name_.c_str();
}

bool YangNodeNcx::is_mode_path()
{
  return is_mode_path_;
}

void YangNodeNcx::set_mode_path()
{
  is_mode_path_ = true;
}

// set all the nodes in the path to the root as "mode_path" nodes
void YangNodeNcx::set_mode_path_to_root()
{
  is_mode_path_ = true;
    for (YangNode *parent = parent_; parent && !parent->is_mode_path() ; parent = parent->get_parent()) {
      parent->set_mode_path();
    }
}

bool YangNodeNcx::is_rpc()
{
  return (RW_YANG_STMT_TYPE_RPC == stmt_type_);
}

bool YangNodeNcx::is_rpc_input()
{
  return is_rpc_input_;
}

bool YangNodeNcx::is_rpc_output()
{
  return is_rpc_output_;
}

bool YangNodeNcx::is_notification()
{
  return is_notification_;
}

std::string YangNodeNcx::get_enum_string(uint32_t value)
{
  RW_ASSERT (is_leafy());
  RW_ASSERT (type_->get_leaf_type() == RW_YANG_LEAF_TYPE_ENUM);

  for (YangValueIter yvi = value_begin(); yvi != value_end(); ++yvi) {
    if (yvi->get_position() == value) {
      return std::string(yvi->get_name());
    }
  }
  return std::string("");
}

YangNode* YangNodeNcx::get_leafref_ref()
{
  RW_ASSERT(   stmt_type_ == RW_YANG_STMT_TYPE_LEAF
            || stmt_type_ == RW_YANG_STMT_TYPE_LEAF_LIST);

  YangNodeNcx* retval;
  if (leafref_ref_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (leafref_ref_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  obj_template_t* ref_obj = obj_get_leafref_targobj(ncxmodel_.ncx_instance_, obj_);
  if (nullptr == ref_obj) {
    return leafref_ref_.locked_cache_set(&cache_state_, nullptr);
  }

  retval = ncxmodel_.locked_populate_target_node(ref_obj);
  RW_ASSERT(retval);
  return leafref_ref_.locked_cache_set(&cache_state_, retval);
}


std::string YangNodeNcx::get_leafref_path_str()
{
  RW_ASSERT(   stmt_type_ == RW_YANG_STMT_TYPE_LEAF
            || stmt_type_ == RW_YANG_STMT_TYPE_LEAF_LIST);

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  const typ_def_t* typ = obj_get_ctypdef(ncxmodel_.ncx_instance_, obj_);
  if (typ == nullptr) {
    return std::string();
  }

  const void* exprstr = typ_get_leafref_path(ncxmodel_.ncx_instance_, typ);
  if (exprstr == nullptr) {
    return std::string();
  }

  return std::string(static_cast<const char*>(exprstr));
}

bool YangNodeNcx::app_data_is_cached(const AppDataTokenBase* token) const noexcept
{
  return app_data_.is_cached(token);
}

bool YangNodeNcx::app_data_is_looked_up(const AppDataTokenBase* token) const noexcept
{
  return app_data_.is_looked_up(token);
}

bool YangNodeNcx::app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept
{
  return app_data_.check_and_get(token, data);
}

bool YangNodeNcx::app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data)
{
  // Trivial case is that it was already looked up...
  if (app_data_.is_looked_up(token)) {
    return app_data_.check_and_get(token, data);
  }

  // Need to take the lock so that we don't race another lookup
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (app_data_.is_looked_up(token)) {
    // raced someone else
    return app_data_.check_and_get(token, data);
  }

  // Search for it...  Don't search if user gave us empty strings.
  if (token->ns_.length() && token->ext_.length()) {
    YangExtension* yext = search_extension(token->ns_.c_str(), token->ext_.c_str());
    if (yext) {
      const char* ext_value = yext->get_value();
      intptr_t intptr_value = reinterpret_cast<intptr_t>(ext_value);
      intptr_t old_value = app_data_set_and_keep_ownership(token, intptr_value);
      RW_ASSERT(0 == old_value);
      *data = intptr_value;
      return true;
    }
  }
  app_data_.set_looked_up(token);
  return false;
}

void YangNodeNcx::app_data_set_looked_up(const AppDataTokenBase* token)
{
  app_data_.set_looked_up(token);
}

intptr_t YangNodeNcx::app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data)
{
  return app_data_.set_and_give_ownership(token, data);
}

intptr_t YangNodeNcx::app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data)
{
  return app_data_.set_and_keep_ownership(token, data);
}

YangModel* YangNodeNcx::get_model()
{
  return &ncxmodel_;
}

void YangNodeNcx::locked_update_version()
{
  if (version_.is_up_to_date(ncxmodel_)) {
    return;
  }
  if (first_child_.is_cached(cache_state_)) {
    locked_update_first_child();
  }
  if (next_sibling_.is_cached(cache_state_)) {
    locked_update_next_sibling();
  }

  // ATTN: not updating CLI mode and print hooks on updates for now.
  // no way to ensure the "stickiness" of manifest based enhancements

  version_.updated(ncxmodel_);
}

YangNodeNcx* YangNodeNcx::locked_update_first_child()
{
  YangNodeNcx* retval = nullptr;
  bool already_cached = first_child_.is_cached_and_get(cache_state_, &retval);

  RW_ASSERT(obj_);
  obj_template_t* ch_obj;
  switch (stmt_type_) {
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_CHOICE:
      ch_obj = obj_first_child(ncxmodel_.ncx_instance_, obj_);
      break;
    default:
      ch_obj = obj_first_child_deep(ncxmodel_.ncx_instance_, obj_);
      break;
  }
  if (nullptr == ch_obj) {
    // There is no current yang child.
    if (!already_cached) {
      // Didn't know that before...  Cache the fact.
      return first_child_.locked_cache_set(&cache_state_, nullptr);
    }

    RW_ASSERT(nullptr == retval); // Had a child and now it's gone?  Huh?
    return retval;
  }

  // There is a yang child node.  See if the cached node changed.
  if (!already_cached) {
    // First time walking the children...
    ; /* Fall through */
  } else if (nullptr == retval) {
    // The child is new - perhaps an augment came in.
    ; /* Fall through */
  } else if (ch_obj == retval->obj_) {
    // The child hasn't changed.
    return retval;
  } else {
    // The first child changed?  Huh?
    RW_ASSERT_NOT_REACHED();
  }

  retval = ncxmodel_.locked_search_node_case_choice(ch_obj);
  if (retval) {
    return next_sibling_.locked_cache_set(&cache_state_, retval);
  }

  YangNode* parent = this;
  switch (stmt_type_) {
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_CHOICE:
      parent = parent_;
      break;
    default:
      break;
  }

  retval = ncxmodel_.locked_new_node(parent, ch_obj);
  return first_child_.locked_cache_set(&cache_state_, retval);
}

YangNodeNcx* YangNodeNcx::locked_update_next_sibling()
{
  if (module_top_) {
    return locked_update_next_sibling_top();
  }

  YangNodeNcx* retval = nullptr;
  bool already_cached = next_sibling_.is_cached_and_get(cache_state_, &retval);

  RW_ASSERT(obj_);
  obj_template_t* sib_obj;
  switch (stmt_type_) {
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_CHOICE:
      sib_obj = obj_next_child(ncxmodel_.ncx_instance_, obj_);
      break;
    default:
      sib_obj = obj_next_child_deep(ncxmodel_.ncx_instance_, obj_);
      break;
  }
  if (nullptr == sib_obj) {
    // There is no current yang next sibling.
    if (!already_cached) {
      // Didn't know that before...  Cache the fact.
      return next_sibling_.locked_cache_set(&cache_state_, nullptr);
    }

    RW_ASSERT(nullptr == retval); // Had a sibling and now it's gone?  Huh?
    return retval;
  }

  // There is a yang sibling node.  See if the model cached node changed.
  if (!already_cached) {
    // First time walking the siblings...
    ; /* Fall through */
  } else if (nullptr == retval) {
    // The sibling is new - perhaps an augment came in.
    ; /* Fall through */
  } else if (sib_obj == retval->obj_) {
    // The first child hasn't changed.
    return retval;
  } else {
    // The first child changed?  Huh?
    RW_ASSERT_NOT_REACHED();
  }

  retval = ncxmodel_.locked_search_node_case_choice(sib_obj);
  if (retval) {
    return next_sibling_.locked_cache_set(&cache_state_, retval);
  }

  retval = ncxmodel_.locked_new_node(this->parent_, sib_obj);
  return next_sibling_.locked_cache_set(&cache_state_, retval);
}

YangNodeNcx* YangNodeNcx::locked_update_next_sibling_top()
{
  RW_ASSERT(module_top_);
  YangNodeNcx* cached_next = nullptr;
  bool already_cached = next_sibling_.is_cached_and_get(cache_state_, &cached_next);

  // Determine if there is a sibling node in this module.
  ncx_module_t* my_ncx_mod = module_top_->mod_;
  RW_ASSERT(my_ncx_mod);
  RW_ASSERT(obj_);
  obj_template_t* sib_obj = ncx_get_next_object(ncxmodel_.ncx_instance_, my_ncx_mod, obj_ );
  if (nullptr != sib_obj) {
    if (already_cached && cached_next) {
      RW_ASSERT(cached_next->module_top_);
      if (cached_next->module_top_ == module_top_) {
        RW_ASSERT(cached_next->obj_ == sib_obj); // Added new node to this module in the middle?
        return cached_next;
      }

      // The last node in this module has changed to sib_obj.
      YangNodeNcx* new_node = ncxmodel_.locked_new_node(module_top_, sib_obj);
      new_node->next_sibling_.locked_cache_set(&new_node->cache_state_, cached_next);
      next_sibling_.locked_cache_set(&cache_state_, new_node);
      return new_node;
    }

    // The last node in this module has changed to sib_obj, and there is no known next.
    YangNodeNcx* new_node = ncxmodel_.locked_new_node(module_top_, sib_obj);
    next_sibling_.locked_cache_set(&cache_state_, new_node);
    return new_node;
  }

  // There is no sibling to this node in this module.
  // Look in the following module(s).
  YangModuleIter mi(module_top_);
  for (++mi; mi != ncxmodel_.module_end(); ++mi ) {
    // Only explicit loads count!
    if (mi->was_loaded_explicitly()) {
      YangNode* found_node = mi->get_first_node();
      if (nullptr != found_node) {
        /*
         * The first node in a module changed, or a module became explicitly
         * loaded.  The next_sibling chain of top nodes is now discontinuous!
         * It is NOT this function's job to fix that - this function is only
         * responsible for fixing its own next pointer.  Eventually, if all
         * the top nodes are walked, then the chain will get fixed.  Only
         * module end nodes should be incorrect; interior module nodes are
         * not affected.
         */
        YangNodeNcx* retval = static_cast<YangNodeNcx*>(found_node); // must be
        next_sibling_.locked_cache_set(&cache_state_, retval);
        return retval;
      }
    }
  }

  // Did not find a following, explicitly loaded, module with a first
  // node.  There is no next.
  if (already_cached) {
    RW_ASSERT(nullptr == cached_next);  // Lost a node?  Huh?
    // Already knew there was no next.
    return cached_next;
  }

  // Remember that there is no next.
  next_sibling_.locked_cache_set(&cache_state_, nullptr);
  return nullptr;
}

YangKeyNcx* YangNodeNcx::locked_update_first_key()
{
  YangKeyNcx* retval;
  if (first_key_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  RW_ASSERT(obj_);

  if (stmt_type_ != RW_YANG_STMT_TYPE_LIST) {
    return nullptr;
  }

  obj_key_t* key = obj_first_key(ncxmodel_.ncx_instance_, obj_);
  if (nullptr == key) {
    // Has no keys.
    return first_key_.locked_cache_set(&cache_state_, nullptr);
  }

  obj_template_t* key_obj = key->keyobj;
  RW_ASSERT(nullptr != key_obj);
  RW_ASSERT(key_obj->parent == obj_); // no deep keys allowed by yang

  // Find the child_node node that defines the key.  This may populate
  // the entire child list.  Oh well.
  for( YangNodeNcx* child_node = get_first_child();
       nullptr != child_node;
       child_node = child_node->get_next_sibling() ) {
    if (key_obj == child_node->obj_) {
      // Found it
      retval = ncxmodel_.locked_new_key(this, child_node, key);
      RW_ASSERT(child_node->is_key_);
      return first_key_.locked_cache_set(&cache_state_, retval);
    }
  }

  // Didn't find the key node?  Huh?
  RW_ASSERT_NOT_REACHED();
}

YangExtNcx* YangNodeNcx::locked_update_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  // Check if the current node has that extension
  RW_ASSERT(obj_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &obj_->appinfoQ);
  if (nullptr != appinfo) {
    // Start walking the uses extensions.
    retval = ncxmodel_.locked_new_ext(appinfo, obj_);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  if (nullptr == obj_->usesobj) {
    // No uses or grouping to look in
    return first_extension_.locked_cache_set(&cache_state_, nullptr);
  }

  RW_ASSERT(OBJ_TYP_USES == obj_->usesobj->objtype);
  appinfo = (ncx_appinfo_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &obj_->usesobj->appinfoQ);
  if (nullptr != appinfo) {
    // Start walking the uses extensions.
    retval = ncxmodel_.locked_new_ext(appinfo, obj_);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  RW_ASSERT(obj_->usesobj->def.uses);
  RW_ASSERT(obj_->usesobj->def.uses->grp);
  appinfo = (ncx_appinfo_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &obj_->usesobj->def.uses->grp->appinfoQ);
  if (nullptr != appinfo) {
    // Start walking the uses extensions.
    retval = ncxmodel_.locked_new_ext(appinfo, obj_);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  return first_extension_.locked_cache_set(&cache_state_, nullptr);
}

YangUsesNcx* YangNodeNcx::locked_update_uses()
{
  YangUsesNcx* retval = nullptr;
  obj_template_t* uses = nullptr;

  if (uses_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  // Get the uses from the object
  if (obj_->usesobj) {
    RW_ASSERT(obj_->usesobj->objtype == OBJ_TYP_USES);
    uses = obj_->usesobj;
  }

  if (nullptr != uses) {
    // Check if already in the map
    retval = ncxmodel_.locked_search_uses(uses);

    if (retval != nullptr) {
     return retval;
    }
    retval = ncxmodel_.locked_new_uses(this, uses);
    return uses_.locked_cache_set(&cache_state_, retval);
  }
  return uses_.locked_cache_set(&cache_state_, nullptr);
}

YangAugmentNcx* YangNodeNcx::locked_update_augment()
{
  YangAugmentNcx* retval = nullptr;
  if (augment_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  /*
   * If this node is a choice or case, or a direct child of a case,
   * then may need to search the entire choice/case hierarchy to
   * determine if this node was brought in as a result of an augment.
   */
  obj_template_t* augobj = obj_->augobj;
  obj_template_t* parent = obj_;
  while (parent && !augobj) {
    switch (parent->objtype) {
      case OBJ_TYP_CASE:
      case OBJ_TYP_CHOICE:
        augobj = parent->augobj;
        parent = parent->parent;
        continue;
      default:
        break;
    }
    break;
  }
  if (!augobj) {
    // Didn't find any augment in ncx.  This node did not come from an augment.
    return augment_.locked_cache_set(&cache_state_, nullptr);
  }

  // Already created and cached the augment descriptor?
  retval = ncxmodel_.locked_search_augment(augobj);
  if (retval) {
    return augment_.locked_cache_set(&cache_state_, retval);
  }

  /*
   * Need to create the augment descriptor.  But first need to
   * determine if it is a module-level augment, or a uses augment.  To
   * determine if it is a module-level augment, must determine the
   * correct module, and then iterate its augment list.  Painful, but
   * we cache the result.
   */
  ncx_module_t* orig_mod = augobj->tkerr.mod;
  RW_ASSERT(orig_mod);
  YangModuleNcx* module = ncxmodel_.locked_search_module(orig_mod);
  RW_ASSERT(module);
  for (YangAugmentNcx* aug = module->get_first_augment(); aug; aug = aug->get_next_augment()) {
    if (aug->obj_augment_ == augobj) {
      return augment_.locked_cache_set(&cache_state_, aug);
    }
  }

  /*
   * This is a uses augment.  This node must have a parent node (the
   * augmented node).  The parent of the augment node must be the uses
   * that specified the augment.
   *
   * ATTN: NO, a uses augment DOES NOT NEED TO HAVE A PARENT NODE
   * because uses is a valid statement at the top of a module.  So, a
   * uses augment might, effectively, augment the module root.  Yuck.
   */
  RW_ASSERT(parent_);
  RW_ASSERT(augobj->parent);
  RW_ASSERT(OBJ_TYP_USES == augobj->parent->objtype);

  // ATTN: This probably fails for choice and case augment targets
  retval = ncxmodel_.locked_new_augment(module, static_cast<YangNodeNcx*>(parent_), augobj);
  return augment_.locked_cache_set(&cache_state_, retval);
}


/*****************************************************************************/
/*!
 * Create a libncx root model node.
 */
YangNodeNcxRoot::YangNodeNcxRoot(YangModelNcxImpl& ncxmodel)
: ncxmodel_(ncxmodel),
  cache_state_(),
  version_(ncxmodel),
  first_child_(),
  app_data_(ncxmodel.app_data_mgr_)
{}

/*!
 * Get the model node of the first child of the first module with any
 * children.
 */
YangNodeNcx* YangNodeNcxRoot::get_first_child()
{
  YangNodeNcx* retval = nullptr;
  if (   version_.is_up_to_date(ncxmodel_)
         && first_child_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (!version_.is_up_to_date(ncxmodel_)) {
    locked_update_version();
  }
  if (first_child_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  return locked_update_first_child();
}

/*!
 * Update the root model node with the first module's first child.
 * ASSUMES THE MUTEX IS LOCKED.
 */
YangNodeNcx* YangNodeNcxRoot::locked_update_first_child()
{
  YangNodeNcx* retval = nullptr;
  bool is_cached = first_child_.is_cached_and_get(cache_state_, &retval);
  if (   version_.is_up_to_date(ncxmodel_)
      && is_cached) {
    return retval;
  }

  for (YangModuleIter mi = ncxmodel_.module_begin();
       mi != ncxmodel_.module_end();
       ++mi ) {
    // Only explicit loads count!
    if (mi->was_loaded_explicitly()) {
      YangNode* node = mi->get_first_node();
      if (nullptr != node) {
        retval = static_cast<YangNodeNcx*>(node); // must be
        return first_child_.locked_cache_set(&cache_state_, retval);
      }
    }
  }

  // Found none.
  RW_ASSERT(!is_cached || nullptr == retval); // Had a first child, and then lost it?  Huh?
  return first_child_.locked_cache_set(&cache_state_, nullptr);
}

/*!
 * Update the node pointers that may have changed due to a version
 * change.  Only update those nodes that have already been cached.
 */
void YangNodeNcxRoot::locked_update_version()
{
  if (first_child_.is_cached(cache_state_)) {
    locked_update_first_child();
  }
  version_.updated(ncxmodel_);
}

YangModel *YangNodeNcxRoot::get_model()
{
  return &ncxmodel_;
}

bool YangNodeNcxRoot::app_data_is_cached(const AppDataTokenBase* token) const noexcept
{
  return app_data_.is_cached(token);
}

bool YangNodeNcxRoot::app_data_is_looked_up(const AppDataTokenBase* token) const noexcept
{
  return app_data_.is_looked_up(token);
}

bool YangNodeNcxRoot::app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept
{
  return app_data_.check_and_get(token, data);
}

bool YangNodeNcxRoot::app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data)
{
  // Trivial case is that it was already looked up...
  if (app_data_.is_looked_up(token)) {
    return app_data_.check_and_get(token, data);
  }

  // Need to take the lock so that we don't race another lookup
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (app_data_.is_looked_up(token)) {
    // raced someone else
    return app_data_.check_and_get(token, data);
  }

  // Search for it...  Don't search if user gave us empty strings.
  if (token->ns_.length() && token->ext_.length()) {
    YangExtension* yext = search_extension(token->ns_.c_str(), token->ext_.c_str());
    if (yext) {
      const char* ext_value = yext->get_value();
      intptr_t intptr_value = reinterpret_cast<intptr_t>(ext_value);
      intptr_t old_value = app_data_set_and_keep_ownership(token, intptr_value);
      RW_ASSERT(0 == old_value);
      *data = intptr_value;
      return true;
    }
  }
  app_data_.set_looked_up(token);
  return false;
}

void YangNodeNcxRoot::app_data_set_looked_up(const AppDataTokenBase* token)
{
  app_data_.set_looked_up(token);
}

intptr_t YangNodeNcxRoot::app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data)
{
  return app_data_.set_and_give_ownership(token, data);
}

intptr_t YangNodeNcxRoot::app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data)
{
  return app_data_.set_and_keep_ownership(token, data);
}


/*****************************************************************************/
/*!
 * Construct a yang model type descriptor for libncx.
 */
YangTypeNcx::YangTypeNcx(YangModelNcxImpl& ncxmodel, typ_def_t* typ)
: ncxmodel_(ncxmodel),
  cache_state_(),
  typ_(typ),
  typed_enum_(nullptr),
  first_value_(),
  first_extension_(),
  ns_(nullptr)
{
  RW_ASSERT(typ_);
  xmlns_id_t nsid = typ->tkerr.mod->nsid;
  if (NCX_CL_NAMED == typ->tclass) {
    typ_template_t *templ = typ->def.named.typ;
    while (NCX_CL_NAMED == templ->typdef.tclass) {
      templ = templ->typdef.def.named.typ;
    }
    // This could be pointing to a typed enumeration. For exact correlation,
    // find it, create a new typed object if required, and stash of its pointer

    if ((NCX_CL_SIMPLE == templ->typdef.tclass) &&
        (NCX_BT_ENUM == templ->typdef.def.simple.btyp)) {
      nsid = templ->tkerr.mod->nsid;

      // Already under a lock - search and create if required
      typed_enum_ = ncxmodel_.locked_search_typed_enum (templ);

      if (nullptr == typed_enum_) {
        typed_enum_ = ncxmodel_.locked_new_typed_enum (templ);
      }

      RW_ASSERT (typed_enum_);
    }
  }

  RW_ASSERT(nsid);
  ns_ = (const char*)xmlns_get_ns_name(ncxmodel_.ncx_instance_, nsid);
  RW_ASSERT(ns_);
  RW_ASSERT(ns_[0]);
}

/*!
 * Get the source file location of the type definition.
 */
std::string YangTypeNcx::get_location()
{
  RW_ASSERT(typ_);
  RW_ASSERT(typ_->tkerr.mod);
  RW_ASSERT(typ_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << typ_->tkerr.mod->sourcefn << ':' << typ_->tkerr.linenum << ':' << typ_->tkerr.linepos;
  return oss.str();
}

/*!
 * Get the leaf type, which is possibly not unitary.
 */
rw_yang_leaf_type_t YangTypeNcx::get_leaf_type()
{
  RW_ASSERT(typ_);
  // ATTN: Should cache this
  switch (typ_get_basetype(ncxmodel_.ncx_instance_, typ_)) {
    case NCX_BT_BOOLEAN: return RW_YANG_LEAF_TYPE_BOOLEAN;
    case NCX_BT_INT8: return RW_YANG_LEAF_TYPE_INT8;
    case NCX_BT_INT16: return RW_YANG_LEAF_TYPE_INT16;
    case NCX_BT_INT32: return RW_YANG_LEAF_TYPE_INT32;
    case NCX_BT_INT64: return RW_YANG_LEAF_TYPE_INT64;
    case NCX_BT_UINT8: return RW_YANG_LEAF_TYPE_UINT8;
    case NCX_BT_UINT16: return RW_YANG_LEAF_TYPE_UINT16;
    case NCX_BT_UINT32: return RW_YANG_LEAF_TYPE_UINT32;
    case NCX_BT_UINT64: return RW_YANG_LEAF_TYPE_UINT64;
    case NCX_BT_DECIMAL64: return RW_YANG_LEAF_TYPE_DECIMAL64;
    case NCX_BT_STRING: return RW_YANG_LEAF_TYPE_STRING;
    case NCX_BT_BINARY: return RW_YANG_LEAF_TYPE_BINARY;
    case NCX_BT_INSTANCE_ID: return RW_YANG_LEAF_TYPE_INSTANCE_ID;
    case NCX_BT_LEAFREF: return RW_YANG_LEAF_TYPE_LEAFREF;
    case NCX_BT_BITS: return RW_YANG_LEAF_TYPE_BITS;
    case NCX_BT_ENUM: return RW_YANG_LEAF_TYPE_ENUM;
    case NCX_BT_EMPTY: return RW_YANG_LEAF_TYPE_EMPTY;
    case NCX_BT_IDREF: return RW_YANG_LEAF_TYPE_IDREF;
    case NCX_BT_UNION: return RW_YANG_LEAF_TYPE_UNION;
    default: break;
  }
  RW_ASSERT_NOT_REACHED();
}

uint8_t YangTypeNcx::get_dec64_fraction_digits()
{
  RW_ASSERT(typ_);
  // We need to cache this??
  RW_ASSERT(get_leaf_type() == RW_YANG_LEAF_TYPE_DECIMAL64);
  return typ_get_fraction_digits(ncxmodel_.ncx_instance_, typ_);
  // ATTN: Need to assert here for non zero?
}

/*!
 * Get the first value descriptor.  The list skips unions - they get
 * expanded inline.  Value descriptor is owned by the model.  Will be
 * nullptr if the leaf type is empty.  Caller may use value descriptor
 * until model is destroyed.
 */
YangValue* YangTypeNcx::get_first_value()
{
  YangValue* retval = nullptr;
  if (first_value_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  // ATTN: Can values change after a module load?
  //   What about new identities or enum values?

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_value();
}

/*!
 * Get the first extension descriptor.  The descriptor is owned by the
 * model.  Will be nullptr if there are no extensions.  Caller may use
 * extension descriptor until model is destroyed.
 */
YangExtNcx* YangTypeNcx::get_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_extension();
}

/*!
 * Update the value node for the first deep value.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangValue* YangTypeNcx::locked_update_first_value()
{
  YangValue* retval = nullptr;
  if (first_value_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(typ_);
  expand_typ_t values = locked_expand_typ(typ_,typ_,true/*top*/);
  return first_value_.locked_cache_set(&cache_state_, values.first);
}

/*!
 * Update the model node for the first list extension.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangExtNcx* YangTypeNcx::locked_update_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(typ_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &typ_->appinfoQ);
  if (nullptr != appinfo) {
    retval = ncxmodel_.locked_new_ext(appinfo, typ_);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  //If this is a named type, check for any extensions in the types type definition template.
  if (NCX_CL_NAMED == typ_->tclass) {
    if (typ_->def.named.typ) {
      appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &typ_->def.named.typ->typdef.appinfoQ);
      if (nullptr != appinfo) {
        retval = ncxmodel_.locked_new_ext(appinfo, typ_);
        return first_extension_.locked_cache_set(&cache_state_, retval);
      }
    }
  }

  return first_extension_.locked_cache_set(&cache_state_, nullptr);
}

/*!
 * Expand a libncx type into a list of values.  Will recursively expand
 * embedded values, if the type is a union, enum, or bits.
 */
YangTypeNcx::expand_typ_t YangTypeNcx::locked_expand_typ(typ_def_t* top_typ, typ_def_t* cur_typ, bool top)
{
  RW_ASSERT(top_typ);
  RW_ASSERT(cur_typ);

  if (NCX_CL_NAMED == cur_typ->tclass) {
    typ_def_t* cur_typdef = typ_get_next_typdef(ncxmodel_.ncx_instance_, cur_typ);
    RW_ASSERT(cur_typdef != cur_typ);
    return locked_expand_typ(top_typ, cur_typdef, top);
  }

  ncx_btype_t bt = typ_get_basetype(ncxmodel_.ncx_instance_, cur_typ);
  switch (bt) {
    // We accept all of these trivially.
    case NCX_BT_INT8:
    case NCX_BT_INT16:
    case NCX_BT_INT32:
    case NCX_BT_INT64:
    case NCX_BT_UINT8:
    case NCX_BT_UINT16:
    case NCX_BT_UINT32:
    case NCX_BT_UINT64:
    case NCX_BT_DECIMAL64:
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
      break;

    case NCX_BT_BOOLEAN:
      return locked_expand_boolean(top_typ);

    case NCX_BT_LEAFREF:
      // ATTN: may need special handling some day, but for now, consider it a trivial type.
      RW_ASSERT(top); // leafref is not allowed in unions.
      break;

    case NCX_BT_INSTANCE_ID:
      // ATTN: may need special handling some day, but for now, consider it a trivial type.
      break;

    case NCX_BT_EMPTY:
      // No values.  Must be sole entry.
      RW_ASSERT(top); // empty is not allowed in unions
      return expand_typ_t(nullptr,nullptr);

    case NCX_BT_IDREF:
      return locked_expand_idref(top_typ,cur_typ);

    case NCX_BT_BITS:
      return locked_expand_bits(top_typ,cur_typ);

    case NCX_BT_ENUM:
      return locked_expand_enum(top_typ,cur_typ);

    case NCX_BT_UNION:
      return locked_expand_union(top_typ,cur_typ);

    // These are bogus.
    case NCX_BT_NONE:
    case NCX_BT_ANY:
    case NCX_BT_SLIST:
    case NCX_BT_FLOAT64:
    case NCX_BT_EXTERN:
    case NCX_BT_INTERN:
      RW_ASSERT_NOT_REACHED();

    // These are complex types, shouldn't see them if leafy.
    case NCX_BT_CONTAINER:
    case NCX_BT_CHOICE:
    case NCX_BT_CASE:
    case NCX_BT_LIST:
      RW_ASSERT_NOT_REACHED();

    default:
      // Garbage!
      RW_ASSERT_NOT_REACHED();
  }

  YangValueNcx* value_p = ncxmodel_.locked_new_value(this,top_typ);
  return expand_typ_t(value_p,value_p);
}

/*!
 * Expand a libncx boolean type into a list of values.
 */
YangTypeNcx::expand_typ_t YangTypeNcx::locked_expand_boolean(typ_def_t * typ)
{
  RW_ASSERT(typ);

  expand_typ_t retval(
      ncxmodel_.locked_new_value_boolean(this, typ, false),
      ncxmodel_.locked_new_value_boolean(this, typ, true));

  retval.first->next_value_ = retval.second;
  return retval;
}

/*!
 * Expand a libncx union type into a list of values.  Recursively
 * expands embedded types.
 */
YangTypeNcx::expand_typ_t YangTypeNcx::locked_expand_union(typ_def_t* top_typ, typ_def_t* typ)
{
  RW_ASSERT(top_typ);
  RW_ASSERT(typ);
  RW_ASSERT(!dlq_empty(ncxmodel_.ncx_instance_, &typ->def.simple.unionQ));
  expand_typ_t retval(nullptr,nullptr);

  // Iterate over union list...
  for (typ_unionnode_t* un = (typ_unionnode_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &typ->def.simple.unionQ);
       nullptr != un;
       un = (typ_unionnode_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, un)) {

    expand_typ_t partial;
    if (un->typ) {
      partial = locked_expand_typ(&un->typ->typdef,&un->typ->typdef,false/*top*/);
    } else {
      RW_ASSERT(un->typdef);
      partial = locked_expand_typ(un->typdef,un->typdef,false/*top*/);
    }

    if (partial.first) {
      if (nullptr == retval.first) {
        // This is the first value found.
        retval.first = partial.first;
      }

      if (retval.second) {
        // Append the returned value(s) to the current list.
        retval.second->next_value_ = partial.first;
      }

      // Remember the current end of list.
      retval.second = partial.second;
    }
  }

  return retval;
}

/*!
 * Expand a libncx bits type into a list of values.  The values will be
 * every bit keyword supported by the type.
 */
YangTypeNcx::expand_typ_t YangTypeNcx::locked_expand_bits(typ_def_t* top_typ, typ_def_t* typ)
{
  RW_ASSERT(top_typ);
  RW_ASSERT(typ);
  expand_typ_t retval(nullptr,nullptr);

  // Iterate over value list...
  for (typ_enum_t* bits_value = typ_first_enumdef(ncxmodel_.ncx_instance_, typ);
       nullptr != bits_value;
       bits_value = typ_next_enumdef(ncxmodel_.ncx_instance_, bits_value)) {
    YangValueNcxBits* value_p = ncxmodel_.locked_new_value_bits(this,top_typ,bits_value);

    if (nullptr == retval.first) {
      // This is the first value.
      retval.first = value_p;
    }

    if (retval.second) {
      // Append the new value to the list.
      retval.second->next_value_ = value_p;
    }

    retval.second = value_p;
  }

  return retval;
}

/*!
 * Expand a libncx enumeration type into a list of values.  The values
 * will be every enumeration keyword supported by the type.
 */
YangTypeNcx::expand_typ_t YangTypeNcx::locked_expand_enum(typ_def_t* top_typ, typ_def_t* typ)
{
  RW_ASSERT(top_typ);
  RW_ASSERT(typ);
  expand_typ_t retval(nullptr,nullptr);

  // Iterate over value list...
  for (typ_enum_t* enum_value = typ_first_enumdef(ncxmodel_.ncx_instance_, typ);
       nullptr != enum_value;
       enum_value = typ_next_enumdef(ncxmodel_.ncx_instance_, enum_value)) {
    YangValueNcxEnum* value_p = ncxmodel_.locked_new_value_enum(this,top_typ,enum_value);

    if (nullptr == retval.first) {
      // This is the first value.
      retval.first = value_p;
    }

    if (retval.second) {
      // Append the new value to the list.
      retval.second->next_value_ = value_p;
    }

    retval.second = value_p;

  }

  return retval;
}

/*!
 * Expand a libncx identityref type into a list of values.  The values
 * will be every identity that uses the same base.
 */
YangTypeNcx::expand_typ_t YangTypeNcx::locked_expand_idref(typ_def_t* top_typ, typ_def_t* typ)
{
  RW_ASSERT(top_typ);
  RW_ASSERT(typ);
  expand_typ_t retval(nullptr,nullptr);

  // Get the Id Ref
  typ_idref_t *idref = typ_get_idref(ncxmodel_.ncx_instance_, typ);

  RW_ASSERT(idref);

  // Iterate over  Identities refered by this Idref
  for (ncx_idlink_t* idlink = (ncx_idlink_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &(idref->base->childQ));
       nullptr != idlink;
       idlink = (ncx_idlink_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, idlink)) {

    ncx_identity_t_ *identity_value = idlink->identity;

    RW_ASSERT(identity_value);

    YangValueNcxIdRef* value_p = ncxmodel_.locked_new_value_idref(this,top_typ, identity_value);

    if (nullptr == retval.first) {
      // This is the first value.
      retval.first = value_p;
    }

    if (retval.second) {
      // Append the new value to the list.
      retval.second->next_value_ = value_p;
    }

    retval.second = value_p;
  }

  return retval;
}

/*!
 * Check if a type is imported from another module using a typedef-ed enum or not.
 */
bool YangTypeNcx::is_imported()
{
  if (nullptr == typed_enum_) {
    return false;
  }

  if (0 == typed_enum_->typedef_->nsid) {
    return false;
  }

  RW_ASSERT(typ_);
  RW_ASSERT(nullptr != typ_->tkerr.mod);

  return typed_enum_->typedef_->nsid != typ_->tkerr.mod->nsid;
}

/*!
 * get the typed enum
 */
YangTypedEnum* YangTypeNcx::get_typed_enum()
{
  return typed_enum_;
}

/*****************************************************************************/
/*!
 * Construct a simple yang value descriptor for libncx.
 */
YangValueNcx::YangValueNcx(YangModelNcxImpl& ncxmodel, YangTypeNcx* owning_type, typ_def_t* typ)
: ncxmodel_(ncxmodel),
  typ_(typ),
  description_(nullptr),
  prefix_(nullptr),
  ns_(nullptr),
  leaf_type_(RW_YANG_LEAF_TYPE_NULL),
  max_length_(),
  owning_type_(owning_type),
  next_value_(),
  first_extension_()
{
  RW_ASSERT(typ_);
  switch (typ_get_basetype(ncxmodel_.ncx_instance_, typ_)) {
    case NCX_BT_BOOLEAN: leaf_type_ = RW_YANG_LEAF_TYPE_BOOLEAN; break;
    case NCX_BT_INT8: leaf_type_ = RW_YANG_LEAF_TYPE_INT8; break;
    case NCX_BT_INT16: leaf_type_ = RW_YANG_LEAF_TYPE_INT16; break;
    case NCX_BT_INT32: leaf_type_ = RW_YANG_LEAF_TYPE_INT32; break;
    case NCX_BT_INT64: leaf_type_ = RW_YANG_LEAF_TYPE_INT64; break;
    case NCX_BT_UINT8: leaf_type_ = RW_YANG_LEAF_TYPE_UINT8; break;
    case NCX_BT_UINT16: leaf_type_ = RW_YANG_LEAF_TYPE_UINT16; break;
    case NCX_BT_UINT32: leaf_type_ = RW_YANG_LEAF_TYPE_UINT32; break;
    case NCX_BT_UINT64: leaf_type_ = RW_YANG_LEAF_TYPE_UINT64; break;
    case NCX_BT_DECIMAL64: leaf_type_ = RW_YANG_LEAF_TYPE_DECIMAL64; break;
    case NCX_BT_STRING: leaf_type_ = RW_YANG_LEAF_TYPE_STRING; break;
    case NCX_BT_BINARY: leaf_type_ = RW_YANG_LEAF_TYPE_BINARY; break;

    // ATTN: These may need DOM referencing ability.
    case NCX_BT_INSTANCE_ID: leaf_type_ = RW_YANG_LEAF_TYPE_INSTANCE_ID; break;
    case NCX_BT_LEAFREF: leaf_type_ = RW_YANG_LEAF_TYPE_LEAFREF; break;

    // ATTN: These need special implementations!
    case NCX_BT_BITS: leaf_type_ = RW_YANG_LEAF_TYPE_BITS; break;
    case NCX_BT_ENUM: leaf_type_ = RW_YANG_LEAF_TYPE_ENUM; break;
    case NCX_BT_EMPTY: leaf_type_ = RW_YANG_LEAF_TYPE_EMPTY; break;
    case NCX_BT_IDREF: leaf_type_ = RW_YANG_LEAF_TYPE_IDREF; break;

    default:
      // All the others are not allowed!
      RW_ASSERT_NOT_REACHED();
  }

  name_ = (const char*)typ->typenamestr;
  RW_ASSERT(name_);
  RW_ASSERT(name_[0]);

  // For built-ins, this should be nullptr.  For typedefs, typ->prefix may
  // be nullptr if there was no prefix specified in the yang file; should
  // use the module's prefix in this case.
  prefix_ = (const char*)typ->prefix;
  if (nullptr == prefix_ && NCX_CL_NAMED == typ->tclass) {
    RW_ASSERT(typ->tkerr.mod);
    prefix_ = (const char*)typ->tkerr.mod->prefix;
  }
  if (nullptr == prefix_) {
    prefix_ = "";
  }

  // Only named types have a namespace.
  if (NCX_CL_NAMED == typ->tclass) {
    xmlns_id_t nsid = typ->tkerr.mod->nsid;
    RW_ASSERT(nsid);
    ns_ = (const char*)xmlns_get_ns_name(ncxmodel_.ncx_instance_, nsid);
    RW_ASSERT(ns_);
    RW_ASSERT(ns_[0]);
  }
  if (nullptr == ns_) {
    ns_ = "";
  }

  // Only named types have a description.  Base types just reuse the type.
  if (NCX_CL_NAMED == typ->tclass) {
    if (typ->def.named.typ && nullptr != typ->def.named.typ->descr) {
      description_ = (const char*)typ->def.named.typ->descr;
    }
  }
  if (nullptr == description_) {
    description_ = "";
  }
}

/*!
 * Destroy a libncx yang value descriptor.
 */
YangValueNcx::~YangValueNcx()
{
}

/*!
 * Get the source file location of a value definition.
 */
std::string YangValueNcx::get_location()
{
  RW_ASSERT(typ_);
  RW_ASSERT(typ_->tkerr.mod);
  RW_ASSERT(typ_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << typ_->tkerr.mod->sourcefn << ':' << typ_->tkerr.linenum << ':' << typ_->tkerr.linepos;
  return oss.str();
}

/*!
 * Get the description for the node.  Buffer owned by libncx.  String
 * is cached.  Caller may use string until model is destroyed.
 */
const char* YangValueNcx::get_description()
{
  RW_ASSERT(description_);
  return description_;
}

/*!
 * Get the short help for the value.  May be empty.  Buffer owned by
 * the library.  String is cached.  Caller may use string until model
 * is destroyed.
 */
const char* YangValueNcx::get_help_short()
{
  if (cache_state_.is_cached(NCX_CACHED_VALUE_HELP_SHORT)) {
    return help_short_.c_str();
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (cache_state_.is_cached(NCX_CACHED_VALUE_HELP_SHORT)) {
    return help_short_.c_str();
  }

  char buf[RW_YANG_SHORT_DESC_BUFLEN];
  rw_yang_make_help_short(get_description(),buf);
  if (buf[0] == '\0') {
    rw_yang_make_help_short(get_name(),buf);
  }
  help_short_ = buf;

  cache_state_.locked_cache_set_flag_only(NCX_CACHED_VALUE_HELP_SHORT);
  return help_short_.c_str();
}

/*!
 * Get the full help for the node.  May be empty.  Buffer owned by
 * libncx.  String is cached.  Caller may use string until model is
 * destroyed.
 */
const char* YangValueNcx::get_help_full()
{
  RW_ASSERT(description_);
  return description_;
}

/*!
 * Get the name of the node.  Buffer owned by libncx.  String is
 * cached.  Caller may use string until model is destroyed.
 */
const char* YangValueNcx::get_name()
{
  RW_ASSERT(name_);
  return name_;
}

/*!
 * Get the namespace prefix for the node.  Buffer owned by libncx.
 * String is cached until version changes.  Caller may use string until
 * model is destroyed.
 */
const char* YangValueNcx::get_prefix()
{
  return prefix_;
}

/*!
 * Get the full namespace name for the node.  Buffer owned by libncx.
 * String is cached until version changes.  Caller may use string until
 * model is destroyed.
 */
const char* YangValueNcx::get_ns()
{
  return ns_;
}

/*!
 * Get the yang leaf type.
 * ATTN: What do enum and bits mean?
 */
rw_yang_leaf_type_t YangValueNcx::get_leaf_type()
{
  return leaf_type_;
}

/*!
 * Get the maximum number of elemets for a list or leaf-list.  If the
 * object is not a leaf-list or list, then it will return 1.
 */
uint64_t YangValueNcx::get_max_length()
{
  uint64_t retval = 0;
  if (max_length_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (max_length_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  switch (leaf_type_) {
    case RW_YANG_LEAF_TYPE_INT8:
    case RW_YANG_LEAF_TYPE_INT16:
    case RW_YANG_LEAF_TYPE_INT32:
    case RW_YANG_LEAF_TYPE_INT64:
    case RW_YANG_LEAF_TYPE_UINT8:
    case RW_YANG_LEAF_TYPE_UINT16:
    case RW_YANG_LEAF_TYPE_UINT32:
    case RW_YANG_LEAF_TYPE_UINT64:
    case RW_YANG_LEAF_TYPE_DECIMAL64:
    case RW_YANG_LEAF_TYPE_EMPTY:
    case RW_YANG_LEAF_TYPE_BOOLEAN:
    case RW_YANG_LEAF_TYPE_ENUM:
      // Not applicable to non-string types
      goto cache_it;

    case RW_YANG_LEAF_TYPE_BITS:
      // ATTN: yang doesn't support length on this type,
      // but it might be interesting to supply max-position/8.
      goto cache_it;

    case RW_YANG_LEAF_TYPE_STRING:
    case RW_YANG_LEAF_TYPE_BINARY:
      // length argument is supported in yang
      break;

    case RW_YANG_LEAF_TYPE_LEAFREF:
      // ATTN: The field contains a value from another leaf, somewhere
      //       else in the schema.  It should be possible to get that
      //       leaf's length, which we should do here
      // ATTN: For now, use a ridiculously small value, to force early
      //       failure in the near future, so we have to fix it
      retval = 4;
      goto cache_it;

    case RW_YANG_LEAF_TYPE_IDREF:
      // ATTN: No length in yang.
      // ATTN: Might be possible to determine actual worst-case length
      //       by inspecting all identities with the same base.  May
      //       also be nice to have extension to specify this.
      // ATTN: For now, use a ridiculously small value, to force early
      //       failure in the near future, so we have to fix it
      retval = 4;
      goto cache_it;

    case RW_YANG_LEAF_TYPE_INSTANCE_ID:
      // ATTN: No length in yang.
      // ATTN: Maps to a restricted xpath expression, so determining a
      //       length from the schema would be challenging, but not
      //       absolutely impossible.  May be nice to have extension to
      //       specify this.
      // ATTN: For now, use a ridiculously small value, to force early
      //       failure in the near future, so we have to fix it
      retval = 4;
      goto cache_it;

    case RW_YANG_LEAF_TYPE_ANYXML:
      // ATTN: No length in yang.  But since we map to string, it would
      //       be nice to have an extension to define the length
      // ATTN: For now, use a ridiculously small value, to force early
      //       failure in the near future, so we have to fix it
      // ATTN: Currently unsupported, anyway
      retval = 4;
      goto cache_it;

    default:
      RW_ASSERT_NOT_REACHED();
  }

  RW_ASSERT(typ_);
  for (const typ_rangedef_t* rangedef = typ_first_rangedef(ncxmodel_.ncx_instance_, typ_);
       rangedef != nullptr;
       rangedef = (const typ_rangedef_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, rangedef)) {
    RW_ASSERT(NCX_BT_UINT32 == rangedef->btyp);
    retval = rangedef->ub.u;
  }

cache_it:
  return max_length_.locked_cache_set(&cache_state_, retval);
}

/*!
 * Get the value descriptor of the next value.  Descriptor is owned by
 * the library.  Caller may use the descriptor until model is
 * destroyed.
 */
YangValueNcx* YangValueNcx::get_next_value()
{
  return next_value_;
}

/*!
 * Determine if a value matches an input string.
 */
rw_status_t YangValueNcx::parse_value(const char* value_string)
{
  RW_ASSERT(value_string);
  if (val_simval_ok(ncxmodel_.ncx_instance_, typ_, (unsigned char *)value_string) == NO_ERR) {
    if (leaf_type_ == RW_YANG_LEAF_TYPE_BINARY) {
      // Additional check to verify whether the characters are base64 encoded.
      if (!rw_yang_is_valid_base64_string(value_string)) {
        return RW_STATUS_FAILURE;
      }
    }
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}

/*!
 * Get the first extension descriptor.  The descriptor is owned by the
 * model.  Will be nullptr if there are no extensions.  Caller may use
 * extension descriptor until model is destroyed.
 */
YangExtNcx* YangValueNcx::get_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  // ATTN: Can extensions change after a module load?

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_extension();
}

/*!
 * Update the model node for the first list extension.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangExtNcx* YangValueNcx::locked_update_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(typ_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &typ_->appinfoQ);
  if (nullptr != appinfo) {
    retval = ncxmodel_.locked_new_ext(appinfo);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  return first_extension_.locked_cache_set(&cache_state_, nullptr);
}


/*****************************************************************************/
/*!
 * Construct keyword-based value descriptor.
 */
YangValueNcxKeyword::YangValueNcxKeyword(
    YangModelNcxImpl& ncxmodel,
    YangTypeNcx* owning_type,
    typ_def_t* typ)
: YangValueNcx(ncxmodel,owning_type,typ)
{
}

/*!
 * Determine if a keyword-based value matches an input string.
 */
rw_status_t YangValueNcxKeyword::parse_value(const char* value_string)
{
  // ATTN: Must also accept prefix!
  // ATTN: Handle protobuf enum name mangling here, too?
  RW_ASSERT(value_string);
  if (0 == strcmp(name_,value_string)) {
    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}

/*!
 * Determine if value is a leading substring of the keyword.  Exact
 * match is considered a valid leading ubstring.
 */
rw_status_t YangValueNcxKeyword::parse_partial(const char* value_string)
{
  size_t i;
  for (i = 0; name_[i] != '\0' && value_string[i] != '\0' && name_[i] == value_string[i]; ++i) {
    ; // null loop
  }

  if (value_string[i] == '\0') {
    // exact match is also considered good
    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}

/*****************************************************************************/
/*!
 * Construct value descriptor for a single libncx boolean entry.
 */
YangValueNcxBool::YangValueNcxBool(
    YangModelNcxImpl & ncxmodel,
    YangTypeNcx * owning_type,
    typ_def_t * typ,
    bool val)
: YangValueNcxKeyword(ncxmodel, owning_type, typ)
{
  if (val) {
    vals_.push_back("true");
    vals_.push_back("1");
  } else {
    vals_.push_back("false");
    vals_.push_back("0");
  }
}

const char * YangValueNcxBool::get_name() {
  return vals_[0].c_str();
}

rw_status_t YangValueNcxBool::parse_partial(const char* value_string)
{
  const std::string value(value_string);

  for (auto valid = vals_.cbegin(); valid != vals_.cend(); ++valid) {
    if (valid->find(value) == 0)
      return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}

rw_status_t YangValueNcxBool::parse_value(const char * value_string)
{
  RW_ASSERT(value_string);
  const std::string value(value_string);

  for (auto valid = vals_.cbegin(); valid != vals_.cend(); ++valid) {
    if (*valid == value)
      return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}


/*****************************************************************************/
/*!
 * Construct value descriptor for a single libncx enum entry.
 */
YangValueNcxEnum::YangValueNcxEnum(
    YangModelNcxImpl& ncxmodel,
    YangTypeNcx* owning_type,
    typ_def_t* typ,
    typ_enum_t* typ_enum)
: YangValueNcxKeyword(ncxmodel,owning_type,typ),
  typ_enum_(typ_enum)
{
  RW_ASSERT(RW_YANG_LEAF_TYPE_ENUM == leaf_type_);
  name_ = (const char*)typ_enum_->name;

  if (nullptr != typ_enum_->descr) {
    description_ = (const char*)typ_enum_->descr;
  }

  // ATTN: What about prefix?
  // ATTN: What about ns?
}

/*!
 * Get the numerical value for an enumeration
 */
uint32_t YangValueNcxEnum::get_position()
{
  RW_ASSERT (typ_enum_);
  return typ_enum_->val;
}

YangExtNcx* YangValueNcxEnum::locked_update_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(typ_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &typ_enum_->appinfoQ);
  if (nullptr != appinfo) {
    retval = ncxmodel_.locked_new_ext(appinfo);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  return first_extension_.locked_cache_set(&cache_state_, nullptr);
}

/*****************************************************************************/
/*!
 * Construct value descriptor for a single libncx Identity Ref entry.
 */
YangValueNcxIdRef::YangValueNcxIdRef(
    YangModelNcxImpl& ncxmodel,
    YangTypeNcx* owning_type,
    typ_def_t* typ,
    ncx_identity_t_* typ_idref)
: YangValueNcxKeyword(ncxmodel,owning_type,typ),
  typ_idref_(typ_idref)
{
  RW_ASSERT(RW_YANG_LEAF_TYPE_IDREF == leaf_type_);
  RW_ASSERT(typ_idref_->base);
  RW_ASSERT(typ_idref_->name);
  RW_ASSERT(typ_idref_->name[0]);

  name_ = (const char*)typ_idref_->name;

  if (nullptr != typ_idref_->base->descr) {
    description_ = (const char*)typ_idref_->descr;
  }

  // Get the ns and prefix from the module in which the identity was defined
  RW_ASSERT(typ_idref->tkerr.mod);
  ncx_module_t* mod = typ_idref->tkerr.mod;

  if (mod->ns) {
    ns_ = (const char*)mod->ns;
  }
  if (mod->prefix) {
    prefix_ = (const char*)mod->prefix;
  }

}


/*****************************************************************************/
/*!
 * Construct value descriptor for a single libncx bits entry.
 */
YangValueNcxBits::YangValueNcxBits(
    YangModelNcxImpl& ncxmodel,
    YangTypeNcx* owning_type,
    typ_def_t* typ,
    typ_enum_t* typ_enum)
: YangValueNcxKeyword(ncxmodel,owning_type,typ),
  typ_enum_(typ_enum)
{
  RW_ASSERT(RW_YANG_LEAF_TYPE_BITS == leaf_type_);
  name_ = (const char*)typ_enum_->name;
  position_ = typ_enum_->pos;

  if (nullptr != typ_enum_->descr) {
    description_ = (const char*)typ_enum_->descr;
  }

  // ATTN: What about prefix?
  // ATTN: What about ns?
}

uint32_t YangValueNcxBits::get_position()
{
  return position_;
}


/*****************************************************************************/
/*!
 * Create a libncx-based yang model.
 */

YangModelNcx* YangModelNcx::create_model(void *trace_ctx)
{
  return new YangModelNcxImpl(trace_ctx);
}

// YangModel C APIs
rw_yang_model_t* rw_yang_model_create_libncx()
{
  return YangModelNcx::create_model();
}

// YangModel C APIs
rw_yang_model_t* rw_yang_model_create_libncx_with_trace(void *trace_ctx)
{
  return YangModelNcx::create_model(trace_ctx);
}


/*****************************************************************************/

unsigned YangModelNcxImpl::g_users;

YangModelNcxImpl::YangModelNcxImpl(void *trace_ctx)
: ncx_instance_(nullptr),
  first_module_(nullptr),
  root_node_(*this),
  type_anyxml_(*this)
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  ++g_users;

  // ATTN: Should this be a static init function?
  // ATTN: Select debug level at runtime
  ncx_instance_ = ncx_alloc();
  RW_ASSERT(ncx_instance_);
  status_t ncxst = ncx_init(
    ncx_instance_,
    TRUE/*savestr*/,
    LOG_DEBUG_INFO,
    FALSE/*logtstamps*/,
    "",
    0,
    nullptr);
  RW_ASSERT(NO_ERR == ncxst);

  // Call Custom init for Custom logging
  ncx_custom_init(ncx_instance_, this, trace_ctx, rw_ncx_logging_routine);

  //Turn off all the warnings.
  for (int warn = ERR_WARN_BASE; warn < ERR_LAST_WARN; warn++) {
    ncx_turn_off_warning(ncx_instance_, (status_t)warn);
  }

  RW_ASSERT(32 >=
    std::max( {
      NCX_CACHED_EXT_end,
      NCX_CACHED_VALUE_end,
      NCX_CACHED_TYPE_end,
      NCX_CACHED_NODE_end,
      NCX_CACHED_KEY_end,
      NCX_CACHED_MODULE_end } ) );

  /*
   * Life is really sad if 64-bit, write-many, cached values, or
   * atomic-intptr_t are not lock free.  So make sure they are.  N.B.,
   * If this fails for you some day, then you need to come up with a
   * new way to cache these things.
   */
  std::atomic<uint64_t> au64;
  RW_ASSERT(au64.is_lock_free());
  std::atomic<intptr_t> aip;
  RW_ASSERT(aip.is_lock_free());
}

YangModelNcxImpl::~YangModelNcxImpl()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  /*
   * Explicitly clear all containers now, to force destructors to run
   * inside the mutex and in a some sort of reasonable order.  N.B.,
   * there is no perfect order, because may of these objects have
   * circular references; we just have to assume that the destructors
   * don't make accesses into related objects.
   */
  typed_enums_map_.clear();
  modules_map_ncx_.clear();
  typed_enums_.clear();
  modules_.clear();
  augments_.clear();
  keys_.clear();
  nodes_.clear();
  values_.clear();
  types_.clear();
  extensions_.clear();

  // Cleanup libncx, too?
  RW_ASSERT(g_users);
  --g_users;
  if (0 == g_users) {
    ncx_cleanup(ncx_instance_);
  }
}

/*!
 * This must be done in a critical section because it will update
 * global variables and shared data structures in libncx.
 */
YangModuleNcx* YangModelNcxImpl::load_module(const char* module_name)
{
  RW_ASSERT(module_name);
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  // ATTN: What to do about versions?
  YangModuleNcx* loaded_module = locked_search_module(module_name);

  if (loaded_module) {
    if (!loaded_module->was_loaded_explicitly()) {
      loaded_module->locked_set_load_name(module_name);
    } else {
      RW_ASSERT(0 == strcmp(module_name, loaded_module->get_name()));
    }
  } else {
    ncx_module_t* new_mod = nullptr;
    status_t ncxst = ncxmod_load_module( ncx_instance_, (xmlChar*)module_name, nullptr, nullptr, &new_mod );
    // ATTN: error diagnostics would be good here
    if (NO_ERR == ncxst) {
      RW_ASSERT(new_mod);

      // Previously loaded via import?
      loaded_module = locked_search_module(new_mod);
      if (loaded_module) {
        RW_ASSERT(!loaded_module->was_loaded_explicitly());
        loaded_module->locked_set_load_name(module_name);
      } else {
        loaded_module = locked_new_module(new_mod, module_name);
      }
    }
  }

  locked_populate_import_modules();
  ++version_;

  return loaded_module;
}

YangModuleNcx* YangModelNcxImpl::get_first_module()
{
  return first_module_;
}

YangModuleNcx* YangModelNcxImpl::locked_search_module(
  ncx_module_t* mod)
{
  RW_ASSERT(mod);
  auto i = modules_map_ncx_.find(mod);
  if (i == modules_map_ncx_.end()) {
    return nullptr;
  }

  return i->second;
}

YangModuleNcx* YangModelNcxImpl::locked_search_module(
  const char* module_name)
{
  RW_ASSERT(module_name);
  ncx_module_t* mod = ncx_find_module(ncx_instance_, (const xmlChar*)module_name, nullptr );
  if (mod) {
    YangModuleNcx* found = locked_search_module(mod);

    // For auto-loaded modules, it may be known already, but not known
    // to yangncx.  Don't create it here - that will happen elsewhere.
    if (found) {
      return found;
    }
  }
  return nullptr;
}

YangModuleNcx* YangModelNcxImpl::locked_new_module(
  ncx_module_t* mod,
  const char* load_name)
{
  RW_ASSERT(mod);
  RW_ASSERT(load_name);
  RW_ASSERT(load_name[0] != '\0');
  YangModuleNcx* new_module_p = new YangModuleNcx(*this, mod, load_name);
  locked_save_module( mod, new_module_p );
  return new_module_p;
}

YangModuleNcx* YangModelNcxImpl::locked_new_module(
  ncx_module_t* mod)
{
  RW_ASSERT(mod);
  YangModuleNcx* new_module_p = new YangModuleNcx(*this, mod);
  locked_save_module( mod, new_module_p );
  return new_module_p;
}

void YangModelNcxImpl::locked_save_module(
  ncx_module_t* mod,
  YangModuleNcx* module)
{
  RW_ASSERT(mod);
  RW_ASSERT(module);

  // Save in both tracking structures.
  modules_.emplace_back(module);
  modules_map_ncx_.insert(std::make_pair(mod,module));

  // ATTN: Need to check for success!

  // ATTN: a throw on the map insert causes inconsistency!
}

void YangModelNcxImpl::locked_populate_import_modules()
{
  YangModuleNcx* last_module = nullptr;
  for (ncx_module_t* mod = ncx_get_first_module(ncx_instance_);
       nullptr != mod;
       mod = ncx_get_next_module(ncx_instance_, mod)) {
    bool newmod = false;
    YangModuleNcx* module = locked_search_module(mod);
    if (nullptr == module) {
      module = locked_new_module(mod);
      newmod = true;
    }

    if (last_module) {
      YangModuleNcx* next_module = last_module->get_next_module();
      if (next_module) {
        if (newmod) {
          // libncx inserted in the middle
          last_module->locked_set_next_module(module);
        } else {
          if (next_module != module) {
            // libncx inserted in the middle
            YangModuleNcx* my_next_module = module->get_next_module();
            RW_ASSERT(nullptr == my_next_module); // libncx changed the order?
            last_module->locked_set_next_module(module);
            module->locked_set_next_module(next_module);
          }
          // else, no change
        }
      } else {
        last_module->locked_set_next_module(module);
      }
    } else {
      if (first_module_) {
        if (module != first_module_) {
          // libncx inserted as new first module
          module->locked_set_next_module(first_module_);
          first_module_ = module;
        }
        // else, no change
      } else {
        first_module_ = module;
      }
    }
    last_module = module;
  }
}

YangNodeNcxRoot* YangModelNcxImpl::get_root_node()
{
  return &root_node_;
}

YangNodeNcx* YangModelNcxImpl::locked_new_node(
  YangModuleNcx* module,
  obj_template_t* obj)
{
  YangNodeNcx* new_node_p = new YangNodeNcx(*this, module, obj);
  node_ptr_t ptr(new_node_p);
  auto status = nodes_.emplace(obj, std::move(ptr));
  RW_ASSERT(status.second);
  module->locked_set_last_node(new_node_p);
  return new_node_p;
}

YangNodeNcx* YangModelNcxImpl::locked_new_node(
  YangNode* parent,
  obj_template_t* obj)
{
  YangNodeNcx* new_node_p = new YangNodeNcx(*this, parent, obj);
  node_ptr_t ptr(new_node_p);
  auto status = nodes_.emplace(obj, std::move(ptr));
  RW_ASSERT(status.second);
  return new_node_p;
}

YangNodeNcx* YangModelNcxImpl::locked_search_node(
  obj_template_t* obj)
{
  RW_ASSERT(obj);
  auto ni = nodes_.find(obj);
  if (ni == nodes_.end()) {
    return nullptr;
  }
  return ni->second.get();
}

YangNodeNcx* YangModelNcxImpl::locked_search_node_case_choice(
  obj_template_t* obj)
{
  RW_ASSERT(obj);
  auto ni = nodes_.find(obj);
  if (ni != nodes_.end()) {
    return ni->second.get();
  }
  auto cai = cases_.find(obj);
  if (cai != cases_.end()) {
    return cai->second.get();
  }
  auto chi = choices_.find(obj);
  if (chi != choices_.end()) {
    return chi->second.get();
  }
  return nullptr;
}

YangNodeNcx* YangModelNcxImpl::locked_populate_target_node(
  obj_template_t* targobj)
{
  RW_ASSERT(targobj);
  YangNodeNcx* target_node = locked_search_node_case_choice(targobj);
  if (target_node) {
    return target_node;
  }

  /*
   * Need to populate the target's parent node, and then iterate its
   * children in order to populate the target node.
   */
  YangNodeNcx* parent_node = nullptr;
  if (targobj->parent) {
    parent_node = locked_populate_target_node(targobj->parent);
  }

  // Search module roots?
  YangModuleNcx* module = nullptr;
  if (parent_node) {
    target_node = parent_node->get_first_child();
  } else {
    if (targobj->tkerr.mod) {
      module = locked_search_module(targobj->tkerr.mod);
    }
    if (nullptr == module && targobj->mod) {
      module = locked_search_module(targobj->mod);
    }
    RW_ASSERT(module);
    target_node = module->get_first_node();
  }

  if (!target_node) {
    return nullptr;
  }

  while (target_node) {
    if (target_node->obj_ == targobj) {
      return target_node;
    }
    // Populate and check choice/case hierarchy
    for (YangNodeNcx* c_target_node = target_node->get_case();
         c_target_node;
         c_target_node = c_target_node->get_case()) {
      if (c_target_node->obj_ == targobj) {
        return c_target_node;
      }
      c_target_node = c_target_node->get_choice();
      RW_ASSERT(c_target_node);
      if (c_target_node->obj_ == targobj) {
        return c_target_node;
      }
    }
    // Iterate to the next sibling; don't leave the target module
    target_node = target_node->get_next_sibling();
    if (module && target_node->get_module() != module) {
      break;
    }
  }

  RW_ASSERT_NOT_REACHED(); // don't want to handle this case - we need to figure this out
}

YangNodeNcx* YangModelNcxImpl::locked_new_choice(
  YangNode* xml_parent,
  YangModuleNcx* module,
  obj_template_t* obj)
{
  YangNodeNcx* new_choice_p;
  if (xml_parent) {
    new_choice_p = new YangNodeNcx(*this, xml_parent, obj);
  } else {
    new_choice_p = new YangNodeNcx(*this, module, obj);
  }
  node_ptr_t ptr(new_choice_p);
  auto status = choices_.emplace(obj, std::move(ptr));
  RW_ASSERT(status.second);
  return new_choice_p;
}

YangNodeNcx* YangModelNcxImpl::locked_search_choice(
  obj_template_t* obj)
{
  RW_ASSERT(obj);
  auto ni = choices_.find(obj);
  if (ni == choices_.end()) {
    return nullptr;
  }
  return ni->second.get();
}

YangNodeNcx* YangModelNcxImpl::locked_new_case(
  YangNode* xml_parent,
  YangModuleNcx* module,
  obj_template_t* obj)
{
  YangNodeNcx* new_case_p;
  if (xml_parent) {
    new_case_p = new YangNodeNcx(*this, xml_parent, obj);
  } else {
    new_case_p = new YangNodeNcx(*this, module, obj);
  }
  node_ptr_t ptr(new_case_p);
  auto status = cases_.emplace(obj, std::move(ptr));
  RW_ASSERT(status.second);
  return new_case_p;
}

YangNodeNcx* YangModelNcxImpl::locked_search_case(
  obj_template_t* obj)
{
  RW_ASSERT(obj);
  auto ni = cases_.find(obj);
  if (ni == cases_.end()) {
    return nullptr;
  }
  return ni->second.get();
}

YangType* YangModelNcxImpl::locked_get_type(
  obj_template_t* obj)
{
  if (obj->objtype == OBJ_TYP_ANYXML) {
    // Anyxml is treated specially by the model.
    return &type_anyxml_;
  }

  typ_def_t* typ = obj_get_typdef(obj);
  RW_ASSERT(typ);

  auto type_i = types_.find(typ);
  if (type_i != types_.end()) {
    // Already known; return it.
    return type_i->second.get();
  }

  // Create a new one.
  YangTypeNcx* new_type_p = new YangTypeNcx(*this, typ);
  type_ptr_t type_ptr(new_type_p);
  auto retval = types_.emplace(typ,std::move(type_ptr));
  RW_ASSERT(retval.second);

  return new_type_p;
}

YangTypedEnumNcx* YangModelNcxImpl::locked_search_typed_enum(
  typ_template_t* enum_typedef)
{
  RW_ASSERT(enum_typedef);
  auto i = typed_enums_map_.find(enum_typedef);
  if (i == typed_enums_map_.end()) {
    return nullptr;
  }

  return i->second;
}

YangTypedEnumNcx* YangModelNcxImpl::locked_new_typed_enum(
  typ_template_t* enum_typedef)
{
  YangTypedEnumNcx*new_enum = new YangTypedEnumNcx(*this, enum_typedef);
  typed_enums_.emplace_back(new_enum);
  typed_enums_map_.insert(std::make_pair(enum_typedef,new_enum));
  return new_enum;
}

YangValueNcx* YangModelNcxImpl::locked_new_value(
  YangTypeNcx* owning_type,
  typ_def_t* typ)
{
  YangValueNcx* new_value_p = new YangValueNcx(*this, owning_type, typ);
  values_.emplace_back(new_value_p);
  return new_value_p;
}

YangValueNcxBool * YangModelNcxImpl::locked_new_value_boolean(
  YangTypeNcx* owning_type,
  typ_def_t* typ,
  bool val)
{
  YangValueNcxBool * new_value_p = new YangValueNcxBool(*this, owning_type, typ, val);
  values_.emplace_back(new_value_p);
  return new_value_p;
}

YangValueNcxEnum* YangModelNcxImpl::locked_new_value_enum(
  YangTypeNcx* owning_type,
  typ_def_t* typ,
  typ_enum_t* typ_enum)
{
  YangValueNcxEnum* new_value_p = new YangValueNcxEnum(*this, owning_type, typ, typ_enum);
  values_.emplace_back(new_value_p);
  return new_value_p;
}

YangValueNcxIdRef* YangModelNcxImpl::locked_new_value_idref(
  YangTypeNcx* owning_type,
  typ_def_t* typ,
  ncx_identity_t_* typ_idref)
{
  YangValueNcxIdRef* new_value_p = new YangValueNcxIdRef(*this, owning_type, typ, typ_idref);
  values_.emplace_back(new_value_p);
  return new_value_p;
}

YangValueNcxBits* YangModelNcxImpl::locked_new_value_bits(
  YangTypeNcx* owning_type,
  typ_def_t* typ,
  typ_enum_t* typ_enum)
{
  YangValueNcxBits* new_value_p = new YangValueNcxBits(*this, owning_type, typ, typ_enum);
  values_.emplace_back(new_value_p);
  return new_value_p;
}

YangKeyNcx* YangModelNcxImpl::locked_new_key(
  YangNodeNcx* list_node,
  YangNodeNcx* key_node,
  obj_key_t* key)
{
  YangKeyNcx* new_key_p = new YangKeyNcx(*this, list_node, key_node, key);
  keys_.emplace_back(new_key_p);
  return new_key_p;
}

YangExtNcx* YangModelNcxImpl::locked_new_ext(
  ncx_appinfo_t* appinfo)
{
  YangExtNcx* new_ext_p = new YangExtNcx(*this, appinfo);
  extensions_.emplace_back(new_ext_p);
  return new_ext_p;
}

YangExtNcxNode* YangModelNcxImpl::locked_new_ext(
  ncx_appinfo_t* appinfo,
  obj_template_t* obj)
{
  YangExtNcxNode* new_ext_p = new YangExtNcxNode(*this, appinfo, obj);
  extensions_.emplace_back(new_ext_p);
  return new_ext_p;
}

YangExtNcxGrouping* YangModelNcxImpl::locked_new_ext(
  ncx_appinfo_t* appinfo,
  grp_template_t* grp)
{
  YangExtNcxGrouping* new_ext_p = new YangExtNcxGrouping(*this, appinfo, grp);
  extensions_.emplace_back(new_ext_p);
  return new_ext_p;
}

YangExtNcxType* YangModelNcxImpl::locked_new_ext(ncx_appinfo_t* appinfo, typ_def_t_* typ)
{
  YangExtNcxType* new_ext_p = new YangExtNcxType(*this, appinfo, typ);
  extensions_.emplace_back(new_ext_p);
  return new_ext_p;
}



YangAugmentNcx* YangModelNcxImpl::locked_search_augment(
  obj_template_t* obj_augment)
{
  auto ai = augments_.find(obj_augment);
  if (ai != augments_.end()) {
    return ai->second.get();
  }
  return nullptr;
}

YangAugmentNcx* YangModelNcxImpl::locked_new_augment(
  YangModuleNcx* module,
  YangNodeNcx* node,
  obj_template_t* obj_augment)
{
  YangAugmentNcx* new_augment_p = new YangAugmentNcx(*this, module, node, obj_augment);
  augment_ptr_t augment_ptr(new_augment_p);
  augments_.emplace(obj_augment, std::move(augment_ptr));
  return new_augment_p;
}

YangGroupingNcx* YangModelNcxImpl::locked_search_grouping(
  grp_template_t* grp)
{
  RW_ASSERT(grp);
  auto ni = groupings_.find(grp);
  if (ni == groupings_.end()) {
    return nullptr;
  }
  return ni->second.get();
}

YangGroupingNcx* YangModelNcxImpl::locked_new_grouping(
  YangModuleNcx* module,
  grp_template_t* grp)
{
  YangGroupingNcx* new_grouping_p = new YangGroupingNcx(*this, module, grp);
  grp_ptr_t ptr(new_grouping_p);
  auto status = groupings_.emplace(grp, std::move(ptr));
  RW_ASSERT(status.second);

  module->locked_set_last_grouping(new_grouping_p);

  return new_grouping_p;
}

YangUsesNcx* YangModelNcxImpl::locked_search_uses(
  obj_template_t* uses)
{
  RW_ASSERT(uses);
  auto i = uses_.find(uses);
  if (i == uses_.end()) {
    return nullptr;
  }
  return i->second.get();
}

YangUsesNcx* YangModelNcxImpl::locked_new_uses(
  YangNode* node,
  obj_template_t* uses)
{
  YangUsesNcx* new_uses_p = new YangUsesNcx(*this, node, uses);
  uses_ptr_t ptr(new_uses_p);
  auto status = uses_.emplace(uses, std::move(ptr));
  RW_ASSERT(status.second);

  return new_uses_p;
}

bool YangModelNcxImpl::app_data_get_token(
  const char* ns,
  const char* ext,
  AppDataTokenBase::deleter_f deleter,
  AppDataTokenBase* token)
{
  RW_ASSERT(ns);
  RW_ASSERT(ext);
  RW_ASSERT(deleter);
  RW_ASSERT(token);

  return (app_data_mgr_.app_data_get_token(ns, ext, deleter, token));
}

void YangModelNcxImpl::set_log_level(rw_yang_log_level_t level)
{
  log_debug_t ncx_level;

  switch (level) {
    default:
    case RW_YANG_LOG_LEVEL_NONE:
      ncx_level = LOG_DEBUG_NONE;
      break;

    case RW_YANG_LOG_LEVEL_ERROR:
      ncx_level = LOG_DEBUG_ERROR;
      break;

    case RW_YANG_LOG_LEVEL_WARN:
      ncx_level = LOG_DEBUG_WARN;
      break;

    case RW_YANG_LOG_LEVEL_INFO:
      ncx_level = LOG_DEBUG_INFO;
      break;

    case RW_YANG_LOG_LEVEL_DEBUG:
      ncx_level = LOG_DEBUG_DEBUG;
      break;

    case RW_YANG_LOG_LEVEL_DEBUG2:
      ncx_level = LOG_DEBUG_DEBUG2;
      break;

    case RW_YANG_LOG_LEVEL_DEBUG3:
      ncx_level = LOG_DEBUG_DEBUG3;
      break;

    case RW_YANG_LOG_LEVEL_DEBUG4:
      ncx_level = LOG_DEBUG_DEBUG4;
      break;
  }

  log_set_debug_level(ncx_instance_, ncx_level);
}

rw_yang_log_level_t YangModelNcxImpl::log_level() const
{
  log_debug_t ncx_level = log_get_debug_level(ncx_instance_);

  switch (ncx_level) {
    default:
    case LOG_DEBUG_NONE:
      return RW_YANG_LOG_LEVEL_NONE;

    case LOG_DEBUG_ERROR:
      return RW_YANG_LOG_LEVEL_ERROR;

    case LOG_DEBUG_WARN:
      return RW_YANG_LOG_LEVEL_WARN;

    case LOG_DEBUG_INFO:
      return RW_YANG_LOG_LEVEL_INFO;

    case LOG_DEBUG_DEBUG:
      return RW_YANG_LOG_LEVEL_DEBUG;

    case LOG_DEBUG_DEBUG2:
      return RW_YANG_LOG_LEVEL_DEBUG2;

    case LOG_DEBUG_DEBUG3:
      return RW_YANG_LOG_LEVEL_DEBUG3;

    case LOG_DEBUG_DEBUG4:
      return RW_YANG_LOG_LEVEL_DEBUG4;
  }
}


/*****************************************************************************/
/*!
 * Construct key descriptor for a single libncx list key entry.
 */
YangKeyNcx::YangKeyNcx(
    YangModelNcxImpl& ncxmodel,
    YangNodeNcx* list_node,
    YangNodeNcx* key_node,
    obj_key_t* key)
: ncxmodel_(ncxmodel),
  cache_state_(),
  key_(key),
  list_node_(list_node),
  key_node_(key_node),
  next_key_()
{
  RW_ASSERT(list_node_);
  RW_ASSERT(key_node_);
  RW_ASSERT(key_);
  RW_ASSERT(RW_YANG_STMT_TYPE_LIST == list_node_->get_stmt_type());
  RW_ASSERT(RW_YANG_STMT_TYPE_LEAF == key_node_->get_stmt_type());
}

/*!
 * Get the owning list node.  Node owned by the library.  Caller may
 * use node until model is destroyed.
 */
YangNodeNcx* YangKeyNcx::get_list_node()
{
  RW_ASSERT(list_node_);
  return list_node_;
}

/*!
 * Get the key's (intermediate) node.  Node owned by the library.
 * Caller may use node until model is destroyed.
 */
YangNodeNcx* YangKeyNcx::get_key_node()
{
  RW_ASSERT(key_node_);
  return key_node_;
}

/*!
 * Get the next key descriptor in a list of key.  Descriptor owned by
 * the library.  Caller may use descriptor until model is destroyed.
 */
YangKeyNcx* YangKeyNcx::get_next_key()
{
  YangKeyNcx* retval;
  if (next_key_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_next_key();
}

/*!
 * Update the key for the next key.  ASSUMES THE MUTEX IS LOCKED.
 */
YangKeyNcx* YangKeyNcx::locked_update_next_key()
{
  YangKeyNcx* retval;
  if (next_key_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(list_node_);
  RW_ASSERT(key_);
  obj_key_t* key = obj_next_key(ncxmodel_.ncx_instance_, key_);
  if (nullptr == key) {
    // Has no next key.
    return next_key_.locked_cache_set(&cache_state_, nullptr);
  }

  obj_template_t* key_obj = key->keyobj;
  RW_ASSERT(nullptr != key_obj);
  RW_ASSERT(key_obj->parent == list_node_->obj_); // no deep keys allowed by yang

  // Find the child_node node that defines the key.  This may populate
  // the entire child list.  Oh well.

  // The keys are not required to be in the same order in the list as in the key
  // statement - The traversal has to start from the begining
  for( YangNodeNcx* sibling_node = list_node_->get_first_child();
       nullptr != sibling_node;
       sibling_node = sibling_node->get_next_sibling() ) {
    if (key_obj == sibling_node->obj_) {
      // Found it
      retval = ncxmodel_.locked_new_key(list_node_, sibling_node, key);
      RW_ASSERT(sibling_node->is_key_);
      return next_key_.locked_cache_set(&cache_state_, retval);
    }
  }

  // Didn't find the key node?  Huh?
  RW_ASSERT_NOT_REACHED();
}


/*****************************************************************************/
YangAugmentNcx::YangAugmentNcx(
    YangModelNcxImpl& ncxmodel,
    YangModuleNcx* module,
    YangNodeNcx* target_node,
    obj_template_t* obj_augment)
: ncxmodel_(ncxmodel),
  obj_augment_(obj_augment),
  module_(module),
  target_node_(target_node)
{
  RW_ASSERT(obj_augment_);
  RW_ASSERT(module_);
  RW_ASSERT(target_node_);

  switch (target_node_->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_LIST:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF:
      break;

    default:
      RW_ASSERT_NOT_REACHED(); // should be impossible
  }
}

std::string YangAugmentNcx::get_location()
{
  RW_ASSERT(obj_augment_);
  RW_ASSERT(obj_augment_->tkerr.mod);
  RW_ASSERT(obj_augment_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << obj_augment_->tkerr.mod->sourcefn << ':' << obj_augment_->tkerr.linenum << ':' << obj_augment_->tkerr.linepos;
  return oss.str();
}

YangModule* YangAugmentNcx::get_module()
{
  RW_ASSERT(module_);
  return module_;
}

YangNodeNcx* YangAugmentNcx::get_target_node()
{
  RW_ASSERT(target_node_);
  return target_node_;
}

YangAugmentNcx* YangAugmentNcx::get_next_augment()
{
  YangAugmentNcx* retval;
  if (next_augment_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_next_augment();
}

YangExtNcx* YangAugmentNcx::get_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_extension();
}

YangAugmentNcx* YangAugmentNcx::locked_update_next_augment()
{
  YangAugmentNcx* retval = nullptr;
  bool already_cached = next_augment_.is_cached_and_get(cache_state_, &retval);
  if (already_cached) {
    return retval;
  }

  // Only module-level augments have a next.
  RW_ASSERT(obj_augment_->parent);
  if (OBJ_TYP_USES == obj_augment_->parent->objtype) {
    return next_augment_.locked_cache_set(&cache_state_, nullptr);
  }

  obj_template_t* sib_obj_augment = obj_augment_;
  while (1) {
    sib_obj_augment = (obj_template_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, sib_obj_augment);

    if (nullptr == sib_obj_augment) {
      return next_augment_.locked_cache_set(&cache_state_, nullptr);
    }

    if (sib_obj_augment->objtype == OBJ_TYP_AUGMENT) {
      break;
    }
  }

  // Find and populate the target node.
  obj_augment_t* sib_augment = sib_obj_augment->def.augment;
  RW_ASSERT(sib_augment);
  obj_template_t* targobj = sib_augment->targobj;
  RW_ASSERT(targobj);
  YangNodeNcx* target_node = ncxmodel_.locked_populate_target_node(targobj);

  // Create the augment descriptor.
  retval = ncxmodel_.locked_new_augment(module_, target_node, sib_obj_augment);
  return next_augment_.locked_cache_set(&cache_state_, retval);
}

YangExtNcx* YangAugmentNcx::locked_update_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(obj_augment_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &obj_augment_->appinfoQ);
  if (nullptr != appinfo) {
    retval = ncxmodel_.locked_new_ext(appinfo);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  return first_extension_.locked_cache_set(&cache_state_, nullptr);
}


/*****************************************************************************/
/*!
 * Construct a yang extension descriptor for libncx.
 */
YangExtNcx::YangExtNcx(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo)
: ncxmodel_(ncxmodel),
  appinfo_(appinfo),
  ns_(nullptr),
  next_extension_()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  RW_ASSERT(appinfo_);
  RW_ASSERT(appinfo_->ext);

  xmlns_id_t nsid = appinfo_->ext->nsid;
  RW_ASSERT(nsid);
  ns_ = (const char*)xmlns_get_ns_name(ncxmodel_.ncx_instance_, nsid);
  RW_ASSERT(ns_);
  RW_ASSERT(ns_[0]);
}

/*!
 * Get the source file location of the extension invocation.
 */
std::string YangExtNcx::get_location()
{
  RW_ASSERT(appinfo_);
  RW_ASSERT(appinfo_->tkerr.mod);
  RW_ASSERT(appinfo_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << appinfo_->tkerr.mod->sourcefn << ':' << appinfo_->tkerr.linenum << ':' << appinfo_->tkerr.linepos;
  return oss.str();
}

/*!
 * Get the source file location of the extension definition.
 */
std::string YangExtNcx::get_location_ext()
{
  RW_ASSERT(appinfo_);
  RW_ASSERT(appinfo_->ext);
  RW_ASSERT(appinfo_->ext->tkerr.mod);
  RW_ASSERT(appinfo_->ext->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << appinfo_->ext->tkerr.mod->sourcefn << ':' << appinfo_->ext->tkerr.linenum << ':' << appinfo_->ext->tkerr.linepos;
  return oss.str();
}

/*!
 * Get the value of the extension.  Buffer owned by the library.
 * String is cached.  Caller may use string until model is destroyed.
 */
const char* YangExtNcx::get_value()
{
  RW_ASSERT(appinfo_);
  return (const char*)appinfo_->value;
}

/*!
 * Get the description for the extension definition.  May be empty.
 * Buffer owned by libncx.  String is cached.  Caller may use string
 * until model is destroyed.
 */
const char* YangExtNcx::get_description_ext()
{
  RW_ASSERT(appinfo_);
  RW_ASSERT(appinfo_->ext);
  if (appinfo_->ext->descr) {
    return (const char*)appinfo_->ext->descr;
  }
  return "";
}

/*!
 * Get the name of the extension.  Buffer owned by the library.  String
 * is cached.  Caller may use string until model is destroyed.
 */
const char* YangExtNcx::get_name()
{
  RW_ASSERT(appinfo_);
  return (const char*)appinfo_->name;
}

/*!
 * Get the module name of the extension definition.  Buffer owned by
 * libncx.  String is cached.  Caller may use string until model is
 * destroyed.
 */
const char* YangExtNcx::get_module_ext()
{
  RW_ASSERT(appinfo_);
  RW_ASSERT(appinfo_->ext);
  RW_ASSERT(appinfo_->ext->tkerr.mod);
  RW_ASSERT(appinfo_->ext->tkerr.mod->name);
  RW_ASSERT(appinfo_->ext->tkerr.mod->name[0]);
  return (const char*)appinfo_->ext->tkerr.mod->name;
}

/*!
 * Get the prefix of the extension, as used in the yang source.  YOU
 * PROBABLY WANT get_module() WHEN SEARCHING FOR A SPECIFIC EXTENSION!
 * Buffer owned by the library.  String is cached.  Caller may use
 * string until model is destroyed.
 */
const char* YangExtNcx::get_prefix()
{
  RW_ASSERT(appinfo_);
  return (const char*)appinfo_->prefix;
}


/*!
 * Get the full namespace name for the extension.  Buffer owned by the
 * library.  String is cached until version changes.  Caller may use
 * string until model is destroyed.  ATTN: Is this the extension
 * definition, or the use?
 */
const char* YangExtNcx::get_ns()
{
  return ns_;
}

/*!
 * Get the descriptor of the next extension.  Descriptor is owned by
 * the library.  Caller may use descriptor until model is destroyed.
 */
YangExtNcx* YangExtNcx::get_next_extension()
{
  YangExtNcx* retval;
  if (next_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_next_extension();
}

/*!
 * Update the descriptor to find the next extension.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangExtNcx* YangExtNcx::locked_update_next_extension()
{
  YangExtNcx* retval;
  if (next_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(appinfo_);
  ncx_appinfo_t* sib_appinfo = (ncx_appinfo_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, appinfo_);
  if (nullptr == sib_appinfo) {
    // There is no next extension.
    return next_extension_.locked_cache_set(&cache_state_, nullptr);
  }

  retval = ncxmodel_.locked_new_ext(sib_appinfo);
  return next_extension_.locked_cache_set(&cache_state_, retval);
}


/*****************************************************************************/
/*!
 * Construct a node-based yang extension descriptor for libncx.
 */
YangExtNcxNode::YangExtNcxNode(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo, obj_template_t* obj)
: YangExtNcx(ncxmodel,appinfo),
  obj_(obj)
{
  RW_ASSERT(obj_);
}

/*!
 * Update the descriptor to find the next extension.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangExtNcx* YangExtNcxNode::locked_update_next_extension()
{
  YangExtNcx* retval;
  if (next_extension_.is_cached_and_get(cache_state_,&retval)) {
    return retval;
  }

  RW_ASSERT(appinfo_);
  ncx_appinfo_t* sib_appinfo = (ncx_appinfo_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, appinfo_);
  if (nullptr != sib_appinfo) {
    // There next extension.
    retval = ncxmodel_.locked_new_ext(sib_appinfo, obj_);
    return next_extension_.locked_cache_set(&cache_state_, retval);
  }

  RW_ASSERT(obj_);
  if (nullptr == obj_->usesobj) {
    // No uses or grouping to look in
    return next_extension_.locked_cache_set(&cache_state_, nullptr);
  }

  // No mode extensions on the current list.  Which list are we looking at?
  RW_ASSERT(OBJ_TYP_USES == obj_->usesobj->objtype);
  if (appinfo_ == (ncx_appinfo_t*)dlq_lastEntry(ncxmodel_.ncx_instance_, &obj_->appinfoQ)) {
    // Try the uses' list.
    sib_appinfo = (ncx_appinfo_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &obj_->usesobj->appinfoQ);
    if (nullptr != sib_appinfo) {
      // Start walking the uses extensions.
      retval = ncxmodel_.locked_new_ext(sib_appinfo, obj_);
      return next_extension_.locked_cache_set(&cache_state_, retval);
    }
  }

  RW_ASSERT(obj_->usesobj->def.uses);
  RW_ASSERT(obj_->usesobj->def.uses->grp);
  if (appinfo_ == (ncx_appinfo_t*)dlq_lastEntry(ncxmodel_.ncx_instance_, &obj_->usesobj->def.uses->grp->appinfoQ)) {
    // Done.
    return next_extension_.locked_cache_set(&cache_state_, nullptr);
  }

  sib_appinfo = (ncx_appinfo_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &obj_->usesobj->def.uses->grp->appinfoQ);
  if (nullptr != sib_appinfo) {
    // Start walking the uses extensions.
    retval = ncxmodel_.locked_new_ext(sib_appinfo, obj_);
    return next_extension_.locked_cache_set(&cache_state_, retval);
  }

  return next_extension_.locked_cache_set(&cache_state_, nullptr);
}

YangExtNcxType::YangExtNcxType(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo, typ_def_t_ *typ)
: YangExtNcx(ncxmodel,appinfo),
    typ_(typ)
{
  RW_ASSERT(typ_);
}

/**
+ * Update the descriptor to find the next extension.  ASSUMES THE MUTEX
+ * IS LOCKED.
+ */
YangExtNcx* YangExtNcxType::locked_update_next_extension()
{
  YangExtNcx* retval;
  if (next_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(appinfo_);
  ncx_appinfo_t* sib_appinfo = (ncx_appinfo_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, appinfo_);
  if (nullptr != sib_appinfo) {
    // There next extension.
    retval = ncxmodel_.locked_new_ext(sib_appinfo, typ_);
    return next_extension_.locked_cache_set(&cache_state_, retval);
  }

  RW_ASSERT(typ_);
  if (NCX_CL_NAMED != typ_->tclass) {
    return next_extension_.locked_cache_set(&cache_state_, nullptr);
  }

  // check for any extensions inside named types type-definiton.
  if (typ_->def.named.typ) {
    if (appinfo_ == (ncx_appinfo_t*)dlq_lastEntry(ncxmodel_.ncx_instance_, &typ_->def.named.typ->typdef.appinfoQ)) {
      return next_extension_.locked_cache_set(&cache_state_, nullptr);
    }

    sib_appinfo = (ncx_appinfo_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &typ_->def.named.typ->typdef.appinfoQ);
    if (nullptr != sib_appinfo) {
      retval = ncxmodel_.locked_new_ext(sib_appinfo, typ_);
      return next_extension_.locked_cache_set(&cache_state_, retval);
    }
  }

  return next_extension_.locked_cache_set(&cache_state_, nullptr);
}

/*****************************************************************************/
/*!
 * The typedef object that defines the module level type-def-ed enumerations
 */
YangTypedEnumNcx::YangTypedEnumNcx(YangModelNcxImpl& ncxmodel, typ_template_t *typ)
: ncxmodel_(ncxmodel),
  cache_state_(),
  typedef_(typ),
  ns_(nullptr),
  name_(nullptr),
  description_(nullptr),
  next_typed_enum_(),
  first_enum_value_(),
  first_extension_()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  RW_ASSERT (typ);

  xmlns_id_t nsid = typ_get_nsid(ncxmodel_.ncx_instance_, typ);
  if (nsid) {
    // local typedefs do not have nsid set.
    ns_ = (const char*)xmlns_get_ns_name(ncxmodel_.ncx_instance_, nsid);
    RW_ASSERT(ns_);
    RW_ASSERT(ns_[0]);
  }

  name_ = (const char *)typ->name;
  RW_ASSERT (name_);

  description_ = (const char *)typ->descr;
  if (nullptr == description_) {
    description_ = "";
  }
}

/*!
 * Destructor for libncx typed enum node
 */
YangTypedEnumNcx::~YangTypedEnumNcx()
{
}

const char *YangTypedEnumNcx::get_description()
{
  return description_;
}

const char * YangTypedEnumNcx::get_name()
{
  return name_;
}

const char* YangTypedEnumNcx::get_ns()
{
  return ns_;
}

std::string YangTypedEnumNcx::get_location()
{
  RW_ASSERT(typedef_);
  RW_ASSERT(typedef_->tkerr.mod);
  RW_ASSERT(typedef_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << typedef_->tkerr.mod->sourcefn << ':' << typedef_->tkerr.linenum << ':' << typedef_->tkerr.linepos;
  return oss.str();
}

/*!
 * Get the next typed enum at the module level
 */
YangTypedEnumNcx* YangTypedEnumNcx::get_next_typed_enum()
{
  YangTypedEnumNcx *retval;

  if (next_typed_enum_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_next_typedef();
}

/*!
 * Update the next typed enum. Assumes that locks have been taken
 */
YangTypedEnumNcx* YangTypedEnumNcx::locked_update_next_typedef()
{
  YangTypedEnumNcx *retval = 0;

  if (next_typed_enum_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT (typedef_);

  typ_template_t *typ_tmpl = (typ_template_t*) dlq_nextEntry(ncxmodel_.ncx_instance_, typedef_);

  while (typ_tmpl != NULL) {
    if ((typ_tmpl->typdef.tclass == NCX_CL_SIMPLE) &&
        (typ_tmpl->typdef.def.simple.btyp == NCX_BT_ENUM)) {
      break;
    }
    typ_tmpl = (typ_template_t*) dlq_nextEntry(ncxmodel_.ncx_instance_, typ_tmpl);
  }

  if (nullptr == typ_tmpl) {
    // no more typedefs
    return next_typed_enum_.locked_cache_set (&cache_state_, nullptr);
  }

  retval = ncxmodel_.locked_search_typed_enum (typ_tmpl);
  if (nullptr == retval) {
    retval = ncxmodel_.locked_new_typed_enum (typ_tmpl);
  }

  RW_ASSERT (retval);
  return next_typed_enum_.locked_cache_set (&cache_state_, retval);
}

YangValue* YangTypedEnumNcx::get_first_enum_value()
{
  YangValue* retval = nullptr;
  if (first_enum_value_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  // ATTN: Can values change after a module load?
  //   What about new identities or enum values?

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_enum_value();
}

YangValue* YangTypedEnumNcx::locked_update_first_enum_value()
{
  YangValue* retval = nullptr;
  if (first_enum_value_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(typedef_);
  typ_def_t* typ = &typedef_->typdef;
  RW_ASSERT(typ);
  YangValueNcxEnum* previous = nullptr;

  // Iterate over value list...
  for (typ_enum_t* enum_value = typ_first_enumdef(ncxmodel_.ncx_instance_, typ);
       nullptr != enum_value;
       enum_value = typ_next_enumdef(ncxmodel_.ncx_instance_, enum_value)) {
    YangValueNcxEnum* value_p = ncxmodel_.locked_new_value_enum(nullptr, typ, enum_value);
    if (!retval) {
      retval = value_p;
    }
    if (previous) {
      previous->next_value_ = value_p;
    }
    previous = value_p;
  }

  return first_enum_value_.locked_cache_set(&cache_state_, retval);
}

/*!
 * Get the first extension descriptor.  The descriptor is owned by the
 * model.  Will be nullptr if there are no extensions.  Caller may use
 * extension descriptor until model is destroyed.
 */
YangExtNcx* YangTypedEnumNcx::get_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_extension();
}

/*!
 * Update the model node for the first list extension.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangExtNcx* YangTypedEnumNcx::locked_update_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(typedef_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &typedef_->typdef.appinfoQ);
  if (nullptr != appinfo) {
    retval = ncxmodel_.locked_new_ext(appinfo);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  return first_extension_.locked_cache_set(&cache_state_, nullptr);
}



/*****************************************************************************/
/*!
 * Construct a yang module for libncx.  Because load_name is passed,
 * this module was loaded explicitly.
 */
YangModuleNcx::YangModuleNcx(YangModelNcxImpl& ncxmodel, ncx_module_t* mod, std::string load_name)
: ncxmodel_(ncxmodel),
  version_(ncxmodel),
  mod_(mod),
  load_name_(load_name),
  first_node_(),
  last_node_(),
  next_module_(),
  first_extension_(),
  first_augment_(),
  imported_modules_(),
  import_count_(0),
  first_grouping_(),
  last_grouping_(),
  app_data_(ncxmodel.app_data_mgr_)
{
  RW_ASSERT(mod_);
  cache_state_.locked_cache_set_flag_only(NCX_CACHED_MODULE_WAS_LOADED);
}

/*!
 * Construct a yang module for libncx.  Because load_name was not
 * passed, this module was not loaded explicitly.
 */
YangModuleNcx::YangModuleNcx(YangModelNcxImpl& ncxmodel, ncx_module_t* mod)
: ncxmodel_(ncxmodel),
  version_(ncxmodel),
  mod_(mod),
  first_node_(),
  last_node_(),
  next_module_(),
  import_count_(0),
  app_data_(ncxmodel.app_data_mgr_)
{
  RW_ASSERT(mod_);
}

/*!
 * Get the source file location of the module definition.
 */
std::string YangModuleNcx::get_location()
{
  RW_ASSERT(mod_);
  if (mod_->source) {
    return (const char*)mod_->source;
  } else {
    return "<internal>";
  }
}

/*!
 * Get the description for the module.  May be empty.  Buffer owned by
 * libncx.  String is cached.  Caller may use string until model is
 * destroyed.
 */
const char* YangModuleNcx::get_description()
{
  RW_ASSERT(mod_);
  if (mod_->descr) {
    return (const char*)mod_->descr;
  } else {
    return "";
  }
}

/*!
 * Get the name of the module.  Buffer owned by the library.  String is
 * cached.  Caller may use string until model is destroyed.
 */
const char* YangModuleNcx::get_name()
{
  RW_ASSERT(mod_);
  RW_ASSERT(mod_->name);
  return (const char*)mod_->name;
}

/*!
 * Get the namespace prefix for the module.  Buffer owned by the library.
 * String is cached until version changes.  Caller may use string until
 * model is destroyed.
 */
const char* YangModuleNcx::get_prefix()
{
  RW_ASSERT(mod_);
  RW_ASSERT(mod_->prefix);
  return (const char*)mod_->prefix;
}

/*!
 * Get the full namespace name for the module.  Buffer owned by the
 * library.  String is cached until version changes.  Caller may use
 * string until model is destroyed.
 */
const char* YangModuleNcx::get_ns()
{
  RW_ASSERT(mod_);
  RW_ASSERT(mod_->ns);
  return (const char*)mod_->ns;
}

/*!
 * Get the next module.  Module is owned by the library.  Caller may
 * use module until model is destroyed.  The attributes of the module
 * may change after future module loads, so the caller should not cache
 * data from the module.
 */
YangModuleNcx* YangModuleNcx::get_next_module()
{
  return next_module_;
}

/*!
   Determine if the module was loaded explicitly.
 */
bool YangModuleNcx::was_loaded_explicitly()
{
  return cache_state_.is_cached(NCX_CACHED_MODULE_WAS_LOADED);
}

/*!
 * Get the first child node.  Node descriptor is owned by the model.
 * Will be nullptr if there are no children.  Caller may use node until
 * model is destroyed.
 */
YangNodeNcx* YangModuleNcx::get_first_node()
{
  YangNodeNcx* retval = nullptr;
  if (   version_.is_up_to_date(ncxmodel_)
      && first_node_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (!version_.is_up_to_date(ncxmodel_)) {
    locked_update_version();
  }
  if (first_node_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  return locked_update_first_node();
}

/*!
 * Get an iterator to the last (known) top-level node of this module.
 * The node may change while iterating, so the caller should NOT cache
 * the result!
 */
YangNodeIter YangModuleNcx::node_end()
{
  YangNodeNcx* last_node = nullptr;
  if (   !version_.is_up_to_date(ncxmodel_)
      || !last_node_.is_cached_and_get(cache_state_, &last_node)) {
    GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
    if (!version_.is_up_to_date(ncxmodel_)) {
      locked_update_version();
    }

    // Called for side-effects only.
    locked_update_first_node();

    last_node_.is_cached_and_get(cache_state_, &last_node);
  }

  // At this point, last_node is either correct, or nullptr.
  if (last_node) {
    return ++YangNodeIter(last_node);
  }

  return YangNodeIter();
}

/*!
 * Get the first extension descriptor.  The descriptor is owned by the
 * model.  Will be nullptr if there are no extensions.  Caller may use
 * extension descriptor until model is destroyed.
 */
YangExtNcx* YangModuleNcx::get_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_extension();
}

/*!
 * Get the first augment descriptor.  The descriptor is owned by the
 * model.  Will be nullptr if there are no augments.  Caller may use
 * augment descriptor until model is destroyed.
 */
YangAugmentNcx* YangModuleNcx::get_first_augment()
{
  YangAugmentNcx* retval = nullptr;
  if (first_augment_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_augment();
}

bool YangModuleNcx::app_data_is_cached(const AppDataTokenBase* token) const noexcept
{
  return app_data_.is_cached(token);
}

bool YangModuleNcx::app_data_is_looked_up(const AppDataTokenBase* token) const noexcept
{
  return app_data_.is_looked_up(token);
}

bool YangModuleNcx::app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept
{
  return app_data_.check_and_get(token, data);
}

bool YangModuleNcx::app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data)
{
  // Trivial case is that it was already looked up...
  if (app_data_.is_looked_up(token)) {
    return app_data_.check_and_get(token, data);
  }

  // Need to take the lock so that we don't race another lookup
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  if (app_data_.is_looked_up(token)) {
    // raced someone else
    return app_data_.check_and_get(token, data);
  }

  // Search for it...  Don't search if user gave us empty strings.
  if (token->ns_.length() && token->ext_.length()) {
    YangExtension* yext = search_extension(token->ns_.c_str(), token->ext_.c_str());
    if (yext) {
      const char* ext_value = yext->get_value();
      intptr_t intptr_value = reinterpret_cast<intptr_t>(ext_value);
      intptr_t old_value = app_data_set_and_keep_ownership(token, intptr_value);
      RW_ASSERT(0 == old_value);
      *data = intptr_value;
      return true;
    }
  }
  app_data_.set_looked_up(token);
  return false;
}

void YangModuleNcx::app_data_set_looked_up(const AppDataTokenBase* token)
{
  app_data_.set_looked_up(token);
}

intptr_t YangModuleNcx::app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data)
{
  return app_data_.set_and_give_ownership(token, data);
}

intptr_t YangModuleNcx::app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data)
{
  return app_data_.set_and_keep_ownership(token, data);
}

void YangModuleNcx::mark_imports_explicit()
{
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  for (YangImportIter it = import_begin(); it != import_end(); it++) {
    YangModuleNcx *mod = dynamic_cast <YangModuleNcx*> (&(*it));
    RW_ASSERT (mod);

    mod->cache_state_.locked_cache_set_flag_only(NCX_CACHED_MODULE_WAS_LOADED);
  }
}

YangModel *YangModuleNcx::get_model()
{
  return &ncxmodel_;
}

/*!
 * Get the first group descriptor defined in the module.  The descriptor is owned by the
 * model.  Will be nullptr if there are groupings defined in the module.  Caller may use
 * grouping descriptor until model is destroyed.
 */
YangNode* YangModuleNcx::get_first_grouping()
{
  YangGroupingNcx* retval = nullptr;
  if (first_grouping_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  // ATTN: Can extensions change after a module load?

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_grouping();
}

/*!
 * Get the imported module. The descriptor is owned by the
 * model.  Will be nullptr if there are no imported modules.  Caller may use
 * extension descriptor until model is destroyed.
 */
YangModule** YangModuleNcx::get_first_import()
{
  YangModule** retval = NULL;
  if (imported_modules_.is_cached_and_get(cache_state_, &retval)) {
    return (retval);
  }
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  return (locked_update_first_import());
}


/*!
 * Get the first typed enum at the top level. could be null
 */
YangTypedEnum* YangModuleNcx::get_first_typed_enum()
{
  YangTypedEnumNcx* retval = nullptr;
  if (first_typedef_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_first_typedef();
}

void YangModuleNcx::locked_update_version()
{
  if (version_.is_up_to_date(ncxmodel_)) {
    return;
  }
  if (first_node_.is_cached(cache_state_)) {
    locked_update_first_node();
  }
  version_.updated(ncxmodel_);
}

void YangModuleNcx::locked_set_load_name(const std::string& load_name)
{
  load_name_ = load_name;
  cache_state_.locked_cache_set_flag_only(NCX_CACHED_MODULE_WAS_LOADED);
}

void YangModuleNcx::locked_set_last_node(YangNodeNcx* node)
{
  YangNodeNcx* cached_first = nullptr;
  bool first_is_cached = first_node_.is_cached_and_get(cache_state_, &cached_first);

  if (!first_is_cached || nullptr == cached_first) {
    first_node_.locked_cache_set(&cache_state_, node);
  }
  last_node_.locked_cache_set(&cache_state_, node);
}

void YangModuleNcx::locked_set_next_module(YangModuleNcx* next_module)
{
  next_module_ = next_module;
}

YangNodeNcx* YangModuleNcx::locked_update_first_node()
{
  YangNodeNcx* cached_first = nullptr;
  bool already_cached = first_node_.is_cached_and_get(cache_state_, &cached_first);

  obj_template_t* ch_obj = ncx_get_first_object(ncxmodel_.ncx_instance_, mod_);
  if (ch_obj) {
    if (already_cached) {
      if (cached_first) {
        RW_ASSERT(cached_first->obj_ == ch_obj); // First child changed!
        return cached_first;
      }
    }

    // Found a (new) first node.
    YangNodeNcx* new_node = ncxmodel_.locked_new_node(this, ch_obj);
    locked_set_last_node(new_node);
    return new_node;
  }

  if (already_cached) {
    RW_ASSERT(nullptr == cached_first); // Lost first child!
    return cached_first;
  }

  // No children.
  locked_set_last_node(nullptr);
  return nullptr;
}

YangExtNcx* YangModuleNcx::locked_update_first_extension()
{
  YangExtNcx* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  // ATTN: What about submodules!!
  // ATTN: Probably need a YangExtNcxModule class that knows how to
  //   walk both the module and all of its submodules.

  RW_ASSERT(mod_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &mod_->appinfoQ);
  if (nullptr != appinfo) {
    retval = ncxmodel_.locked_new_ext(appinfo);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }

  return first_extension_.locked_cache_set(&cache_state_, nullptr);
}

YangAugmentNcx* YangModuleNcx::locked_update_first_augment()
{
  YangAugmentNcx* retval = nullptr;
  if (first_augment_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(mod_);
  obj_template_t* obj_augment = (obj_template_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &mod_->datadefQ);
  while (obj_augment && obj_augment->objtype != OBJ_TYP_AUGMENT) {
    obj_augment = (obj_template_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, obj_augment);
  }
  if (nullptr == obj_augment) {
    return first_augment_.locked_cache_set(&cache_state_, nullptr);
  }

  // Find and populate the target node.
  obj_augment_t* augment = obj_augment->def.augment;
  RW_ASSERT(augment);
  obj_template_t* targobj = augment->targobj;
  RW_ASSERT(targobj);
  YangNodeNcx* target_node = ncxmodel_.locked_populate_target_node(targobj);

  // Create the augment descriptor.
  retval = ncxmodel_.locked_new_augment(this, target_node, obj_augment);
  return first_augment_.locked_cache_set(&cache_state_, retval);
}

YangTypedEnumNcx* YangModuleNcx::locked_update_first_typedef()
{
  YangTypedEnumNcx* retval = nullptr;
  if (first_typedef_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  RW_ASSERT(mod_);
  typ_template_t * typ_tmpl = (typ_template_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &mod_->typeQ);
  while (typ_tmpl != NULL) {
    if ((typ_tmpl->typdef.tclass == NCX_CL_SIMPLE) &&
        (typ_tmpl->typdef.def.simple.btyp == NCX_BT_ENUM)) {

      retval = ncxmodel_.locked_search_typed_enum (typ_tmpl);
      if (nullptr == retval) {
        retval = ncxmodel_.locked_new_typed_enum (typ_tmpl);
      }

      RW_ASSERT(retval);

      return first_typedef_.locked_cache_set(&cache_state_, retval);

    }
    typ_tmpl = (typ_template_t*) dlq_nextEntry(ncxmodel_.ncx_instance_, typ_tmpl);
  }
  return nullptr;
}

YangModule** YangModuleNcx::locked_update_first_import()
{
  YangModule** modules = nullptr;
  unsigned index = 0;

  if (imported_modules_.is_cached_and_get(cache_state_, &modules)) {
    return modules;
  }

  import_count_ = dlq_count(ncxmodel_.ncx_instance_, &mod_->importQ);
  if (0 == import_count_) {
    return NULL;
  }

  modules = static_cast<YangModule**> (malloc((import_count_+ 1) * sizeof(YangModule*)));
  memset(modules, 0, (import_count_+1) * sizeof(YangModule*));

  RW_ASSERT(modules);

  RW_ASSERT(mod_);

  ncx_import_t* import = (ncx_import_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &mod_->importQ);

  while (import != NULL) {
    RW_ASSERT(index < import_count_);
    modules[index] = ncxmodel_.locked_search_module((const char*)import->module);
    if (nullptr == modules[index]) {
      modules[index] = ncxmodel_.load_module((const char*)import->module);
    }
    RW_ASSERT(modules[index]);
    import = (ncx_import_t*) dlq_nextEntry(ncxmodel_.ncx_instance_, import);
    index++;
  }
  imported_modules_.locked_cache_set(&cache_state_, modules);
  return (modules);
}

YangGroupingNcx* YangModuleNcx::locked_update_first_grouping()
{
  YangGroupingNcx* retval = nullptr;

  if (first_grouping_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  grp_template_t* grp = (grp_template_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &mod_->groupingQ);

  if (nullptr != grp) {
    YangGroupingNcx* new_grp = ncxmodel_.locked_new_grouping(this, grp);
    if (first_grouping_.is_cached_and_get(cache_state_, &retval)) {
      return retval;
    }
    return first_grouping_.locked_cache_set(&cache_state_, new_grp);
  }

  return first_grouping_.locked_cache_set(&cache_state_, nullptr);
}

void YangModuleNcx::locked_set_last_grouping(YangGroupingNcx* grp)
{
  YangGroupingNcx* cached_first = nullptr;
  bool first_is_cached = first_grouping_.is_cached_and_get(cache_state_, &cached_first);

  if (!first_is_cached || nullptr == cached_first) {
    first_grouping_.locked_cache_set(&cache_state_, grp);
  }
  last_grouping_.locked_cache_set(&cache_state_, grp);
}


/*****************************************************************************/
/*!
 * Construct a yang model grouping  for libncx. This class inherits from the YangNode class
 */

YangGroupingNcx::YangGroupingNcx(YangModelNcxImpl& ncxmodel, YangModuleNcx* module, grp_template_t* grp)
: ncxmodel_(ncxmodel),
  cache_state_(),
  grp_template_(grp),
  module_(module),
  description_(nullptr),
  name_(nullptr),
  prefix_(nullptr),
  ns_(nullptr),
  parent_(nullptr),
  first_child_(),
  first_extension_(),
  next_grouping_(),
  uses_()
{
  RW_ASSERT(module_);
  RW_ASSERT(grp_template_);

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  node_tag_ = grp_template_->objtag;

  // Set the group name
  RW_ASSERT(grp->name && grp->name[0] != '\0');
  name_ = (const char*)grp->name;

  // Set the group descr
  if (grp->descr && grp->descr[0] != '\0') {
    description_ = (const char*)grp->descr;
  } else {
    // ATTN: Something better here
    description_ = "";
  }

  prefix_ = module_->get_prefix();
  ns_ = module_->get_ns();
}

std::string YangGroupingNcx::get_location()
{
  RW_ASSERT(grp_template_);
  RW_ASSERT(grp_template_->tkerr.mod);
  RW_ASSERT(grp_template_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << grp_template_->tkerr.mod->sourcefn << ':' << grp_template_->tkerr.linenum << ':' << grp_template_->tkerr.linepos;
  return oss.str();
}

const char* YangGroupingNcx::get_description()
{
  RW_ASSERT(description_);
  return description_;
}

const char* YangGroupingNcx::get_name()
{
  RW_ASSERT(name_);
  return name_;
}

const char* YangGroupingNcx::get_prefix()
{
  return prefix_;
}

const char* YangGroupingNcx::get_ns()
{
  return ns_;
}

uint32_t YangGroupingNcx::get_node_tag()
{
  return node_tag_;
}

YangNodeNcx* YangGroupingNcx::get_first_child()
{
  YangNodeNcx* retval;

  if (first_child_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  if (first_child_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  return locked_update_first_child();
}

YangNode* YangGroupingNcx::get_next_grouping()
{
  YangGroupingNcx* retval;
  if (next_grouping_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
  return locked_update_next_grouping();
}

/*!
 * Update the model node for the first deep child.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangNodeNcx* YangGroupingNcx::locked_update_first_child()
{
  YangNodeNcx* retval = nullptr;
  bool already_cached = first_child_.is_cached_and_get(cache_state_, &retval);

  RW_ASSERT(grp_template_);

  dlq_hdr_t       *que;
  obj_template_t  *chobj = NULL;

  /* go through the child nodes of this object looking
   * for the first data object; skip over all meta-objects
   */
  que = &(grp_template_->datadefQ);
  if (que) {
    for (chobj = (obj_template_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, que);
         chobj != NULL;
         chobj = (obj_template_t *)dlq_nextEntry(ncxmodel_.ncx_instance_, chobj)) {
      if (obj_has_name(ncxmodel_.ncx_instance_, chobj) && obj_is_enabled(ncxmodel_.ncx_instance_, chobj)) {
        // if Choice or case go further deep
        if (chobj->objtype == OBJ_TYP_CHOICE || chobj->objtype == OBJ_TYP_CASE) {
          chobj = obj_first_child_deep(ncxmodel_.ncx_instance_, chobj);
        } else {
          // Found the first node, break out of the loop
          break;
        }
      }
    }
  }

  if (nullptr == chobj) {
    // There is no current yang child.
    if (!already_cached) {
      // Didn't know that before...  Cache the fact.
      return first_child_.locked_cache_set(&cache_state_, nullptr);
    }
    RW_ASSERT(nullptr == retval); // Had a child and now it's gone?  Huh?
    return retval;
  }

  // There is a yang child node.  See if the cached node changed.
  if (!already_cached) {
    // First time walking the children...
    ; /* Fall through */
  } else if (nullptr == retval) {
    // The child is new - perhaps an augment came in.
    ; /* Fall through */
  } else if (chobj == retval->obj_) {
    // The child hasn't changed.
    return retval;
  } else {
    // The first child changed?  Huh?
    RW_ASSERT_NOT_REACHED();
  }
  retval = ncxmodel_.locked_new_node(this,chobj);
  return first_child_.locked_cache_set(&cache_state_, retval);
}

/*!
 * Update the grouping descriptor for the next grouping.  ASSUMES THE
 * MUTEX IS LOCKED.
 */
YangNode* YangGroupingNcx::locked_update_next_grouping()
{
  YangGroupingNcx* retval = nullptr;
  bool already_cached = next_grouping_.is_cached_and_get(cache_state_, &retval);
  if (already_cached) {
    return retval;
  }

  RW_ASSERT(grp_template_ != nullptr);

  grp_template_t* next_grp = (grp_template_t*) dlq_nextEntry(ncxmodel_.ncx_instance_, grp_template_);

  if (next_grp != nullptr) {
    retval = ncxmodel_.locked_new_grouping(module_,next_grp);
  }

  return (next_grouping_.locked_cache_set(&cache_state_, retval));
}

/*!
 * Update the model grouping for the first list extension.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangExtNcx* YangGroupingNcx::locked_update_first_extension()
{
  YangExtNcxGrouping* retval = nullptr;
  if (first_extension_.is_cached_and_get(cache_state_, &retval)) {
    return retval;
  }
  // Check if the current node has that extension
  RW_ASSERT(grp_template_);
  ncx_appinfo_t* appinfo = (ncx_appinfo_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, &grp_template_->appinfoQ);
  if (nullptr != appinfo) {
    // Start walking the uses extensions.
    retval = ncxmodel_.locked_new_ext(appinfo, grp_template_);
    return first_extension_.locked_cache_set(&cache_state_, retval);
  }
  return retval;
}


/*****************************************************************************/
/*!
 *  Constructs a uses objec
 */
YangUsesNcx::YangUsesNcx(
  YangModelNcxImpl& ncxmodel,
  YangNode* node,
  obj_template_t* obj)
: ncxmodel_(ncxmodel),
  cache_state_(),
  name_(nullptr),
  obj_(obj),
  refined_(false),
  augmented_(false),
  grouping_(nullptr)
{
  RW_ASSERT(node);
  RW_ASSERT(obj);
  RW_ASSERT(OBJ_TYP_USES == obj->objtype);
  RW_ASSERT(obj->def.uses);

  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  obj_template_t *chobj = (obj_template_t *)dlq_firstEntry(ncxmodel_.ncx_instance_, obj_->def.uses->datadefQ);
  name_ = (const char*)obj_->def.uses->name;
  if (nullptr == name_) {
    name_ = "";
  }

  for (;chobj; chobj = (obj_template_t *)dlq_nextEntry(ncxmodel_.ncx_instance_, chobj)) {
    if (chobj->objtype != OBJ_TYP_REFINE) {
      refined_ = true;
    }
    if (chobj->objtype != OBJ_TYP_AUGMENT) {
      augmented_ = true;
    }
  }
  prefix_ = (const char*) obj_->def.uses->prefix;

  if (obj_->def.uses->grp) {
    // Check if the group is already loaded and in the map
    //
    grouping_ = ncxmodel_.locked_search_grouping(obj_->def.uses->grp);

    if (grouping_ == nullptr) {
      // Walk the module and find the grouping_
      if (obj_->def.uses->grp->tkerr.mod) {
        auto ymod = ncxmodel_.locked_search_module(obj_->def.uses->grp->tkerr.mod);
        if (ymod) {
          for (auto gi = ymod->grouping_begin();
               gi != ymod->grouping_end();
               gi++) {
            // This will load all the top level groupings for this module
          }
        }
      }
    }
    grouping_ = ncxmodel_.locked_search_grouping(obj_->def.uses->grp);
  }
}

std::string YangUsesNcx::get_location()
{
  RW_ASSERT(obj_);
  RW_ASSERT(obj_->tkerr.mod);
  RW_ASSERT(obj_->tkerr.mod->sourcefn);

  std::ostringstream oss;
  oss << obj_->tkerr.mod->sourcefn << ':' << obj_->tkerr.linenum << ':' << obj_->tkerr.linepos;
  return oss.str();
}

/*!
 * Get the grouping
 */
YangGroupingNcx* YangUsesNcx::get_grouping()
{
  return grouping_;
}

/*!
 * Check whether the using is augmented or not
 */
bool YangUsesNcx::is_augmented()
{
  return augmented_;
}

/*!
 * Check whether the using is refined or not
 */
bool YangUsesNcx::is_refined()
{
  return refined_;
}


/*****************************************************************************/
/*!
 * Construct a group-based yang extension descriptor for libncx.
 */
YangExtNcxGrouping::YangExtNcxGrouping(YangModelNcxImpl& ncxmodel, ncx_appinfo_t* appinfo, grp_template_t* grp)
: YangExtNcx(ncxmodel,appinfo),
  grp_(grp)
{
  RW_ASSERT(grp_);
}

/*!
 * Update the descriptor to find the next extension.  ASSUMES THE MUTEX
 * IS LOCKED.
 */
YangExtNcx* YangExtNcxGrouping::locked_update_next_extension()
{
  YangExtNcx* retval;
  if (next_extension_.is_cached_and_get(cache_state_,&retval)) {
    return retval;
  }

  RW_ASSERT(appinfo_);
  ncx_appinfo_t* sib_appinfo = (ncx_appinfo_t*)dlq_nextEntry(ncxmodel_.ncx_instance_, appinfo_);
  if (nullptr != sib_appinfo) {
    // There next extension.
    retval = ncxmodel_.locked_new_ext(sib_appinfo, grp_);
    return next_extension_.locked_cache_set(&cache_state_, retval);
  }

#if 0
  // ATTN Check with Tom
  RW_ASSERT(grp_);
  if (nullptr == grp_->usesobj) {
    // No uses or grouping to look in
    return next_extension_.locked_cache_set(&cache_state_, nullptr);
  }


  // No mode extensions on the current list.  Which list are we looking at?
  RW_ASSERT(OBJ_TYP_USES == grp_->usesobj->objtype);
  if (appinfo_ == (ncx_appinfo_t*)dlq_lastEntry(ncxmodel_.ncx_instance_, &grp_->appinfoQ)) {
    // Try the uses' list.
    sib_appinfo = (ncx_appinfo_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &grp_->usesobj->appinfoQ);
    if (nullptr != sib_appinfo) {
      // Start walking the uses extensions.
      retval = ncxmodel_.locked_new_ext(sib_appinfo, grp_);
      return next_extension_.locked_cache_set(&cache_state_, retval);
    }
  }

  RW_ASSERT(grp_->usesobj->def.uses);
  RW_ASSERT(grp_->usesobj->def.uses->grp);
  if (appinfo_ == (ncx_appinfo_t*)dlq_lastEntry(ncxmodel_.ncx_instance_, &grp_->usesobj->def.uses->grp->appinfoQ)) {
    // Done.
    return next_extension_.locked_cache_set(&cache_state_, nullptr);
  }

  sib_appinfo = (ncx_appinfo_t*)dlq_firstEntry(ncxmodel_.ncx_instance_, &grp_->usesobj->def.uses->grp->appinfoQ);
  if (nullptr != sib_appinfo) {
    // Start walking the uses extensions.
    retval = ncxmodel_.locked_new_ext(sib_appinfo, grp_);
    return next_extension_.locked_cache_set(&cache_state_, retval);
  }

#endif

  return next_extension_.locked_cache_set(&cache_state_, nullptr);
}

