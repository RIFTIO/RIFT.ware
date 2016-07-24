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

#include <protoc-c/c_primitive_field.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <stdio.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

PrimitiveFieldGenerator::
PrimitiveFieldGenerator(const FieldDescriptor* descriptor)
  : FieldGenerator(descriptor) {
}

PrimitiveFieldGenerator::~PrimitiveFieldGenerator() {}

void PrimitiveFieldGenerator::AssignStructMembers(io::Printer* printer, int num) const
{
  char kn[10];

  if (num != -1) {
    sprintf(kn, "%d", num);
  } else {
    kn[0] = 0;
  }

  map<string, string> vars;
  vars["name"] = FieldName(descriptor_);
  vars["deprecated"] = FieldDeprecated(descriptor_);

  std::ostringstream buffer;
  buffer << "var_.k.$name$$deprecated$ = key" << kn << "_;";
  printer->Print(vars, buffer.str().c_str());
}


void PrimitiveFieldGenerator::GenerateStructMembers(io::Printer* printer) const
{
  string c_type;
  map<string, string> vars;
  switch (descriptor_->type()) {
    case FieldDescriptor::TYPE_SINT32  : 
    case FieldDescriptor::TYPE_SFIXED32: 
    case FieldDescriptor::TYPE_INT32   : c_type = "int32_t"; break;
    case FieldDescriptor::TYPE_SINT64  : 
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64   : c_type = "int64_t"; break;
    case FieldDescriptor::TYPE_UINT32  : 
    case FieldDescriptor::TYPE_FIXED32 : c_type = "uint32_t"; break;
    case FieldDescriptor::TYPE_UINT64  : 
    case FieldDescriptor::TYPE_FIXED64 : c_type = "uint64_t"; break;
    case FieldDescriptor::TYPE_FLOAT   : c_type = "float"; break;
    case FieldDescriptor::TYPE_DOUBLE  : c_type = "double"; break;
    case FieldDescriptor::TYPE_BOOL    : c_type = "protobuf_c_boolean"; break;
    case FieldDescriptor::TYPE_ENUM    : 
    case FieldDescriptor::TYPE_STRING  :
    case FieldDescriptor::TYPE_BYTES   :
    case FieldDescriptor::TYPE_GROUP   :
    case FieldDescriptor::TYPE_MESSAGE : GOOGLE_LOG(FATAL) << "not a primitive type"; break;

    // No default because we want the compiler to complain if any new
    // types are added.
  }
  vars["c_type"] = c_type;
  vars["name"] = FieldName(descriptor_);
  vars["deprecated"] = FieldDeprecated(descriptor_);
  vars["inline_max"] = SimpleItoa(riftopts.inline_max);

  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      printer->Print(vars, "$c_type$ $name$$deprecated$;\n");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (descriptor_->containing_oneof() == NULL)
        printer->Print(vars, "protobuf_c_boolean has_$name$$deprecated$;\n");
      printer->Print(vars, "$c_type$ $name$$deprecated$;\n");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      printer->Print(vars, "size_t n_$name$$deprecated$;\n");
      if (riftopts.flatinline) {
	printer->Print(vars, "$c_type$ $name$$deprecated$[$inline_max$]/*rift inline*/;\n");
      } else {
	printer->Print(vars, "$c_type$ *$name$$deprecated$;\n");
      }
      break;
  }
}
string PrimitiveFieldGenerator::GetDefaultValue() const
{
  /* XXX: SimpleItoa seems woefully inadequate for anything but int32,
   * but that's what protobuf uses. */
  switch (descriptor_->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return SimpleItoa(descriptor_->default_value_int32());
    case FieldDescriptor::CPPTYPE_INT64:
      return SimpleItoa(descriptor_->default_value_int64()) + "ll";
    case FieldDescriptor::CPPTYPE_UINT32:
      return SimpleItoa(descriptor_->default_value_uint32()) + "u";
    case FieldDescriptor::CPPTYPE_UINT64:
      return SimpleItoa(descriptor_->default_value_uint64()) + "ull";
    case FieldDescriptor::CPPTYPE_FLOAT:
      return SimpleFtoa(descriptor_->default_value_float());
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return SimpleDtoa(descriptor_->default_value_double());
    case FieldDescriptor::CPPTYPE_BOOL:
      return descriptor_->default_value_bool() ? "1" : "0";
    default:
      GOOGLE_LOG(DFATAL) << "unexpected CPPTYPE in c_primitive_field";
      return "UNEXPECTED_CPPTYPE";
  }
}
void PrimitiveFieldGenerator::GenerateStaticInit(io::Printer* printer) const
{
  map<string, string> vars;
  if (descriptor_->has_default_value()) {
    vars["default_value"] = GetDefaultValue();
  } else {
    vars["default_value"] = "0";
  }
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      printer->Print(vars, "$default_value$");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      printer->Print(vars, "0,$default_value$");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      if (riftopts.flatinline) {
	printer->Print("0, { }");
      } else {
	printer->Print("0,NULL");
      }
      break;
  }
}

void PrimitiveFieldGenerator::GenerateDescriptorInitializer(io::Printer* printer) const
{
  string c_type_macro;
  switch (descriptor_->type()) {
  #define WRITE_CASE(shortname) case FieldDescriptor::TYPE_##shortname: c_type_macro = #shortname; break;
    WRITE_CASE(INT32)
    WRITE_CASE(SINT32)
    WRITE_CASE(UINT32)
    WRITE_CASE(SFIXED32)
    WRITE_CASE(FIXED32)

    WRITE_CASE(INT64)
    WRITE_CASE(SINT64)
    WRITE_CASE(UINT64)
    WRITE_CASE(FIXED64)
    WRITE_CASE(SFIXED64)

    WRITE_CASE(FLOAT)
    WRITE_CASE(DOUBLE)

    WRITE_CASE(BOOL)
  #undef WRITE_CASE

    case FieldDescriptor::TYPE_ENUM    : 
    case FieldDescriptor::TYPE_STRING  :
    case FieldDescriptor::TYPE_BYTES   :
    case FieldDescriptor::TYPE_GROUP   :
    case FieldDescriptor::TYPE_MESSAGE : GOOGLE_LOG(FATAL) << "not a primitive type"; break;

    // No default because we want the compiler to complain if any new
    // types are added.
  }
  bool uses_has = true;
  GenerateDescriptorInitializerGeneric(printer, uses_has, c_type_macro, "NULL");
}

string PrimitiveFieldGenerator::GetTypeName() const
{
  return "primitive";
}

string PrimitiveFieldGenerator::GetPointerType() const
{
  switch (descriptor_->type()) {
    case FieldDescriptor::TYPE_SINT32  : 
    case FieldDescriptor::TYPE_SFIXED32: 
    case FieldDescriptor::TYPE_INT32   : return "int32_t*";
    case FieldDescriptor::TYPE_SINT64  : 
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64   : return "int64_t*";
    case FieldDescriptor::TYPE_UINT32  : 
    case FieldDescriptor::TYPE_FIXED32 : return "uint32_t*";
    case FieldDescriptor::TYPE_UINT64  : 
    case FieldDescriptor::TYPE_FIXED64 : return "uint64_t*";
    case FieldDescriptor::TYPE_FLOAT   : return "float*";
    case FieldDescriptor::TYPE_DOUBLE  : return "double*";
    case FieldDescriptor::TYPE_BOOL    : return "protobuf_c_boolean*";

    case FieldDescriptor::TYPE_ENUM    : 
    case FieldDescriptor::TYPE_STRING  :
    case FieldDescriptor::TYPE_BYTES   :
    case FieldDescriptor::TYPE_GROUP   :
    case FieldDescriptor::TYPE_MESSAGE : GOOGLE_LOG(FATAL) << "not a primitive type"; break;
  }

  return "void*";
}

string PrimitiveFieldGenerator::GetGiTypeName(bool use_const) const
{
  switch (descriptor_->type()) {
    case FieldDescriptor::TYPE_SINT32  : 
    case FieldDescriptor::TYPE_SFIXED32: 
    case FieldDescriptor::TYPE_INT32   : return "int32_t";
    case FieldDescriptor::TYPE_SINT64  : 
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64   : return "gint64";
    case FieldDescriptor::TYPE_UINT32  : 
    case FieldDescriptor::TYPE_FIXED32 : return "uint32_t";
    case FieldDescriptor::TYPE_UINT64  : 
    case FieldDescriptor::TYPE_FIXED64 : return "guint64";
    case FieldDescriptor::TYPE_FLOAT   : return "float";
    case FieldDescriptor::TYPE_DOUBLE  : return "gdouble";
    case FieldDescriptor::TYPE_BOOL    : return "gboolean";

    case FieldDescriptor::TYPE_ENUM    : 
    case FieldDescriptor::TYPE_STRING  :
    case FieldDescriptor::TYPE_BYTES   :
    case FieldDescriptor::TYPE_GROUP   :
    case FieldDescriptor::TYPE_MESSAGE : GOOGLE_LOG(FATAL) << "not a primitive type"; break;
  }

  return "void";
}

string PrimitiveFieldGenerator::GetGiReturnAnnotations() const
{
  /*
   *  The following annotatons are used for return
   *  primitive types : None
   */
  string annotations = "";

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    annotations.append("(array length=len)");
    annotations.append("(transfer none)");
    annotations.append("(nullable)");
  } 

  if (annotations.length()) {
    annotations.append(":");
  }
  return annotations;
}

string PrimitiveFieldGenerator::GetGiGetterReturnType() const
{
  string return_type = GetGiTypeName();

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return_type.append("*");
  }
  return_type.append(" ");

  return return_type;
}

string PrimitiveFieldGenerator::GetGiGetterParameterList() const
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

string PrimitiveFieldGenerator::GetGiSetterAnnotations() const
{
  std::string annotations = "";

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    annotations.append("(array length=len)");
  }

  if (annotations.length()) {
    annotations.append(":");
  }
  return annotations;
}

string PrimitiveFieldGenerator::GetGiSetterParameterList() const
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

void PrimitiveFieldGenerator::GenerateGiCGetterMethod(io::Printer* printer) const
{
  std::map<string, string> vars;

  vars["pb_field"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + FieldName(descriptor_);
  vars["boxed_field"] = std::string("boxed->") + GetGiBoxedFieldName();
  vars["pb_qfield"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + GetQuantifierField();

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
    printer->Print(vars, "*len = $pb_qfield$;\n"
                         "return(($getter_rt$)$pb_field$);\n");
  } else {
    printer->Print(vars, "return(($getter_rt$)$pb_field$);\n");
  }

  printer->Outdent();
  printer->Print("}\n\n");
  return;
}

void PrimitiveFieldGenerator::GenerateGiCSetterMethod(io::Printer* printer) const
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

  // Output the setter definitions
  printer->Print(vars, "void $setter_fn$");
  printer->Print(GetGiSetterParameterList().c_str());

  // Begining Brace of the function
  printer->Print("{\n");
  printer->Indent();

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    if (riftopts.flatinline) {
      vars["inline_max"] = SimpleItoa(riftopts.inline_max);
      printer->Print(vars, "RW_ASSERT(len <= $inline_max$); \n"
                           "$pb_qfield$  = len; \n"
                           " if ($fname$ == NULL) { \n"
                           "   return; \n"
                           "}\n"
                           "memcpy(&$pb_field$, $fname$, len*sizeof($fname$[0])); \n");
    } else {
      printer->Print(vars, "if ($pb_field$ != NULL) {\n"
                           "  g_free($pb_field$); \n"
                           "  $pb_field$ = NULL; \n"
                           "}"
                           "$pb_qfield$  = len; \n"
                           " if ($fname$ == NULL) {\n"
                           "  return; \n"
                           " }\n"
                           "$pb_field$ = g_malloc0(len*sizeof($fname$[0])); \n"
                           "memcpy($pb_field$, $fname$, len*sizeof($fname$[0])); \n");
    }
  } else {
    printer->Print(vars, "$pb_field$ = $fname$;\n");
    if (HasQuantifierField()) {
      printer->Print(vars, "$pb_qfield$  = TRUE;\n");
    }
  }
  printer->Outdent();
  printer->Print("}\n");
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
