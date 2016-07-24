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

#include <protoc-c/c_enum_field.h>
#include <protoc-c/c_enum.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <stdio.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

using internal::WireFormat;

void get_enum_riftopts(const EnumDescriptor* descriptor, rw_eopts_t *rw_eopts);

// TODO(kenton):  Factor out a "SetCommonFieldVariables()" to get rid of
//   repeat code between this and the other field types.
void SetEnumVariables(const FieldDescriptor* descriptor,
                      map<string, string>* variables,
		      struct riftfopts *riftopts) {

  (*variables)["name"] = FieldName(descriptor);
  (*variables)["type"] = FullNameToC(descriptor->enum_type()->full_name());
  rw_eopts_t e_opts;
  get_enum_riftopts(descriptor->enum_type(), &e_opts);
  if (descriptor->has_default_value()) {
    const EnumValueDescriptor* default_value = descriptor->default_value_enum();
    if (e_opts.rw_c_prefix.length()) {
      (*variables)["default"] = e_opts.rw_c_prefix + "_" + ToUpper(default_value->name());
    }
    else if (default_value->type()->containing_type() == NULL) {
      (*variables)["default"] = ToUpper(default_value->name());
    } else {
      (*variables)["default"] = FullNameToUpper(default_value->type()->full_name())
        + "__" + default_value->name();
    }
  } else
    (*variables)["default"] = "0";
  (*variables)["deprecated"] = FieldDeprecated(descriptor);
  (*variables)["inline_max"] = SimpleItoa(riftopts->inline_max);
}

// ===================================================================

EnumFieldGenerator::
EnumFieldGenerator(const FieldDescriptor* descriptor)
  : FieldGenerator(descriptor)
{
  SetEnumVariables(descriptor, &variables_, &riftopts);
}

EnumFieldGenerator::~EnumFieldGenerator() {}

void EnumFieldGenerator::AssignStructMembers(io::Printer* printer, int keynum) const
{
  char kn[10];

  if (keynum != -1) {
    sprintf(kn, "%d", keynum);
  } else {
    kn[0] = 0;
  }

  std::ostringstream buffer;
  buffer << "var_.k.$name$$deprecated$ = key" << kn << "_;";
  printer->Print(variables_, buffer.str().c_str());
}

void EnumFieldGenerator::GenerateStructMembers(io::Printer* printer) const
{
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      printer->Print(variables_, "$type$ $name$$deprecated$;\n");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (descriptor_->containing_oneof() == NULL)
        printer->Print(variables_, "protobuf_c_boolean has_$name$$deprecated$;\n");
      printer->Print(variables_, "$type$ $name$$deprecated$;\n");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      printer->Print(variables_, "size_t n_$name$$deprecated$;\n");
      if (riftopts.flatinline) {
	printer->Print(variables_, "$type$ $name$$deprecated$[$inline_max$]/*rift inline*/;\n");
      } else {
	printer->Print(variables_, "$type$ *$name$$deprecated$;\n");
      }
      break;
  }
}

string EnumFieldGenerator::GetDefaultValue(void) const
{
  return variables_.find("default")->second;
}
void EnumFieldGenerator::GenerateStaticInit(io::Printer* printer) const
{
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      printer->Print(variables_, "$default$");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      printer->Print(variables_, "0,$default$");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      // no support for default?
      if (riftopts.flatinline) {
	printer->Print("0,{ }");
      } else {
	printer->Print("0,NULL");
      }
      break;
  }
}

void EnumFieldGenerator::GenerateDescriptorInitializer(io::Printer* printer) const
{
  string addr = "&" + FullNameToLower(descriptor_->enum_type()->full_name()) + "__descriptor";
  GenerateDescriptorInitializerGeneric(printer, true, "ENUM", addr);
}

string EnumFieldGenerator::GetTypeName() const
{
  return "enum";
}

string EnumFieldGenerator::GetPointerType() const
{
  return string(FullNameToC(descriptor_->enum_type()->full_name())) + "*";
}

string EnumFieldGenerator::GetGiTypeName(bool use_const) const
{
  return "const char *";
}

string EnumFieldGenerator::GetGiReturnAnnotations() const
{
  /*
   *  The following annotatons are used for return
   *  primitive types : None
   *  lists: (transfer container)(element-type utf8)
   */
  string annotations = "";

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    annotations.append("(array length=len)");
    annotations.append("(transfer container) (element-type utf8)");
    annotations.append("(nullable)");
  }

  if (annotations.length()) {
    annotations.append(":");
  }
  return annotations;
}

string EnumFieldGenerator::GetGiGetterReturnType() const
{
  string return_type = GetGiTypeName();

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return_type.append("*");
  }
  return_type.append(" ");

  return return_type;
}

string EnumFieldGenerator::GetGiGetterParameterList() const
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

string EnumFieldGenerator::GetGiSetterAnnotations() const
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

string EnumFieldGenerator::GetGiSetterParameterList() const
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

void EnumFieldGenerator::GenerateGiCGetterMethod(io::Printer* printer) const
{
  std::map<string, string> vars;

  vars["pb_field"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + FieldName(descriptor_);
  vars["boxed_field"] = std::string("boxed->") + GetGiBoxedFieldName();
  vars["pb_qfield"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + GetQuantifierField();
  vars["f_to_str"] = FullNameToGICBase(descriptor_->enum_type()->full_name()) + "_" + "to_str";

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
    printer->Print(vars, " *len = $pb_qfield$;\n"
                         " const gchar **enum_array = g_new0(const gchar*, *len);\n"
                         " int i = 0; \n"
                         " for (i = 0; i < *len; i++) {\n"
                         "  enum_array[i] = $f_to_str$($pb_field$[i]);\n "
                         " } \n" 
                         " return (enum_array);\n"); 
  } else {
    printer->Print(vars, "return($f_to_str$($pb_field$));\n");
  }
  printer->Outdent();
  printer->Print("}\n\n");
  return;
}

void EnumFieldGenerator::GenerateGiCSetterMethod(io::Printer* printer) const
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

  vars["f_from_str"]  =  FullNameToGICBase(descriptor_->enum_type()->full_name()) + "_" + "from_str";
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    if(riftopts.flatinline) {
      vars["inline_max"] = SimpleItoa(riftopts.inline_max);
      printer->Print(vars, "RW_ASSERT(len <= $inline_max$);\n" // Return error instead of assert??
                           "int i = 0; \n"
                           "for (i = 0; i < len; i++) {\n"
                           " $pb_field$[i] = $f_from_str$($fname$[i], err);\n"
                           "}\n"
                           "$pb_qfield$ = len;\n");
    } else {
      // Not inline
      printer->Print(vars, "if ($pb_field$ != NULL) {\n"
                           "  g_free($pb_field$);\n"
                           "  $pb_field$ = NULL;\n"
                           "}\n"
                           "if ($fname$ != NULL) {\n"
                           "  $pb_field$ = g_malloc0(len * sizeof(int)); \n"
                           "  int i = 0; \n"
                           "  for (i = 0; i < len; i++) {\n"
                           "    $pb_field$[i] = $f_from_str$($fname$[i], err);\n"
                           "  }\n" 
                           "}\n"
                           "$pb_qfield$ = len;\n");
    }
  } else {
    printer->Print(vars, "$pb_field$ = $f_from_str$($fname$, err);\n");
    if (HasQuantifierField()) {
      printer->Print(vars, "$pb_qfield$ = TRUE;\n");
    }
  }

  printer->Outdent();
  printer->Print("}\n");
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
