/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwuagent_proto_splitter.cpp
 *
 * Protobuf conversion support.
 */

#include "rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;


ProtoSplitter::ProtoSplitter(
    SbReqEditConfig* edit_cfg,
    rwdts_xact_t* xact,
    YangModel *model,
    RWDtsXactFlag flag,
    XMLNode* root_node)
    : edit_cfg_(edit_cfg),
      xact_(xact),
      dts_flags_(flag),
      root_node_(root_node),
      transaction_contents_(),
      block_collector_(transaction_contents_)
{
}

ProtoSplitter::ProtoSplitter(
    SbReqEditConfig* edit_cfg,
    rwdts_xact_t *xact,
    rw_yang::YangModel *model,
    RWDtsXactFlag flag,
    rw_yang::XMLNode* root_node,
    std::list<BlockContents> & transaction_contents)
    : edit_cfg_(edit_cfg),
      xact_(xact),
      dts_flags_(flag),
      root_node_(root_node),
      transaction_contents_(),
      block_collector_(transaction_contents),
      own_contents_(false),
      create_blocks_(false)
{

}

ProtoSplitter::~ProtoSplitter()
{
  if (own_contents_) {
    for (auto block_contents : transaction_contents_) {
      if (block_contents.first) {
        rw_keyspec_path_free(block_contents.first, nullptr);
      }
      if (block_contents.second) {
        protobuf_c_message_free_unpacked (nullptr, block_contents.second);
      }
    }
  }
}

rw_xml_next_action_t ProtoSplitter::visit(
    XMLNode* node,
    XMLNode** path,
    int32_t level )
{
  if (!(level >=0 && level < RW_MAX_XML_NODE_DEPTH)) {
    return RW_XML_ACTION_TERMINATE;
  }

  if (node == root_node_) {
    node = root_node_->get_first_child();
  }

  // In case of empty root xml
  // for ex. in delete queries
  if (nullptr == node) {
    return RW_XML_ACTION_NEXT;
  }

  YangNode *yn = node->get_yang_node();

  if (nullptr == yn) {
    return RW_XML_ACTION_NEXT;
  }

  // construct new block contents to be filled in as we create it
  block_collector_.emplace_back(nullptr, nullptr);
  BlockContents & contents = block_collector_.back();

  ProtobufCMessage *message;
  rw_yang_netconf_op_status_t ncs = node->to_pbcm (&message);
  if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
    return RW_XML_ACTION_TERMINATE;
  } else {
    contents.second = message;
  }

  created_block_ = true;
  rw_keyspec_path_t* keyspec = nullptr;

  ncs = node->to_keyspec(&keyspec);
  if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
    return RW_XML_ACTION_TERMINATE;
  } else {
    contents.first = keyspec;
  }

  rw_keyspec_path_set_category (keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  if (!create_blocks_) {
    // don't make the block, we are only collecting the information 
    // necessary to make the blocks later
    return RW_XML_ACTION_NEXT_SIBLING;
  }

  if (!edit_cfg_->add_block_to_xact(keyspec, RWDTS_QUERY_UPDATE, message)) {
    return RW_XML_ACTION_TERMINATE;
  }

  return RW_XML_ACTION_NEXT_SIBLING;
}

bool ProtoSplitter::executed_xact()
{
  return created_block_ && create_blocks_;
}
