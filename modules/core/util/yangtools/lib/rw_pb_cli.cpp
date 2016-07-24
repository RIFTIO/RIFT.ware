
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_pb_cli.cpp
 * @author Shaji Radhakrishnan (shaji.radhakrishnan@riftio.com)
 * @date 2015/03/05
 * @brief Implementation of proto/xml to rw-cli conversion
 */

#include "rw_xml.h"
#include "rw_keyspec.h"
#include <iostream>
#include <inttypes.h>
#include <boost/scoped_array.hpp>

using namespace rw_yang;

static
unsigned rw_pb_create_field_indices(const rw_yang_pb_msgdesc_t* ypbc_msgdesc,
                               YangNode* ynode,
                               std::vector<int>& fields)
{
  RW_ASSERT(ypbc_msgdesc);
  RW_ASSERT(ynode);

  auto ypbc_fdescs = ypbc_msgdesc->ypbc_flddescs;
  unsigned nkeys = 0;

  if (ynode->get_stmt_type() == RW_YANG_STMT_TYPE_LIST) {
    for (YangKeyIter yki = ynode->key_begin(); yki != ynode->key_end(); yki++) {
      YangNode *knode = yki->get_key_node();
      for (unsigned i = 0; i < ypbc_msgdesc->num_fields; i++) {
        if (!strcmp(knode->get_name(), ypbc_fdescs[i].yang_node_name)) {
          fields.push_back(ypbc_fdescs[i].pbc_order);
        }
      }
      nkeys++;
    }
  }

  RW_ASSERT(fields.size() == nkeys);

  // Remaining fields in the yang-order, though it doesn't matter.
  for (unsigned i = 0; i < ypbc_msgdesc->num_fields; i++) {
    if (!(ypbc_fdescs[i].pbc_fdesc->rw_flags & RW_PROTOBUF_FOPT_KEY)) {
      fields.push_back(ypbc_fdescs[i].pbc_order);
    }
  }

  return nkeys;
}

static 
rw_status_t rw_pb_to_cli_internal(YangNode* ynode, 
                                  const ProtobufCMessage *pbcmsg, 
                                  std::vector<std::string>& commands,
                                  const std::string& line,
                                  unsigned indent)
{
  std::stringstream oss;
  rw_status_t status = RW_STATUS_FAILURE;

  std::string start(indent, ' ');
  std::string space(1, ' ');
  std::string pad = space;

  if (!ynode) {
    return status;
  }

  if (!line.empty()) {
    oss << line << space;
  } else {
    oss << start;
  }
  
  if (!strcmp(ynode->get_parent()->get_name(), "data") &&
      !strcmp(ynode->get_name(), "colony")) {
    // Duplicate node-names at the same level have to be qualified 
    // with the namespace prefix. To achieve this, the yangmodel should
    // have the rw-composite.yang loaded for the API to see all the yang modules
    // that confd would see. For now qualifying only for "colony".
    oss << ynode->get_module()->get_prefix() << ":"
        << ynode->get_name();
  } else {
    oss << ynode->get_name();
  }

  const ProtobufCMessageDescriptor *desc = pbcmsg->descriptor;
  const rw_yang_pb_msgdesc_t *ypbc_mdesc = desc->ypbc_mdesc;
  RW_ASSERT(ypbc_mdesc);
  RW_ASSERT(ypbc_mdesc->num_fields == desc->n_fields);

  // Get the field indices in yang order. Keys first.
  std::vector<int> fields;
  auto nkeys = rw_pb_create_field_indices(ypbc_mdesc, ynode, fields);
  RW_ASSERT(fields.size() == desc->n_fields);

  unsigned op_keys = 0;
  bool new_mode = false;
  bool print_line = true;

  for (size_t i = 0; i < desc->n_fields; i++) {

    const ProtobufCFieldDescriptor *fdesc = &desc->fields[fields[i]-1];
    void *value_ptr = nullptr;
    size_t offset = 0;
    protobuf_c_boolean array_of_ptrs = false;

    size_t count =
        protobuf_c_message_get_field_count_and_offset (pbcmsg, fdesc,
                                                       &value_ptr, &offset,
                                                       &array_of_ptrs);
    if (!count) {
      // Field not set, continue.
      continue;
    }

    YangNode* child_ynode = 
        ynode->search_child_by_mangled_name(fdesc->name);

    if (!child_ynode) {
      return status;
    }

    switch (child_ynode->get_stmt_type()) {
      case RW_YANG_STMT_TYPE_ROOT:
      case RW_YANG_STMT_TYPE_CONTAINER: {

        RW_ASSERT(1 == count);
        print_line = false;

        rw_status_t status = rw_pb_to_cli_internal(child_ynode, (ProtobufCMessage*)value_ptr, 
                                                   commands, oss.str(), indent);

        if (RW_STATUS_SUCCESS != status) {
          return status;
        }

        break;
      }

      case RW_YANG_STMT_TYPE_LEAF: {
        char value_str [RW_PB_MAX_STR_SIZE];
        size_t value_str_len = sizeof(value_str);

        RW_ASSERT(1 == count);
        print_line = true;
        bool ok = protobuf_c_field_get_text_value (NULL, fdesc, value_str, &value_str_len, value_ptr);
        RW_ASSERT(ok);

        bool is_key = !!(fdesc->rw_flags & RW_PROTOBUF_FOPT_KEY);

        if (!is_key) {
          oss << pad << child_ynode->get_name();
        } else {
          op_keys++;
          if (op_keys == nkeys) {
            new_mode = true; // Enter new mode after op keys.
            indent += 2;
            pad = std::string(indent, ' '); // change the indent.
          }
        }

        switch (child_ynode->get_type()->get_leaf_type()) {
           case RW_YANG_LEAF_TYPE_BINARY: {

             RW_ASSERT(value_str_len <= RW_PB_MAX_STR_SIZE);
             auto b64_len = rw_yang_base64_get_encoded_len(value_str_len);
             boost::scoped_array<char> b64_val(new char[b64_len+4]);
             rw_yang_base64_encode(value_str, value_str_len,
                                   b64_val.get(), b64_len+4);

             oss << space << b64_val.get();
             break;
           }
           case RW_YANG_LEAF_TYPE_ENUM: {

            int32_t int_enum = *(int32_t*)((char *)value_ptr);
            std::string text_val = child_ynode->get_enum_string (int_enum);

            if (text_val.length() > value_str_len) {
              return RW_STATUS_FAILURE;
            }

            value_str_len = sprintf (value_str, "%s", text_val.c_str());
            oss << space << value_str;
            break;
           }
           case RW_YANG_LEAF_TYPE_EMPTY:
             break; // leaf name is the presence.
           default: {
             oss << space << value_str;
             break;
           }
        }

        if (new_mode) {
          oss << std::endl;
          commands.push_back(oss.str());
          oss.str("");
        }

        break;
      }

      case RW_YANG_STMT_TYPE_LEAF_LIST: {
        print_line = true;
        for (size_t j = 0; j < count; j++) {
          char value_str [RW_PB_MAX_STR_SIZE];
          size_t value_str_len = sizeof(value_str);
          void * v_ptr = nullptr;

          // Add support latter.
          RW_ASSERT(fdesc->type != PROTOBUF_C_TYPE_BYTES);

          if (array_of_ptrs) {
            v_ptr = *((char **)value_ptr + j);
          }
          else {
            v_ptr = (char *)value_ptr + j * offset;
          }

          if (protobuf_c_field_get_text_value(NULL, fdesc, value_str, &value_str_len, ((char *)v_ptr))) {
            oss << pad << child_ynode->get_name();
            oss << space << value_str;
          }
        }
        break;
      }

      case RW_YANG_STMT_TYPE_LIST: {
        print_line = false;
        for (size_t j = 0; j < count; j++) {
          void *v_ptr = nullptr;

          if (array_of_ptrs) {
            v_ptr = *((char **)value_ptr + j);
          }
          else {
            v_ptr = (char *)value_ptr + j * offset;
          }
          status = rw_pb_to_cli_internal(child_ynode, (ProtobufCMessage *)v_ptr, commands, oss.str(), indent);
          if (status != RW_STATUS_SUCCESS) {
            return status;
          }
        }
        break;
      }

      case RW_YANG_STMT_TYPE_ANYXML:
      case RW_YANG_STMT_TYPE_RPC:
      case RW_YANG_STMT_TYPE_RPCIO:
      case RW_YANG_STMT_TYPE_NOTIF:
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
      case RW_YANG_STMT_TYPE_NA:
      default:
        RW_ASSERT_NOT_REACHED();
    }

  } // for all the fields

  if (new_mode) {
    oss << start << "exit" << std::endl;
    commands.push_back(oss.str());
  } 
  else if (print_line) {
    oss << std::endl;
    commands.push_back(oss.str());
  }

  return RW_STATUS_SUCCESS;
}

char* rw_pb_to_cli(rw_yang_model_t* yang_modelp,
                   const ProtobufCMessage *pbcmsg)
{
  if (!yang_modelp || !pbcmsg) {
    return NULL;
  }

  YangModel* yang_model = static_cast<YangModel *>(yang_modelp);
  YangNode* root_node = yang_model->get_root_node();

  if (!root_node) {
    return NULL;
  }

  if (!pbcmsg->descriptor || !pbcmsg->descriptor->ypbc_mdesc) {
    return NULL;
  }

  auto ypbc_mdesc = pbcmsg->descriptor->ypbc_mdesc;
  if (ypbc_mdesc->msg_type ==  RW_YANGPBC_MSG_TYPE_NOTIFICATION ||
      ypbc_mdesc->msg_type ==  RW_YANGPBC_MSG_TYPE_RPC_INPUT ||
      ypbc_mdesc->msg_type ==  RW_YANGPBC_MSG_TYPE_RPC_OUTPUT) {
    // Not supported.
    return NULL;
  }

  YangNode* top_ynode = 
      root_node->search_child(ypbc_mdesc->yang_node_name, ypbc_mdesc->module->ns);

  if (!top_ynode) {  // Not rooted??
    return NULL;
  }

  auto ynode_type = top_ynode->get_stmt_type();
  if (ynode_type  == RW_YANG_STMT_TYPE_RPC ||
      ynode_type  == RW_YANG_STMT_TYPE_NOTIF ||
      ynode_type  == RW_YANG_STMT_TYPE_RPCIO) { // Redundant check, but its ok.
    // Not supported.
    return NULL;
  }

  std::stringstream oss;
  unsigned indent = 2; //start indent.

  // output commands for confd-cli
  oss << "config" << std::endl;
  rw_status_t status;
  std::vector<std::string> commands;

  status = rw_pb_to_cli_internal(top_ynode, pbcmsg, commands, "", indent);

  if (status != RW_STATUS_SUCCESS) {
    return NULL;
  }

  for (auto const& c: commands) {
    oss << c.c_str();
  }

  oss << "commit" << std::endl;
  oss << "end" << std::endl;

  size_t len = oss.str().size();

  if (len <= 0) {
    return NULL;
  }

  char *cli_str = (char *)malloc(len+1);
  RW_ASSERT(cli_str);
  strncpy(cli_str, oss.str().c_str(), len);
  cli_str[len] = '\0';
  return cli_str;
}
