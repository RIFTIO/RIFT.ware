
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_yang_pb.cpp
 * @date 2014/08/29
 * @brief RIFT.ware yang/protobuf integration.
 */

#include <rw_yang_pb.h>
#include <rw_keyspec.h>

using namespace rw_yang;


const rw_yang_pb_msgdesc_t* rw_yang_pb_msgdesc_get_msg_msgdesc(
  const rw_yang_pb_msgdesc_t* in_msgdesc)
{
  RW_ASSERT(in_msgdesc);
  switch (in_msgdesc->msg_type) {
    case RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY:
    case RW_YANGPBC_MSG_TYPE_SCHEMA_PATH:
    case RW_YANGPBC_MSG_TYPE_SCHEMA_LIST_ONLY:
      if (in_msgdesc->u) {
        return &in_msgdesc->u->msg_msgdesc;
      }
      break;

    case RW_YANGPBC_MSG_TYPE_MODULE_ROOT:
    case RW_YANGPBC_MSG_TYPE_CONTAINER:
    case RW_YANGPBC_MSG_TYPE_LIST:
    case RW_YANGPBC_MSG_TYPE_LEAF_LIST:
    case RW_YANGPBC_MSG_TYPE_RPC_INPUT:
    case RW_YANGPBC_MSG_TYPE_RPC_OUTPUT:
    case RW_YANGPBC_MSG_TYPE_GROUPING:
    case RW_YANGPBC_MSG_TYPE_NOTIFICATION:
      return in_msgdesc;

    default:
      break;
  }
  return nullptr;
}

const rw_yang_pb_msgdesc_t* rw_yang_pb_get_module_root_msgdesc(
    const rw_yang_pb_msgdesc_t* in_msgdesc)
{
  RW_ASSERT(in_msgdesc);

  auto msg_desc = rw_yang_pb_msgdesc_get_msg_msgdesc(in_msgdesc);
  const rw_yang_pb_msgdesc_t* return_mdesc = nullptr;

  switch (msg_desc->msg_type) {
    case RW_YANGPBC_MSG_TYPE_MODULE_ROOT:
      return_mdesc = msg_desc;
      break;
    case RW_YANGPBC_MSG_TYPE_CONTAINER:
    case RW_YANGPBC_MSG_TYPE_LIST:
    case RW_YANGPBC_MSG_TYPE_LEAF_LIST: {
      auto cat = rw_keyspec_path_get_category(msg_desc->schema_path_value);
      switch (cat) {
        case RW_SCHEMA_CATEGORY_ANY:
        case RW_SCHEMA_CATEGORY_DATA:
        case RW_SCHEMA_CATEGORY_CONFIG:
          return_mdesc = msg_desc->module->data_module;
          break;
        case RW_SCHEMA_CATEGORY_RPC_INPUT:
          return_mdesc = msg_desc->module->rpci_module;
          break;
        case RW_SCHEMA_CATEGORY_RPC_OUTPUT:
          return_mdesc = msg_desc->module->rpco_module;
          break;
        case RW_SCHEMA_CATEGORY_NOTIFICATION:
          return_mdesc = msg_desc->module->notif_module;
          break;
        default:
          break;
      }
      break;
    }
    case RW_YANGPBC_MSG_TYPE_RPC_INPUT:
      return_mdesc = msg_desc->module->rpci_module;
      break;
    case RW_YANGPBC_MSG_TYPE_RPC_OUTPUT:
      return_mdesc = msg_desc->module->rpco_module;
      break;
    case RW_YANGPBC_MSG_TYPE_NOTIFICATION:
      return_mdesc = msg_desc->module->notif_module;
      break;
    case RW_YANGPBC_MSG_TYPE_GROUPING:
    default:
      break;
  }

  return return_mdesc;
}

const rw_yang_pb_msgdesc_t* rw_yang_pb_msgdesc_get_path_msgdesc(
  const rw_yang_pb_msgdesc_t* in_msgdesc)
{
  RW_ASSERT(in_msgdesc);
  switch (in_msgdesc->msg_type) {
    case RW_YANGPBC_MSG_TYPE_SCHEMA_PATH:
      return in_msgdesc;

    case RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY:
    case RW_YANGPBC_MSG_TYPE_SCHEMA_LIST_ONLY:
      if (in_msgdesc->u) {
        return in_msgdesc->u->msg_msgdesc.schema_path_ypbc_desc;
      }
      break;

    case RW_YANGPBC_MSG_TYPE_MODULE_ROOT:
    case RW_YANGPBC_MSG_TYPE_CONTAINER:
    case RW_YANGPBC_MSG_TYPE_LIST:
    case RW_YANGPBC_MSG_TYPE_LEAF_LIST:
    case RW_YANGPBC_MSG_TYPE_RPC_INPUT:
    case RW_YANGPBC_MSG_TYPE_RPC_OUTPUT:
    case RW_YANGPBC_MSG_TYPE_GROUPING:
    case RW_YANGPBC_MSG_TYPE_NOTIFICATION:
      return in_msgdesc->schema_path_ypbc_desc;

    default:
      break;
  }
  return nullptr;
}

const rw_yang_pb_msgdesc_t* rw_yang_pb_msgdesc_get_entry_msgdesc(
  const rw_yang_pb_msgdesc_t* in_msgdesc)
{
  RW_ASSERT(in_msgdesc);
  switch (in_msgdesc->msg_type) {
    case RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY:
      return in_msgdesc;

    case RW_YANGPBC_MSG_TYPE_SCHEMA_PATH:
    case RW_YANGPBC_MSG_TYPE_SCHEMA_LIST_ONLY:
      if (in_msgdesc->u) {
        return in_msgdesc->u->msg_msgdesc.schema_entry_ypbc_desc;
      }
      break;

    case RW_YANGPBC_MSG_TYPE_MODULE_ROOT:
    case RW_YANGPBC_MSG_TYPE_CONTAINER:
    case RW_YANGPBC_MSG_TYPE_LIST:
    case RW_YANGPBC_MSG_TYPE_LEAF_LIST:
    case RW_YANGPBC_MSG_TYPE_RPC_INPUT:
    case RW_YANGPBC_MSG_TYPE_RPC_OUTPUT:
    case RW_YANGPBC_MSG_TYPE_GROUPING:
    case RW_YANGPBC_MSG_TYPE_NOTIFICATION:
      return in_msgdesc->schema_entry_ypbc_desc;

    default:
      break;
  }
  return nullptr;
}

static const rw_yang_pb_msgdesc_t* rw_schema_pbcm_get_any_msgdesc(
  rw_keyspec_instance_t* instance,
  const ProtobufCMessage* msg,
  const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT(msg);
  const ProtobufCMessageDescriptor* pbcmd = msg->descriptor;
  RW_ASSERT(pbcmd);

  // The easy way - the message has a concrete schema already.
  if (pbcmd->ypbc_mdesc) {
    return pbcmd->ypbc_mdesc;
  }

  // The message must be a generic path and a schema is required to convert.
  if (   nullptr == schema
      || pbcmd != &rw_schema_path_spec__descriptor) {
    return nullptr;
  }

  const rw_yang_pb_msgdesc_t* retval = nullptr;
  rw_status_t rs = rw_keyspec_path_find_msg_desc_schema(
    (rw_keyspec_path_t*)msg,
    instance,
    schema,
    &retval,
    NULL/*remainder*/ );

  // If did not find the entire thing, then there is no match at all.
  // So, do not accept RW_STATUS_OUT_OF_BOUNDS.
  if (RW_STATUS_SUCCESS != rs) {
    return nullptr;
  }
  RW_ASSERT(retval);
  return retval;
}

const rw_yang_pb_msgdesc_t* rw_schema_pbcm_get_msg_msgdesc(
  rw_keyspec_instance_t* instance,
  const ProtobufCMessage* msg,
  const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT(msg);
  const rw_yang_pb_msgdesc_t* msgdesc =
      rw_schema_pbcm_get_any_msgdesc(instance, msg, schema);
  if (nullptr == msgdesc) {
    return nullptr;
  }
  return rw_yang_pb_msgdesc_get_msg_msgdesc(msgdesc);
}

const rw_yang_pb_msgdesc_t* rw_schema_pbcm_get_path_msgdesc(
  rw_keyspec_instance_t* instance,
  const ProtobufCMessage* msg,
  const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT(msg);
  const rw_yang_pb_msgdesc_t* msgdesc =
      rw_schema_pbcm_get_any_msgdesc(instance, msg, schema);
  if (nullptr == msgdesc) {
    return nullptr;
  }
  return rw_yang_pb_msgdesc_get_path_msgdesc(msgdesc);
}

const rw_yang_pb_msgdesc_t* rw_schema_pbcm_get_entry_msgdesc(
  rw_keyspec_instance_t* instance,
  const ProtobufCMessage* msg,
  const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT(msg);
  const rw_yang_pb_msgdesc_t* msgdesc =
      rw_schema_pbcm_get_any_msgdesc(instance, msg, schema);
  if (nullptr == msgdesc) {
    return nullptr;
  }
  return rw_yang_pb_msgdesc_get_entry_msgdesc(msgdesc);
}

