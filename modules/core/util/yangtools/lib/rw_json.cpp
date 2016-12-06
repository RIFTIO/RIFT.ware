
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */



/**
 * @file rw_json.cpp
 * @author Sameer Nayak
 * @date 29/07/2015
 * @brief Implementation of generic RW JSON library functions
 */

#include "rw_xml.h"
#include <iostream>
#include <inttypes.h>
#include <boost/scoped_array.hpp>

#include "rw_json.h"
using namespace rw_yang;
static int 
rw_json_pbcm_to_json_int(const ProtobufCMessage *msg,
                          const ProtobufCMessageDescriptor *desc,
                          std::ostringstream &s_val); 
static inline std::string c_escape_string(
  const char* input,
  unsigned len)
{
  // Maximumun possible length. All characters could be non-printable plus 1
  // byte for null termination.
  unsigned dest_len = (len * 4) + 1;
  boost::scoped_array<char> dest(new char[dest_len]);
  RW_ASSERT(dest);
  unsigned used = 0;
  for (unsigned i = 0; i < len; i++) {
    switch(input[i]) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n'; break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r'; break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't'; break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default: {
        if (!isprint(input[i])) {
          char oct[10];
          // octal escaping for non printable characters.
          sprintf(oct, "\\%03o", static_cast<uint8_t>(input[i]));
          for (unsigned j = 0; j < 4; j++) {
            dest[used++] = oct[j];
          }
        } else {
          dest[used++] = input[i];
        }
      }
    }
  }
  RW_ASSERT(used <= dest_len);
  dest[used] = '\0';
  return std::string(dest.get());
}

/*
 * rw_json_pbcm_to_json: Converts pb to json
 * Input:
 * @msg : protomsg
 * @desc: descriptor to be used. Uses msg->descriptor if null
 * @out_string: output String
 *
 * Returns : number of characters written
 */
int 
rw_json_pbcm_to_json(const ProtobufCMessage *msg,
                            const ProtobufCMessageDescriptor *desc,
                            char **out_string)
{
  int bytes = 0;
  std::ostringstream s_val;
  if (!desc)
  {
    desc = msg->descriptor;
  }
  bytes = rw_json_pbcm_to_json_int(msg, desc, s_val);
  *out_string = strdup(s_val.str().c_str());
  return bytes;
}
int 
rw_json_pbcm_to_json_int(const ProtobufCMessage *msg,
                            const ProtobufCMessageDescriptor *desc,
                            std::ostringstream &s_val)
{
  const ProtobufCFieldDescriptor *fd = NULL;
  void *field_ptr = NULL;
  size_t off = 0;
  protobuf_c_boolean is_dptr = false;
  //std::ostringstream s_val;
  int position = 0;
  size_t count = 0;
  unsigned i = 0, j = 0;
  bool field_found = false;
  s_val << "{";
  for (i = 0; i < desc->n_fields; i++)
  {
    fd = &desc->fields[i];

    field_ptr = NULL;
    count = protobuf_c_message_get_field_count_and_offset(msg, fd, &field_ptr, &off, &is_dptr);
    if (count)
    {
      if (field_found)
      {
        s_val << ", ";
      }
      s_val << "\"" << fd->name  << "\"" << ": ";
      if(fd->label == PROTOBUF_C_LABEL_REPEATED)
      {
        s_val << "[";
      }
    }
    for (j = 0; j < count; j++)
    {
      if (j)
      {
        s_val << ", ";
      }
      switch(fd->type)
      {
        case PROTOBUF_C_TYPE_INT32:
        case PROTOBUF_C_TYPE_SINT32:
        case PROTOBUF_C_TYPE_SFIXED32:
        case PROTOBUF_C_TYPE_UINT32:
        case PROTOBUF_C_TYPE_FIXED32:
        case PROTOBUF_C_TYPE_BOOL:
        case PROTOBUF_C_TYPE_ENUM:
        case PROTOBUF_C_TYPE_FLOAT:
        case PROTOBUF_C_TYPE_DOUBLE:
        case PROTOBUF_C_TYPE_INT64:
        case PROTOBUF_C_TYPE_SINT64:
        case PROTOBUF_C_TYPE_SFIXED64:
        case PROTOBUF_C_TYPE_FIXED64:
        case PROTOBUF_C_TYPE_UINT64:
          {
            char val[128];
            size_t len = sizeof(val);
            if (protobuf_c_field_get_text_value(NULL, fd, val, &len, field_ptr)) 
            {
              s_val << val;
            }
          }
          break;
        case PROTOBUF_C_TYPE_STRING:
          {
            char val[128];
            size_t len = sizeof(val);
            if (protobuf_c_field_get_text_value(NULL, fd, val, &len, field_ptr)) 
            {
              s_val << "\"" << val << "\"";
            }
          }
          break;
        case PROTOBUF_C_TYPE_MESSAGE:
          {
            ProtobufCFieldInfo fi = {};
            protobuf_c_message_get_field_instance(NULL, msg, fd, j, &fi);
            RW_ASSERT(fi.element);
            position += rw_json_pbcm_to_json_int((const ProtobufCMessage *)fi.element, fd->msg_desc, s_val); 
          }
          break;
        case PROTOBUF_C_TYPE_BYTES:
          //s_val << "\""<< c_escape_string((const char *)field_ptr, strlen((char *)field_ptr)).c_str() << "\"" ;
          break;
        default:
          RW_CRASH();
          break;
      }
    } //for count
    if (count)
    {
      if (fd->label == PROTOBUF_C_LABEL_REPEATED)
      {
        s_val << "]";
      }
      field_found = true;
    }
  } //for n_fields loop
  s_val << "}";
  return position;
}
