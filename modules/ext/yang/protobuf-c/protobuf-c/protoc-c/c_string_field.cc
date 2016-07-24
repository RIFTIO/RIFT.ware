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

#include <protoc-c/c_string_field.h>
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

static void SetStringVariables(const FieldDescriptor* descriptor,
                               map<string, string>* variables,
                               struct riftfopts *riftopts) {
  (*variables)["name"] = FieldName(descriptor);
  (*variables)["default"] = FullNameToLower(descriptor->full_name())
        + "__default_value";
  (*variables)["deprecated"] = FieldDeprecated(descriptor);
  (*variables)["stringmax"] = SimpleItoa(riftopts->string_max);
  (*variables)["inlinemax"] = SimpleItoa(riftopts->inline_max);
  (*variables)["c_type"] = std::string(riftopts->c_type);
  (*variables)["c_type_helper"] = ToCamel(std::string(riftopts->c_type) + "_helper");
}

// ===================================================================

StringFieldGenerator::
StringFieldGenerator(const FieldDescriptor* descriptor)
  : FieldGenerator(descriptor) {
  SetStringVariables(descriptor, &variables_, &riftopts);
}

StringFieldGenerator::~StringFieldGenerator() {}

void StringFieldGenerator::AssignStructMembers(io::Printer* printer, int keynum) const
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
      buffer << "strncpy(var_.k.$name$$deprecated$, key" << kn << "_, $stringmax$-1);";
    } else {
      buffer << "var_.k.$name$$deprecated$ = key" << kn << "_;";
    }
  }

  printer->Print(variables_, buffer.str().c_str());
}

void StringFieldGenerator::GenerateStructMembers(io::Printer* printer) const
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
        if (riftopts.c_type[0]) {
          printer->Print(variables_, "$c_type$* $name$$deprecated$;\n");
        } else {
          printer->Print(variables_, "ProtobufCBinaryData $name$$deprecated$;\n");
        }
        break;
      case FieldDescriptor::LABEL_REPEATED:
        if (riftopts.c_type[0]) {
          printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
          printer->Print(variables_, "$c_type$ **$name$$deprecated$;\n");
        } else {
          printer->Print(variables_, "ProtobufCBinaryData *$name$$deprecated$;\n");
        }
        break;
      }
    }
  } else {
    if (riftopts.flatinline) {
      switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_REQUIRED:
        printer->Print(variables_, "char $name$$deprecated$[$stringmax$];/* rift inline */\n");
        break;
      case FieldDescriptor::LABEL_OPTIONAL:
        if (descriptor_->containing_oneof() == NULL)
           printer->Print(variables_, "protobuf_c_boolean has_$name$$deprecated$;\n");
        printer->Print(variables_, "char $name$$deprecated$[$stringmax$];/* rift inline */\n");
        break;
      case FieldDescriptor::LABEL_REPEATED:
        printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
        printer->Print(variables_, "char $name$$deprecated$[$inlinemax$][$stringmax$];/*rift inline */\n");
        break;
      }
    } else {
      switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_REQUIRED:
      case FieldDescriptor::LABEL_OPTIONAL:
        printer->Print(variables_, "char *$name$$deprecated$;\n");
        break;
      case FieldDescriptor::LABEL_REPEATED:
        printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
        printer->Print(variables_, "char **$name$$deprecated$;\n");
        break;
      }
    }
  }
}
void StringFieldGenerator::GenerateDefaultValueDeclarations(io::Printer* printer) const
{
  printer->Print(variables_, "extern char $default$[];\n");
}
void StringFieldGenerator::GenerateDefaultValueImplementations(io::Printer* printer) const
{
  std::map<string, string> vars;
  vars["default"] = variables_.find("default")->second;
  vars["escaped"] = CEscape(descriptor_->default_value_string());
  printer->Print(vars, "char $default$[] = \"$escaped$\";\n");
}

string StringFieldGenerator::GetDefaultValue(void) const
{
  return variables_.find("default")->second;
}
void StringFieldGenerator::GenerateStaticInit(io::Printer* printer) const
{
  std::map<string, string> vars;
  if (descriptor_->has_default_value()) {
    vars["default"] = GetDefaultValue();
    vars["escaped"] = CEscape(descriptor_->default_value_string());
  } else {
    if (!riftopts.flatinline) {
      vars["default"] = "NULL";
    } else {
      vars["default"] = "\"\"";
      vars["escaped"] = "";
    }
  }
  if (isCType()) {
    // ATTN: Support for default value!
    vars["c_type"] = std::string(riftopts.c_type);
    switch (descriptor_->label()) {
      case FieldDescriptor::LABEL_REQUIRED:
        if (riftopts.flatinline) {
          printer->Print(vars, "$c_type$_STATIC_INIT");
        } else {
          printer->Print(vars, "NULL");
        }
        break;
      case FieldDescriptor::LABEL_OPTIONAL:
        if (riftopts.flatinline) {
          printer->Print(vars, "0,$c_type$_STATIC_INIT");
        } else {
          printer->Print(vars, "NULL");
        }
        break;
      case FieldDescriptor::LABEL_REPEATED:
        if (riftopts.flatinline) {
          printer->Print("0,{");
          for (int i=0; i<riftopts.inline_max; i++) {
            printer->Print(vars, "$c_type$_STATIC_INIT,");
          }
          printer->Print("}");
        } else {
          printer->Print(vars, "0,NULL");
        }
        break;
    }
    return;
  }

  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      if (riftopts.flatinline) {
        printer->Print(vars, "\"$escaped$\"");
      } else {
        printer->Print(vars, "$default$");
      }
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (riftopts.flatinline) {
        if (descriptor_->has_default_value()) {
          printer->Print(vars, "1, \"$escaped$\"");
        } else {
          printer->Print(vars, "0, \"$escaped$\"");
        }
      } else {
        printer->Print(vars, "$default$");
      }
      break;
    case FieldDescriptor::LABEL_REPEATED:
      if (!riftopts.flatinline) {
        printer->Print(vars, "0,NULL");
      } else {
        printer->Print(vars, "0,{ }");
      }
      break;
  }
}
void StringFieldGenerator::GenerateDescriptorInitializer(io::Printer* printer) const
{
  int opthas = false;
  if (riftopts.flatinline) {
    opthas = true;
  }
  GenerateDescriptorInitializerGeneric(printer, opthas, "STRING", "NULL");
}

string StringFieldGenerator::GetTypeName() const
{
  if (riftopts.c_type[0]) {
    return "ctype";
  }
  return "string";
}

string StringFieldGenerator::GetPointerType() const
{
  if (isCType()) {
    if (riftopts.flatinline) {
      return string(riftopts.c_type) + "*";
    }
    return string(riftopts.c_type) + "**";
  }

  if (riftopts.flatinline) {
    return "char*";
  }

  return "char**";
}

string StringFieldGenerator::GetGiTypeName(bool use_const) const
{
  if (use_const) {
    return "const gchar *";
  }

  return "gchar*";
}

string StringFieldGenerator::GetGiReturnAnnotations() const
{
  /*
   *  The following annotatons are used for return
   *  lists(inline): (array length=len)(tranfer full)
   *  lists(pointy) : (array length=len)(transfer none)
   */
  string annotations = "";

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    annotations.append("(array length=len)");
    if (riftopts.flatinline || isCType()) {
      annotations.append("(transfer full)");
    } else {
      annotations.append("(transfer none)");
    }
    annotations.append("(nullable)");
  }

  if (annotations.length()) {
    annotations.append(":");
  }
  return annotations;
}

string StringFieldGenerator::GetGiGetterReturnType() const
{
  bool use_const = true;
  if (isCType() || (riftopts.flatinline && descriptor_->label() == FieldDescriptor::LABEL_REPEATED)) {
    use_const = false;
  }
  string return_type = GetGiTypeName(use_const);

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return_type.append("*");
  }
  return_type.append(" ");

  return return_type;
}

string StringFieldGenerator::GetGiGetterParameterList() const
{
  string gi_type_ptr = std::string("(") + FullNameToGI(FieldScope(descriptor_)) + "*";
  string param_list = gi_type_ptr + " boxed";
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    param_list.append(", gint *len");
  }
  param_list.append(", GError **err");
  param_list.append(")");

  return param_list;
}

string StringFieldGenerator::GetGiSetterAnnotations() const
{
  std::string annotations;

  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REPEATED:
      annotations.append("(array length=len)");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (!isCType() && !riftopts.flatinline) {
        annotations.append("(nullable)");
      }
      // ** fall through **
    case FieldDescriptor::LABEL_REQUIRED:
      break;
    default:
      return "";
  }

  if (annotations.length()) {
    annotations.append(":");
  }
  return annotations;
}

string StringFieldGenerator::GetGiSetterParameterList() const
{
  string gi_type_ptr = FullNameToGI(FieldScope(descriptor_)) + "*";
  string param_list = std::string("(") + gi_type_ptr + " boxed, " + GetGiTypeName(false);
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    param_list.append("*");
  }
  param_list.append(" ");
  param_list.append(FieldName(descriptor_));

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    param_list.append(", gint len");
  }

  param_list.append(", GError **err");

  param_list.append(")");

  return param_list;
}

void StringFieldGenerator::GenerateGiCGetterMethod(io::Printer* printer) const
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

  if (isCType()) {
    printer->Print(vars, "const ProtobufCFieldDescriptor* fd = boxed->s.message->base_concrete.descriptor->fields.$fname$;\n"
                         "RW_ASSERT(fd && fd->ctype && fd->ctype->to_string);\n");
  }

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    printer->Print(vars, "*len = $pb_qfield$;\n"
                         "if ($pb_qfield$ == 0) {\n"
                         "  return NULL; \n"
                         "}\n");

    if (riftopts.flatinline || isCType()) {

      printer->Print(vars, "gchar **array = g_new(gchar*, *len); \n"
                           "int i = 0; \n"
                           "for (i = 0; i < *len; i++) { \n");
      if (!isCType()) {
        printer->Print(vars, "  array[i] = strdup($pb_field$[i]); \n"
                             "}\n"
                             "return array;\n");
      } else {

        if (riftopts.flatinline) {
          printer->Print(vars, "  void *value = &$pb_field$[i];\n");
        } else {
          printer->Print(vars, "  void *value = $pb_field$[i];\n");
        }

        printer->Print(vars, "  gchar str_local[RW_PB_MAX_STR_SIZE];\n"
                             "  size_t sz = RW_PB_MAX_STR_SIZE; \n"
                             "  gboolean ret = protobuf_c_field_get_text_value(NULL, fd, str_local, &sz, value);\n"
                             "  RW_ASSERT(ret); \n"
                             "  array[i] = strdup(str_local); \n"
                             "}\n"
                             "return array;\n");
      }
    }
    else {
      printer->Print(vars, "return(($getter_rt$)$pb_field$);\n");
    }
  } else {

    if (isCType()) {
      if (HasQuantifierField()) {
        printer->Print(vars, "if (!$pb_qfield$) {\n"
                             "  return NULL; \n"
                             "}\n");
      }

      if (!riftopts.flatinline) {
         printer->Print(vars, "if (NULL == $pb_field$) {\n"
                              "  return NULL; \n"
                              "}\n"
                              "void *value = $pb_field$;\n");
      } else {
        printer->Print(vars, "void *value = &$pb_field$;\n");
      }

      printer->Print(vars, "gchar str_local[RW_PB_MAX_STR_SIZE];\n"
                           "size_t sz = RW_PB_MAX_STR_SIZE; \n"
                           "gboolean ret = protobuf_c_field_get_text_value(NULL, fd, str_local, &sz, value);\n"
                           "RW_ASSERT(ret); \n"
                           "return strdup(str_local); \n");

    } else {
      printer->Print(vars, "return(($getter_rt$)$pb_field$);\n");
    }
  }

  printer->Outdent();
  printer->Print("}\n\n");
  return;
}

void StringFieldGenerator::GenerateGiCSetterMethod(io::Printer* printer) const
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

    printer->Print("size_t i = 0;\n");

    if (isCType()) {
      printer->Print(vars, "RW_ASSERT(fd->ctype);\n"
                           "for (i = 0; i < len; i++) {\n"
                           "  if (!protobuf_c_message_set_field_text_value(NULL, &boxed->s.message->base, fd, $fname$[i], strlen($fname$[i]))) {\n"
                           "    PROTOBUF_C_GI_RAISE_EXCEPTION(err, $domain$, PROTO_GI_ERROR_FAILURE, \"Failed set_field CType with status %d\\n\", 0);\n"
                           "   protobuf_c_message_delete_field(NULL, &boxed->s.message->base, fd->id);\n" // To make sure any previously set list elements are cleared.
                           "   break; \n"
                           "  }\n"
                           "}\n");
    } else {

      if (riftopts.flatinline) {

        vars["inline_max"] = SimpleItoa(riftopts.inline_max);
        vars["string_max"] = SimpleItoa(riftopts.string_max);

        printer->Print(vars, "RW_ASSERT(len <= $inline_max$);\n" // Return error instead of assert??
                             "$pb_qfield$  = ($fname$ == NULL) ? 0: len;\n"
                             "for (i = 0; i < len; i++) {\n"
                             "  memcpy($pb_field$[i], $fname$[i], sizeof($pb_field$[i])); \n"
                             "  $pb_field$[i][sizeof($pb_field$[i])-1] = '\\0';\n"
                             "}\n");
      }
      else {

        printer->Print(vars, "$pb_qfield$ = $fname$ == NULL ? 0: len;\n"
                             "$pb_field$ = g_new(gchar*, len); \n"
                             "for (i = 0; i < len; i++) {\n"
                             " $pb_field$[i] = g_strdup ($fname$[i]); \n"
                             "}\n");
      }
    }
  } else {

    if (isCType()) {
      printer->Print(vars, "if (!protobuf_c_message_set_field_text_value(NULL, &boxed->s.message->base, fd, $fname$, strlen($fname$))) {\n"
                           "   PROTOBUF_C_GI_RAISE_EXCEPTION(err, $domain$, PROTO_GI_ERROR_FAILURE, \"Failed set_field CType with status %d\\n\", 0);\n"
                           "}\n");
    } else {
      if (riftopts.flatinline) {
        printer->Print(vars, "strncpy($pb_field$, $fname$, sizeof($pb_field$)-1);\n"
                             "$pb_field$[sizeof($pb_field$)-1] = '\\0';\n");
      } else {
        printer->Print(vars, "$pb_field$ = $fname$ == NULL ? NULL: g_strdup($fname$);\n");
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
