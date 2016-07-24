// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

// Copyright (c) 2008-2013, Dave Benson.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified to implement C code by Dave Benson.

#include <protoc-c/c_bytes_field.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/descriptor.pb.h>
#include <stdio.h>
#include "../protobuf-c/rift-protobuf-c.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

using internal::WireFormat;

static void SetBytesVariables(const FieldDescriptor* descriptor,
                              map<string, string>* variables,
                              struct riftfopts *riftopts) {
  (*variables)["name"] = FieldName(descriptor);
  (*variables)["default"] =
    "\"" + CEscape(descriptor->default_value_string()) + "\"";
  (*variables)["deprecated"] = FieldDeprecated(descriptor);
  (*variables)["stringmax"] = SimpleItoa(riftopts->string_max);
  (*variables)["inlinemax"] = SimpleItoa(riftopts->inline_max);
  (*variables)["c_type"] = std::string(riftopts->c_type);
}

// ===================================================================

BytesFieldGenerator::
BytesFieldGenerator(const FieldDescriptor* descriptor)
  : FieldGenerator(descriptor) {
  SetBytesVariables(descriptor, &variables_, &riftopts);
  variables_["default_value"] = descriptor->has_default_value()
                              ? GetDefaultValue() 
                              : string("{0,NULL}");
}

BytesFieldGenerator::~BytesFieldGenerator() {}

void BytesFieldGenerator::AssignStructMembers(io::Printer* printer, int keynum) const
{
  char kn[10];
  if (keynum != -1) {
    sprintf(kn, "%d", keynum);
  } else {
    kn[0] = 0;
  }

  std::ostringstream buffer;
  if (riftopts.c_type[0]) {
    buffer << "var_.k.$name$$deprecated$ = key" << kn << "_;";
    /* ATTN: Should use deep_copy? */
  } else {
    if (riftopts.flatinline) {
      buffer << "memcpy(var_.k.$name$$deprecated$, key" << kn << "_, keylen" << kn << "_;";
      /* ATTN: Should memset remainder? */
      buffer << "var_k.$name$$deprecated$_len = keylen" << kn << "_;";
    }
    else {
      buffer << "var_.k.$name$$deprecated$ = key" << kn << "_;";
      buffer << "var_.k.$name$$deprecated$_len = keylen" << kn << "_;";
    }
  }

  printer->Print(variables_, buffer.str().c_str());
}

void BytesFieldGenerator::GenerateMiniKeyStructMembers(io::Printer* printer) const
{
  if (riftopts.c_type[0]) {
    if (riftopts.flatinline) {
      printer->Print(variables_, "$c_type$ $name$$deprecated$;\n");
    } else {
      printer->Print(variables_, "$c_type$* $name$$deprecated$;\n");
    }
  } else {
    if (riftopts.flatinline) {
      printer->Print(variables_, "uint8_t $name$$deprecated$[$stringmax$];\n");
      printer->Print(variables_, "size_t $name$$deprecated$_len;\n");
    } else {
      printer->Print(variables_, "uint8_t *$name$$deprecated$;\n");
      printer->Print(variables_, "size_t $name$$deprecated$_len;\n");
    }
  }
}

void BytesFieldGenerator::GenerateStructMembers(io::Printer* printer) const
{
  if (riftopts.c_type[0]) {
    if (riftopts.flatinline) {
      switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_OPTIONAL:
        if (descriptor_->containing_oneof() == NULL)
          printer->Print(variables_, "protobuf_c_boolean has_$name$$deprecated$;\n");
        // fallthru
      case FieldDescriptor::LABEL_REQUIRED:
        printer->Print(variables_, "$c_type$ $name$$deprecated$;\n");
        break;
      case FieldDescriptor::LABEL_REPEATED:
        printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
        printer->Print(variables_, "$c_type$  $name$$deprecated$[$inlinemax$];\n");
        break;
      }
    } else {
      switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_OPTIONAL:
      case FieldDescriptor::LABEL_REQUIRED:
        printer->Print(variables_, "$c_type$* $name$$deprecated$;\n");
        break;
      case FieldDescriptor::LABEL_REPEATED:
        printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
        printer->Print(variables_, "$c_type$ **$name$$deprecated$;\n");
        break;
      }
    }
  } else {
    if (riftopts.flatinline) {
      switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_OPTIONAL:
        if (descriptor_->containing_oneof() == NULL)
           printer->Print(variables_, "protobuf_c_boolean has_$name$$deprecated$;\n");
        // fallthru
      case FieldDescriptor::LABEL_REQUIRED:
        printer->Print(variables_, "union {\n");
        printer->Print(variables_, "  struct {\n");
        printer->Print(variables_, "    size_t len;\n");
        printer->Print(variables_, "    uint8_t data[$stringmax$];\n");
        printer->Print(variables_, "  } _placeholder_$name$$deprecated$;\n");
        printer->Print(variables_, "  ProtobufCFlatBinaryData $name$$deprecated$;/*rift inline*/\n");
        printer->Print(variables_, "};\n");
        break;
      case FieldDescriptor::LABEL_REPEATED:
        printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
        printer->Print(variables_, "union {\n");
        printer->Print(variables_, "  struct {\n");
        printer->Print(variables_, "    size_t len;\n");
        printer->Print(variables_, "    uint8_t data[$stringmax$];\n");
        printer->Print(variables_, "  };\n");
        printer->Print(variables_, "  ProtobufCFlatBinaryData meh;//$name$$deprecated$;/*rift inline*/\n");
        printer->Print(variables_, "} $name$$deprecated$[$inlinemax$];\n");
        break;
      }
    } else {
      switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_REQUIRED:
        printer->Print(variables_, "ProtobufCBinaryData $name$$deprecated$;\n");
        break;
      case FieldDescriptor::LABEL_OPTIONAL:
        if (descriptor_->containing_oneof() == NULL)
          printer->Print(variables_, "protobuf_c_boolean has_$name$$deprecated$;\n");
        printer->Print(variables_, "ProtobufCBinaryData $name$$deprecated$;\n");
        break;
      case FieldDescriptor::LABEL_REPEATED:
        printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
        printer->Print(variables_, "ProtobufCBinaryData *$name$$deprecated$;\n");
        break;
      }
    }
  }
}

void BytesFieldGenerator::GenerateDefaultValueDeclarations(io::Printer* printer) const
{
  std::map<string, string> vars;
  vars["default_value_data"] = FullNameToLower(descriptor_->full_name())
                             + "__default_value_data";
  vars["c_type"] = std::string(riftopts.c_type);

  if (!isCType()) {
    printer->Print(vars, "extern uint8_t $default_value_data$[];\n");
  }
}

void BytesFieldGenerator::GenerateDefaultValueImplementations(io::Printer* printer) const
{
  std::map<string, string> vars;
  vars["default_value_data"] = FullNameToLower(descriptor_->full_name())
                             + "__default_value_data";
  vars["escaped"] = CEscape(descriptor_->default_value_string());
  vars["c_type"] = std::string(riftopts.c_type);

  if (!isCType()) {
    printer->Print(vars, "uint8_t $default_value_data$[] = \"$escaped$\";\n");
  }
}
string BytesFieldGenerator::GetDefaultValue(void) const
{
  if (isCType()) {
    // ATTN: Need to implement sensible default value for binary c-type!
    return std::string(" NULL");
  }

  return "{ "
    + SimpleItoa(descriptor_->default_value_string().size())
    + ", "
    + FullNameToLower(descriptor_->full_name())
    + "__default_value_data }";
}
void BytesFieldGenerator::GenerateStaticInit(io::Printer* printer) const
{
  std::map<string, string> vars;

  if (isCType()) {
    // ATTN: Support for default value!
    vars["c_type"] = std::string(riftopts.c_type);
    if (riftopts.flatinline) {
      switch (descriptor_->label()) {
        case FieldDescriptor::LABEL_OPTIONAL:
          printer->Print(vars, "0,$c_type$_STATIC_INIT");
          return;
        case FieldDescriptor::LABEL_REQUIRED:
          printer->Print(vars, "$c_type$_STATIC_INIT");
          return;
        case FieldDescriptor::LABEL_REPEATED:
          printer->Print("0,{");
          for (int i=0; i<riftopts.inline_max; i++) {
            printer->Print(vars, "$c_type$_STATIC_INIT,");
          }
          printer->Print("}");
          return;
      }
    }
    switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_OPTIONAL:
      case FieldDescriptor::LABEL_REQUIRED:
        printer->Print(vars, "NULL");
        return;
      case FieldDescriptor::LABEL_REPEATED:
        printer->Print(vars, "0,NULL");
        return;
    }
    return;
  }

  if (descriptor_->has_default_value()) {
    if (riftopts.flatinline) {
      vars["default_value"]
        =   string("{ { ")
          + SimpleItoa(descriptor_->default_value_string().size())
          + ", \""
          + CEscape(descriptor_->default_value_string())
          + "\" } }";
    } else {
      vars["default_value"] = GetDefaultValue();
    }
  } else {
    if (riftopts.flatinline) {
      vars["default_value"] = "{ { 0, \"\" } }";
    } else {
      vars["default_value"] = "{ 0, NULL }";
    }
  }

  if (riftopts.flatinline) {
    switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_OPTIONAL:
      printer->Print(vars, "0,$default_value$");
      break;
    case FieldDescriptor::LABEL_REQUIRED:
      printer->Print(vars, "$default_value$");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      // no support for default?
      printer->Print("0, { }");
      break;
    }
  } else {
    switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      printer->Print(vars, "$default_value$");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      printer->Print(vars, "0, $default_value$");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      // no support for default?
      printer->Print("0,NULL");
      break;
    }
  }
}
void BytesFieldGenerator::GenerateDescriptorInitializer(io::Printer* printer) const
{
  GenerateDescriptorInitializerGeneric(printer, true, "BYTES", "NULL");
}

string BytesFieldGenerator::GetTypeName() const
{
  if (isCType()) {
    return "ctype";
  }
  return "bytes";
}

string BytesFieldGenerator::GetPointerType() const
{
  if (isCType()) {
    if (riftopts.flatinline) {
      return string(riftopts.c_type) + "*";
    }
    return string(riftopts.c_type) + "**";
  }

  if (riftopts.flatinline) {
    return "ProtobufCFlatBinaryData*";
  }

  return "ProtobufCBinaryData*";
}

string BytesFieldGenerator::GetGiTypeName(bool use_const) const
{
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return "GPtrArray";
  }
  if (use_const) {
    return "const guint8 *";
  }
  return "guint8*";
}

string BytesFieldGenerator::GetGiReturnAnnotations() const
{
  /*
   *  The following annotatons are used for return
   *  list: (element-type GLib.ByteArray)(tranfer full)
   */
  string annotations = "";

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    annotations.append("(transfer full) (element-type GLib.ByteArray)");
  } else {
    annotations.append("(array length=len)(transfer none)");
  }

  annotations.append("(nullable)");

  if (annotations.length()) {
    annotations.append(":");
  }

  return annotations;
}

string BytesFieldGenerator::GetGiGetterReturnType() const
{
  bool use_const = true;
  if (isCType()) {
    use_const = false;
  }
  string return_type = GetGiTypeName(use_const);
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return_type.append("*");
  }
  return_type.append(" ");

  return return_type;
}

string BytesFieldGenerator::GetGiGetterParameterList() const
{
  string gi_type_ptr = std::string("(") + FullNameToGI(FieldScope(descriptor_)) + "*";
  string param_list = gi_type_ptr + " boxed";

  if (descriptor_->label() != FieldDescriptor::LABEL_REPEATED) {
    param_list.append(", gint *len");
  }

  param_list.append(", GError **err");
  param_list.append(")");

  return param_list;
}

string BytesFieldGenerator::GetGiSetterAnnotations() const
{
  std::string annotations = "";

  annotations.append("(transfer none)");
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    annotations.append("(element-type GLib.ByteArray)");
  } else {
    annotations.append("(array length=len)");
  }

  if (annotations.length()) {
    annotations.append(":");
  }
  return annotations;
}

string BytesFieldGenerator::GetGiSetterParameterList() const
{
  string gi_type_ptr = FullNameToGI(FieldScope(descriptor_)) + "*";
  string param_list = std::string("(") + gi_type_ptr + " boxed, " + GetGiTypeName(false);
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    param_list.append("*");
  }
  param_list.append(" ");
  param_list.append(FieldName(descriptor_));

  if (descriptor_->label() != FieldDescriptor::LABEL_REPEATED) {
    param_list.append(", gint len");
  }

  param_list.append(", GError **err");

  param_list.append(")");

  return param_list;
}

bool BytesFieldGenerator::HasLengthOut() const
{
  if (descriptor_->label() != FieldDescriptor::LABEL_REPEATED) {
    return true;
  }
  return false;
}

void BytesFieldGenerator::GenerateGiCGetterMethod(io::Printer* printer) const
{
  std::map<string, string> vars;

  vars["pb_field"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + FieldName(descriptor_);
  vars["boxed_field"] = std::string("boxed->") + GetGiBoxedFieldName();
  vars["pb_qfield"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + GetQuantifierField();
  vars["fname"] = FieldName(descriptor_);

  // Output the getter definitions
  vars["getter_rt"] = GetGiGetterReturnType();
  string getter_func = GetParentGiCIdentifier("get", FieldName(descriptor_));
  if (FieldName(descriptor_) == "type") {
    getter_func.append("_yang"); // Disambiguate with an _yang
  }
  vars["getter_fn"] = getter_func;
  
  if (descriptor_->file()->package().length()) {
    vars["domain"] = MangleNameToUpper(descriptor_->file()->package());
  } else {
    vars["domain"] = MangleNameToUpper(StripProto(descriptor_->file()->name()));
  }

  printer->Print(vars, "$getter_rt$ $getter_fn$");
  printer->Print(GetGiGetterParameterList().c_str());
  printer->Print("\n");

  // Begining Brace of the function
  printer->Print(vars, "{\n"
                       "  /* Return 0 value for any return type */\n"
                       "  PROTOBUF_C_GI_CHECK_FOR_ERROR_RETVAL(boxed, err, $domain$, ($getter_rt$)0);\n\n");
  printer->Indent();

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {

    printer->Print(vars, " if ($pb_qfield$ == 0) {\n"
                         "    return NULL;\n"
                         " }\n"
                         " int i = 0; \n"
                         " gint len = $pb_qfield$; \n"
                         " GPtrArray *l_bytes = g_ptr_array_sized_new(len);\n");
    if (isCType()) {
      printer->Print(vars, " const ProtobufCFieldDescriptor *fd = boxed->s.message->base_concrete.descriptor->fields.$fname$;\n"
                           " RW_ASSERT(fd && fd->ctype);\n");
    }

    printer->Print(vars, "for (i = 0; i < len; i++) { \n");

    if (isCType()) {
      if (riftopts.flatinline) {
        printer->Print(vars, "  void *value = &$pb_field$[i];\n");
      } else {
        printer->Print(vars, "  void *value = $pb_field$[i];\n");
      }
      printer->Print(vars, "  size_t sz = fd->ctype->get_packed_size(fd->ctype, fd, value);\n"
                           "  guint8 *data = g_malloc0(sz);\n"
                           "  fd->ctype->pack(fd->ctype, fd, value, data); \n"
                           "  g_ptr_array_add(l_bytes, g_byte_array_new_take(data, sz));\n");
    } else {
      printer->Print(vars, "  g_ptr_array_add(l_bytes, g_byte_array_new_take(g_memdup($pb_field$[i].data, $pb_field$[i].len), $pb_field$[i].len));\n");
    }

    printer->Print(vars, "}\n"
                         "return l_bytes; \n");

  } else {

    if (HasQuantifierField()) {
      printer->Print(vars, "if (!$pb_qfield$) {\n"
                           "  *len = 0;\n"
                           "  return NULL;\n"
                           "}\n");
    }
    if (isCType()) {
      if (!riftopts.flatinline) {
        printer->Print(vars, "if (NULL == $pb_field$) {\n"
                             "  *len = 0; \n"
                             "  return NULL; \n"
                             "}\n");  
      }

      printer->Print(vars, "const ProtobufCFieldDescriptor* fd = boxed->s.message->base_concrete.descriptor->fields.$fname$;\n"
                           "RW_ASSERT(fd && fd->ctype && fd->ctype->to_string);\n");

      if (riftopts.flatinline) {
        printer->Print(vars, "void *value = &$pb_field$;\n");
      } else {
        printer->Print(vars, "void *value = $pb_field$;\n");
      }

      printer->Print(vars, "*len = fd->ctype->get_packed_size(fd->ctype, fd, value);\n"
                           "guint8 *data = g_malloc0(*len);\n"
                           "fd->ctype->pack(fd->ctype, fd, value, data); \n"
                           "return data; \n");
                           
    } else {
      printer->Print(vars, " *len = $pb_field$.len; \n"
                           " if ($pb_field$.len == 0) {\n"
                           "   return NULL; \n"
                           " } \n"
                           " return($pb_field$.data);\n");
    }
  }
  printer->Outdent();
  printer->Print("}\n\n");
  return;
}

void BytesFieldGenerator::GenerateGiCSetterMethod(io::Printer* printer) const
{
  std::map<string, string> vars;

  vars["pb_field"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + FieldName(descriptor_);
  vars["boxed_field"] = std::string("boxed->") + GetGiBoxedFieldName();
  vars["pb_qfield"] =  GetParentGiCIdentifier("helper_macro") + "(boxed)->" + GetQuantifierField();
  string setter_func = GetParentGiCIdentifier("set", FieldName(descriptor_));
  if (FieldName(descriptor_) == "type") {
    setter_func.append("_yang"); // Disambiguate with an _yang
  }
  vars["setter_fn"] = setter_func;
  vars["fname"] = FieldName(descriptor_);
  if (descriptor_->file()->package().length()) {
    vars["domain"] = MangleNameToUpper(descriptor_->file()->package());
  } else {
    vars["domain"] = MangleNameToUpper(StripProto(descriptor_->file()->name()));
  }
  vars["string_max"] = SimpleItoa(riftopts.string_max);
  vars["inline_max"] = SimpleItoa(riftopts.inline_max);

  // Output the setter definitions
  printer->Print(vars, "void $setter_fn$");
  printer->Print(GetGiSetterParameterList().c_str());

  // Begining Brace of the function
  printer->Print("{\n");

  printer->Indent();
  printer->Print(vars, "const ProtobufCFieldDescriptor* fd = boxed->s.message->base_concrete.descriptor->fields.$fname$;\n"
                       "RW_ASSERT(fd);\n"
                       "protobuf_c_message_delete_field(NULL, &boxed->s.message->base, fd->id);\n");

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {


    if (isCType()) {
      printer->Print(vars, "RW_ASSERT(fd->ctype);\n"
                           "size_t i; \n"
                           "for (i = 0; i < $fname$->len; i++) {\n"
                           "  GByteArray *bytes = (GByteArray *)($fname$->pdata[i]); \n"
                           "  if (!protobuf_c_message_set_field_text_value(NULL, &boxed->s.message->base, fd, (char *)bytes->data, bytes->len)) {\n"
                           "    PROTOBUF_C_GI_RAISE_EXCEPTION(err, $domain$, PROTO_GI_ERROR_FAILURE, \"Failed set_field CType with status %d\\n\", 0);\n"
                           "   protobuf_c_message_delete_field(NULL, &boxed->s.message->base, fd->id);\n" // To make sure any previously set list elements are cleared.
                           "   break; \n"
                           "  }\n"
                           "}\n");
    } else {

      printer->Print(vars, "$pb_qfield$  = ($fname$ == NULL) ? 0: $fname$->len;\n"
                           "size_t i;\n");

      if (riftopts.flatinline) {
        printer->Print(vars, "RW_ASSERT($fname$->len <= $inline_max$); \n"
                             "for (i = 0; i < $fname$->len; i++) {\n"
                             "  GByteArray *bytes = (GByteArray *)($fname$->pdata[i]); \n"
                             "  RW_ASSERT(bytes->len <= $string_max$);\n"
                             "  $pb_field$[i].len = bytes->len; \n"
                             "  memcpy($pb_field$[i].data, bytes->data, bytes->len); \n"
                             "}\n");

      } else {
        printer->Print(vars, "$pb_field$ = g_new(ProtobufCBinaryData, $fname$->len);\n"
                             "for (i = 0; i < $fname$->len; i++) {\n"
                             "  GByteArray *bytes = (GByteArray *)($fname$->pdata[i]); \n"
                             "  $pb_field$[i].len = bytes->len; \n"
                             "  $pb_field$[i].data = g_memdup(bytes->data, bytes->len); \n"
                             "}\n");
      }
    }

  } else {

    if (isCType()) {
      printer->Print(vars, "if (!protobuf_c_message_set_field_text_value(NULL, &boxed->s.message->base, fd, (char *)$fname$, len)) {\n"
                           "   PROTOBUF_C_GI_RAISE_EXCEPTION(err, $domain$, PROTO_GI_ERROR_FAILURE, \"Failed set_field CType with status %d\\n\", 0);\n"
                           "}\n");
    } else {
      if (riftopts.flatinline) {
        printer->Print(vars, "RW_ASSERT(len <= $string_max$);\n"
                             "memcpy($pb_field$.data, $fname$, len);\n"
                             "$pb_field$.len = len;\n");
      }
      else {
        printer->Print(vars, "$pb_field$.len = len;\n"
                             "if ($pb_field$.len) {\n"
                             " $pb_field$.data = g_memdup($fname$, len); \n"
                             "} else {\n" 
                             "  $pb_field$.data = NULL;\n"
                             "}\n");
      }
      if (HasQuantifierField()) {
        printer->Print(vars, "$pb_qfield$ = TRUE;\n");
      }
    }
  }

  printer->Outdent();
  printer->Print("}\n");
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
