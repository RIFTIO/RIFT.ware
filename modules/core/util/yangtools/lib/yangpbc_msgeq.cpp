
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file yangpbc_msgeq.cpp
 * @author Tom Seidenberg
 * @date 2014/11/01
 * @brief Message equivalence management.
 */

#include "yangpbc.hpp"

#include <sstream>

namespace rw_yang {


PbEqMgr::PbEqMgr(PbModel* pbmodel)
: pbmodel_(pbmodel)
{
  RW_ASSERT(pbmodel_);
}

PbModel* PbEqMgr::get_pbmodel() const
{
  return pbmodel_;
}

void PbEqMgr::parse_equivalence(
  PbMessage* pbmsg)
{
  RW_ASSERT(pbmsg);
  auto key = make_key(pbmsg);

  // Root nodes are always unique.
  if (pbmsg->get_parent_pbmsg()) {
    // Look through all the known equivalences for an exact match.
    for (auto epi = eq_map_.lower_bound(key);
         epi != eq_map_.upper_bound(key);
         ++epi) {
      PbEqMsgSet* pbeq = epi->second.get();
      RW_ASSERT(pbeq);
      if (pbeq->is_equivalent(pbmsg)) {
        add_equivalence(pbeq, pbmsg);
        return;
      }
    }
  }

  // Didn't find a match, need to create a new one
  PbEqMsgSet::eq_uptr_t pbeq_uptr(new PbEqMsgSet(pbmsg));
  PbEqMsgSet* pbeq = pbeq_uptr.get();
  eq_map_.emplace(key, std::move(pbeq_uptr));
  add_equivalence(pbeq, pbmsg);

  if (pbmsg->is_top_level_message()) {
    pbmodel_->save_top_level_equivalence(pbeq);
  }

  /*
   * Need to determine the new message set's set equivalences, as
   * viewed through the lens of all the other schemas.  When viewed in
   * other schemas, this message may actually contain fewer fields
   * (because the other schemas do not have all the augments that the
   * message's defining schema has).  That, in turn, means that the
   * message may have a different set of equivalences in those other
   * schemas.  Determination of those equivalences may be necessary in
   * order to generate the correct code references.  The comparisons
   * happen in both directions - look for equivalences that decay into
   * this equivalence, and look for equivalences this equivalence will
   * decay into (in terms of module import relationshsips).
   */
  for (auto epi = eq_map_.lower_bound(key);
       epi != eq_map_.upper_bound(key);
       ++epi) {
    epi->second->parse_schema_lower_equivalence(pbeq);
    pbeq->parse_schema_lower_equivalence(epi->second.get());
  }
}

void PbEqMgr::find_preferred_msgs()
{
  /*
   * Iterate over all the equivalence sets, selecting a preferred
   * message in each set.
   */
  for (auto const& eqm: eq_map_) {
    eqm.second->pick_preferences();
  }
}

std::string PbEqMgr::make_key(PbMessage* pbmsg)
{
  RW_ASSERT(pbmsg);
  PbNode* pbnode = pbmsg->get_pbnode();
  RW_ASSERT(pbnode);
  std::string key = pbnode->get_proto_msg_new_typename();
  if (0 == key.length()) {
    key = pbnode->get_proto_msg_typename();
  }
  return key;
}

std::string PbEqMgr::make_key(PbEqMsgSet* pbeq)
{
  RW_ASSERT(pbeq);
  return pbeq->get_key();
}

void PbEqMgr::add_equivalence(
  PbEqMsgSet* pbeq,
  PbMessage* pbmsg)
{
  RW_ASSERT(pbeq);
  RW_ASSERT(pbmsg);

  bool new_def = pbeq->add_equivalence(pbmsg);
  if (!new_def) {
    // Not new - don't care.
    return;
  }

  check_conflicts(pbeq, pbmsg);
}

void PbEqMgr::check_conflicts(
  PbEqMsgSet* pbeq,
  PbMessage* pbmsg)
{
  RW_ASSERT(pbeq);
  RW_ASSERT(pbmsg);

  // Root nodes never conflict.
  if (nullptr == pbmsg->get_parent_pbmsg()) {
    return;
  }

  // Check for potential top-level message definition conflicts.
  if (!pbmsg->is_top_level_message()) {
    // Not top level - cannot have a conflict, by definition
    return;
  }
  PbModule* check_pbmod = pbmsg->get_lci_pbmod();

  /*
   * Iterate over all the equivalence sets, looking for multiple sets
   * that have conflicting rwpb:msg-new extensions when viewed from a
   * single schema.  These should be considered yang bugs, because
   * multiple top-level message definitions would be required to have
   * the same name.  That is not allowed - it would cause duplicate
   * proto/C type definitions.
   */
  auto const& key = pbeq->get_key();
  auto i_compare = eq_map_.lower_bound(key);
  RW_ASSERT(i_compare != eq_map_.end()); // pbeq must be there, at least
  auto i_stop = eq_map_.upper_bound(key);
  for (; i_compare != i_stop; ++i_compare ) {
    PbEqMsgSet* other_pbeq = i_compare->second.get();
    RW_ASSERT(other_pbeq);
    if (other_pbeq == pbeq) {
      // Skip the set being checked.
      continue;
    }

    PbMessage* other_pbmsg = other_pbeq->get_any_pbmsg();
    if (!other_pbmsg->is_top_level_message()) {
      // Not top level - cannot have a conflict, by definition.
      continue;
    }

    std::string this_loc = pbmsg->get_ynode()->get_location();
    std::string other_loc = other_pbmsg->get_ynode()->get_location();
    if (this_loc != other_loc) {
      std::ostringstream oss;
      if (   pbmsg->get_pbmod() == get_pbmodel()->get_schema_pbmod()
          && other_pbmsg->get_pbmod() == get_pbmodel()->get_schema_pbmod()) {
        oss << "Conflicting rwpb:msg-new definitions for "
            << pbmsg->get_pbnode()->get_proto_msg_new_typename()
            << ", other definition at " << other_loc
            << ", suppressing both";
        get_pbmodel()->error(this_loc, oss.str());
        pbeq->mark_conflict(check_pbmod);
        other_pbeq->mark_conflict(check_pbmod);
        continue;
      }
      if (pbmsg->get_pbmod() == get_pbmodel()->get_schema_pbmod()) {
        oss << "Duplicate rwpb:msg-new definition for "
            << pbmsg->get_pbnode()->get_proto_msg_new_typename()
            << ", suppressing definition at " << other_loc
            << ", keeping this definition";
        get_pbmodel()->warning(this_loc, oss.str());
        other_pbeq->mark_conflict(check_pbmod);
        continue;
      }
      if (other_pbmsg->get_pbmod() == get_pbmodel()->get_schema_pbmod()) {
        oss << "Duplicate rwpb:msg-new definition for "
            << pbmsg->get_pbnode()->get_proto_msg_new_typename()
            << ", suppressing definition at " << this_loc
            << ", keeping this definition";
        get_pbmodel()->warning(other_loc, oss.str());
        pbeq->mark_conflict(check_pbmod);
        continue;
      }
      oss << "Duplicate rwpb:msg-new definitions for "
          << pbmsg->get_pbnode()->get_proto_msg_new_typename()
          << ", previous definition at " << other_loc
          << ", suppressing both";
      get_pbmodel()->warning(this_loc, oss.str());
      pbeq->mark_conflict(check_pbmod);
      other_pbeq->mark_conflict(check_pbmod);
      continue;
    }

    for (auto const& other_pbmod: other_pbeq->defining_pbmods_) {
      if (check_pbmod != other_pbmod) {
        // Different defining module - not a conflict.
        continue;
      }

      /*
       * When viewed through the lens of check_pbmod, the message sets
       * defined by pbeq and other_pbeq conflict.  They both define the
       * same top-level message type, within the same package, but the
       * definitions are different.
       *
       * There are several ways this conflict could be resolved.
       *  - Generate only the lowest-level definition (the purest
       *    grouping) as top-level, and generate the others only in
       *    their schema location.  The suppressed top-level messages
       *    generate warnings.
       *  - Generate all versions only in their schema location.  The
       *    suppressed top-level messages generate warnings.
       *  - Generate an error for the conflict and let the yang author
       *    solve the problem by moving or specifying new rwpb:msg-new
       *    extensions.
       *  - Generate a new super-type message that merges all of the
       *    fields visible in the same schema.  A complicating factor
       *    with a super-type is that conflicts may be caused by things
       *    that are not mergeable - such as fields with the same name
       *    buf different types, or by incompatible yang refinements)
       *
       * The current code will generate a warning message upon a
       * conflict and suppress all top-level definitions.
       */
      const char* reason = nullptr;
      PbField* bad_field = nullptr;
      bool equiv = pbmsg->is_equivalent(other_pbmsg, &reason, &bad_field, check_pbmod);
      RW_ASSERT(!equiv); // huh? already decided they were not equivalent!
      RW_ASSERT(reason);

      std::string new_loc;
      YangUses* yuses = pbmsg->get_ynode()->get_uses();
      if (yuses) {
        new_loc = yuses->get_location();
      } else {
        new_loc = pbmsg->get_ynode()->get_location();
      }

      std::string other_loc;
      yuses = other_pbmsg->get_ynode()->get_uses();
      if (yuses) {
        other_loc = yuses->get_location();
      } else {
        other_loc = other_pbmsg->get_ynode()->get_location();
      }

      std::ostringstream oss;
      oss << "Refined/Augmented yang node " << pbmsg->get_ynode()->get_name()
          << " has multiple conflicting definitions in module "
          << check_pbmod->get_ymod()->get_name()
          << ", previous definition at " << other_loc
          << ": " << reason;
      if (bad_field) {
        oss << " in field " << bad_field->get_ynode()->get_name();
      }
      get_pbmodel()->warning(new_loc, oss.str());

      pbeq->mark_conflict(check_pbmod);
      other_pbeq->mark_conflict(check_pbmod);
    }
  }
}



/*****************************************************************************/

PbEqMsgSet::PbEqMsgSet(
  PbMessage* pbmsg)
{
  RW_ASSERT(pbmsg);
  base_pbmod_ = pbmsg->get_pbmod();
  defining_pbmods_.emplace(pbmsg->get_lci_pbmod());
}

PbModel* PbEqMsgSet::get_pbmodel() const
{
  return base_pbmod_->get_pbmodel();
}

PbModule* PbEqMsgSet::get_base_pbmod() const
{
  return base_pbmod_;
}

std::string PbEqMsgSet::get_key() const
{
  return PbEqMgr::make_key(get_any_pbmsg());
}

PbMessage* PbEqMsgSet::get_any_pbmsg() const
{
  if (pbmsgs_.size()) {
    PbMessage* pbmsg = *pbmsgs_.begin();
    RW_ASSERT(pbmsg);
    return pbmsg;
  }
  RW_ASSERT(suppressed_pbmsgs_.size());
  PbMessage* pbmsg = *suppressed_pbmsgs_.begin();
  RW_ASSERT(pbmsg);
  return pbmsg;
}

PbModule* PbEqMsgSet::get_any_pbmod() const
{
  PbModule* pbmod = *defining_pbmods_.begin();
  RW_ASSERT(pbmod);
  return pbmod;
}

bool PbEqMsgSet::is_equivalent(
  PbMessage* pbmsg,
  PbModule* schema_pbmod) const
{
  RW_ASSERT(pbmsg);
  return pbmsg->is_equivalent(get_any_pbmsg(), nullptr, nullptr, schema_pbmod);
}

bool PbEqMsgSet::add_equivalence(
  PbMessage* pbmsg)
{
  RW_ASSERT(pbmsg);
  pbmsgs_.emplace_back(pbmsg);
  pbmsg->set_pbeq(this);

  PbModule* lci_pbmod = pbmsg->get_lci_pbmod();
  RW_ASSERT(lci_pbmod);

  auto dmi = reference_pbmods_.find(lci_pbmod);
  if (dmi != reference_pbmods_.end()) {
    // Have already seen this LCI with this message set.
    return false;
  }
  reference_pbmods_.emplace(lci_pbmod);

  /*
   * The LCI is not known yet.  Check all of the existing defining
   * modules for verify that they are still valid and/or to determine
   * if this is a new defining module.  The defining modules must be
   * mutually non-(transitive)-importing.
   */
  bool new_define = true;
  for (auto dmi = defining_pbmods_.begin();
       dmi != defining_pbmods_.end();
       /*none*/) {
    PbModule* other_pbmod = *dmi;
    if (other_pbmod != lci_pbmod) {
      if (other_pbmod->imports_transitive(lci_pbmod)) {
        /*
         * The new module is an import of another defining module.
         * That other defining module is not defining, after all, so
         * remove it.
         */
        RW_ASSERT(new_define);
        dmi = defining_pbmods_.erase(dmi);
        continue;
      }
      if (lci_pbmod->imports_transitive(other_pbmod)) {
        // The new module imports a defining module; it is not new
        new_define = false;
      }
    }
    ++dmi;
  }

  if (new_define) {
    defining_pbmods_.emplace(lci_pbmod);
  }
  RW_ASSERT(defining_pbmods_.size());
  RW_ASSERT(defining_pbmods_.size() <= reference_pbmods_.size());
  return true;
}

void PbEqMsgSet::parse_schema_lower_equivalence(
  PbEqMsgSet* lower_pbeq)
{
  RW_ASSERT(lower_pbeq);
  PbEqMsgSet* higher_pbeq = this;
  if (higher_pbeq == lower_pbeq) {
    // Self
    return;
  }

  PbMessage* higher_compare_pbmsg = higher_pbeq->get_any_pbmsg();

  // Ignore root nodes
  if (nullptr == higher_compare_pbmsg->get_parent_pbmsg()) {
    return;
  }

  PbModule* lower_defining_pbmod = lower_pbeq->get_any_pbmod();
  PbMessage* lower_compare_pbmsg = lower_pbeq->get_any_pbmsg();
  if (!higher_compare_pbmsg->is_equivalent(lower_compare_pbmsg, nullptr, nullptr, lower_defining_pbmod)) {
    // Not interesting - it is not equivalent
    return;
  }

  auto oei = higher_pbeq->other_pbeqs_.find(lower_pbeq);
  if (oei != higher_pbeq->other_pbeqs_.end()) {
    // Already knew the relationship
    return;
  }

  /*
   * The messages must not be equivalent in totality - that's one of
   * the underlying assumptions of the fact that there are 2 different
   * equivalence sets!  So, assert on that to make sure the code is not
   * buggy.
   */
  RW_ASSERT(!lower_compare_pbmsg->is_equivalent(higher_compare_pbmsg, nullptr, nullptr));
  higher_pbeq->other_pbeqs_.emplace(lower_pbeq);
}

void PbEqMsgSet::parse_find_dependencies(
  depends_map_t* depends_map,
  depends_map_iter_t this_dmi) const
{
  PbMessage* pbmsg = get_any_pbmsg();
  RW_ASSERT(pbmsg->is_parse_complete());

  /*
   * Iterate all the yang children of one of the representative
   * messages, (recursively) recording output-order constraints between
   * this equivalence set and child message equivalence sets.  These
   * dependencies may determine how C code will be generated, because
   * some C code must be generated in definition order (in particular,
   * C structs for inline messages).
   */
  for (auto fi = pbmsg->field_begin(); fi != pbmsg->field_end(); ++fi) {
    PbField* pbfld = fi->get();
    RW_ASSERT(pbfld);
    PbMessage* field_pbmsg = pbfld->get_field_pbmsg();
    if (!field_pbmsg) {
      // not a message, no dependency
      continue;
    }

    PbModule* lci_pbmod = get_any_pbmod();
    if (lci_pbmod != get_pbmodel()->get_schema_pbmod()) {
      /*
       * Don't care about dependency order if the message is an
       * external dependency.  Definitions from external dependencies
       * are taken from proto import or C include, and therefore
       * assumed to exist at any point of use.
       */
      continue;
    }

    PbEqMsgSet* other_pbeq = field_pbmsg->get_pbeq();
    RW_ASSERT(other_pbeq);

    auto other_dmi = depends_map->find(other_pbeq);
    if (other_dmi != depends_map->end()) {
      this_dmi->second.emplace(other_pbeq);
      if (other_dmi->second.size()) {
        /*
         * Have already determined the child's dependency set, so don't
         * do it again.  Just take all of its dependencies as our own.
         */
        this_dmi->second.insert(other_dmi->second.begin(), other_dmi->second.end());
        continue;
      }
    }

    // Find the child's dependencies
    other_pbeq->parse_find_dependencies(depends_map, this_dmi);
  }
}

void PbEqMsgSet::parse_order_dependencies(
  depends_map_t* depends_map,
  depends_map_iter_t this_dmi,
  eq_list_t* order_list)
{
  // Spin until there are no unordered dependencies left.
  while (this_dmi->second.size()) {
    auto other_i = this_dmi->second.begin();
    PbEqMsgSet* other_pbeq = *other_i;
    auto other_dmi = depends_map->find(other_pbeq);
    if (other_dmi != depends_map->end()) {
      // Figure out the other's dependencies first
      other_pbeq->parse_order_dependencies(depends_map, other_dmi, order_list);
    }
    this_dmi->second.erase(other_i);
  }
  order_list->emplace_back(this);
  depends_map->erase(this_dmi);
}

void PbEqMsgSet::order_dependencies(
  eq_list_t* order_list)
{
  RW_ASSERT(order_list);

  // Build the mapping table
  depends_map_t depends_map;
  for (PbEqMsgSet* pbeq: *order_list) {
    depends_map.emplace(pbeq, eq_set_t{});
  }

  // Empty the input list, in order to rebuild it
  order_list->clear();

  // Find all the dependencies within the mapping.
  for (auto dmi = depends_map.begin();
       dmi != depends_map.end();
       ++dmi) {
    dmi->first->parse_find_dependencies(&depends_map, dmi);
  }

  // Rebuild the list base on the order.
  while (depends_map.size()) {
    auto dmi = depends_map.begin();
    dmi->first->parse_order_dependencies(&depends_map, dmi, order_list);
  }
}

void PbEqMsgSet::pick_preferences()
{
  /*
   * Iterate through all the defining modules, picking a preference for
   * each of those modules.  Remember all of the messages that get
   * selected across all defining modules.
   */
  std::set<PbMessage*> selected;
  for (PbModule* defining_pbmod: defining_pbmods_) {
    if (has_conflict(defining_pbmod)) {
      continue;
    }

    /*
     * Iterate through all the messages, looking for the best ones.  There
     * must be a best one per defining module - it is not possible to pick
     * just one when there are multiple defining modules because yangpbc
     * must recreate each module's schema.
     */
    PbMessage* best_pbmsg = nullptr;
    for (PbMessage* compare_pbmsg: pbmsgs_) {
      if (!best_pbmsg) {
        // The first one is always best.
        best_pbmsg = compare_pbmsg;
        continue;
      }

      best_pbmsg = best_pbmsg->equivalent_which_comes_first(compare_pbmsg, defining_pbmod);
    }
    if (best_pbmsg) {
      preferences_.emplace(defining_pbmod, best_pbmsg);
      selected.emplace(best_pbmsg);
    }
  }

  /*
   * Now suppress all messages were not selected.  Suppression modifies
   * pbmsgs_ in place, invalidating any iterators to the suppressed
   * message; so the next iterator must be calculated before
   * suppressing.
   */
  for (auto ci = pbmsgs_.begin();
       ci != pbmsgs_.end();
       /*none*/) {
    PbMessage* check_pbmsg = *ci;
    ++ci;
    auto seli = selected.find(check_pbmsg);
    if (seli != selected.end()) {
      // Was kept, don't suppress it, mark it as a top message.
      if (check_pbmsg->is_top_level_message()) {
        check_pbmsg->set_chosen_eq();
      }
      continue;
    }
    suppress_msg(check_pbmsg);
  }

  /*
   * ATTN: problem with grouping/top-level orderings?
   *  - Is it possible for a top-level to refer to a grouping message?
   *  - And conversely, is it possible for a grouping to refer to a
   *    top-level?
   *  - ATTN: I think the answer to both the previous 2 questions is yes,
   *    and therefore there is a problem ordering top-levels with respect
   *    to groupings!
   */
}

void PbEqMsgSet::suppress_msg(
  PbMessage* pbmsg)
{
  RW_ASSERT(pbmsg);
  // ATTN: This should be a set, not a list.  Is it even used?
  suppressed_pbmsgs_.emplace_back(pbmsg);
  pbmsgs_.remove(pbmsg);

  // Need to iterate over all children messages and suppress them, too
  for (auto fi = pbmsg->field_begin(); fi != pbmsg->field_end(); ++fi) {
    PbField* pbfld = fi->get();
    RW_ASSERT(pbfld);
    PbMessage* field_pbmsg = pbfld->get_field_pbmsg();
    if (field_pbmsg) {
      PbEqMsgSet* other_pbeq = field_pbmsg->get_pbeq();
      RW_ASSERT(other_pbeq);
      other_pbeq->suppress_msg(field_pbmsg);
    }
  }
}

void PbEqMsgSet::mark_conflict(
  PbModule* schema_pbmod)
{
  RW_ASSERT(schema_pbmod);
  auto ci = conflict_pbmods_.find(schema_pbmod);
  if (ci == conflict_pbmods_.end()) {
    conflict_pbmods_.emplace(schema_pbmod);
    PbMessage* pbmsg = get_any_pbmsg();

    // Need to iterate over all children messages and mark them, too
    for (auto fi = pbmsg->field_begin(); fi != pbmsg->field_end(); ++fi) {
      PbField* pbfld = fi->get();
      RW_ASSERT(pbfld);
      PbMessage* field_pbmsg = pbfld->get_field_pbmsg();
      if (field_pbmsg && !field_pbmsg->is_top_level_message()) {
        PbEqMsgSet* other_pbeq = field_pbmsg->get_pbeq();
        RW_ASSERT(other_pbeq);
        other_pbeq->mark_conflict(schema_pbmod);
      }
    }
  }
}

bool PbEqMsgSet::has_conflict(
  PbModule* schema_pbmod) const
{
  RW_ASSERT(schema_pbmod);
  auto ci = conflict_pbmods_.find(schema_pbmod);
  return ci != conflict_pbmods_.end();
}

PbMessage* PbEqMsgSet::get_preferred_msg(
  PbModule* schema_pbmod)
{
  if (!schema_pbmod) {
    schema_pbmod = get_pbmodel()->get_schema_pbmod();
  }
  auto pi = preferences_.find(schema_pbmod);
  if (pi != preferences_.end()) {
    return pi->second;
  }

  // ATTN: This is not a stable sort, when viewed from other schemas!
  // Take the first module that works.
  for (auto const& pref: preferences_) {
    if (schema_pbmod->imports_transitive(pref.first)) {
      PbMessage* best_pbmsg = pref.second;
      RW_ASSERT(best_pbmsg); // must have found a best - pbmsgs_ cannot be empty
      preferences_.emplace(schema_pbmod, best_pbmsg);
      return best_pbmsg;
    }
  }

  return nullptr;
}

PbEqMsgSet::msg_citer_t PbEqMsgSet::msg_begin() const
{
  return pbmsgs_.begin();
}

PbEqMsgSet::msg_citer_t PbEqMsgSet::msg_end() const
{
  return pbmsgs_.end();
}


} // namespace rw_yang

// End of yangpbc_msgeq.cpp
