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

#include <stdio.h>

#include <protoc-c/c_message_field.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/descriptor.pb.h>

#include "../protobuf-c/rift-protobuf-c.h"

#if 0
#define RIFTDBG1(arg1) printf(arg1)
#define RIFTDBG2(arg1,arg2) printf(arg1,arg2)
#define RIFTDBG3(arg1,arg2,arg3) printf(arg1,arg2,arg3)
#else
#define RIFTDBG1(a)
#define RIFTDBG2(a,b)
#define RIFTDBG3(a,b,c)
#endif

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

using internal::WireFormat;

// ===================================================================


MessageFieldGenerator::
    MessageFieldGenerator(const FieldDescriptor* descriptor)
    : FieldGenerator(descriptor) {
      riftopts.ismsg = true;
    }

MessageFieldGenerator::~MessageFieldGenerator() {
}

void MessageFieldGenerator::GenerateStructMembers(io::Printer* printer) const
{
  map<string, string> vars;
  vars["name"] = FieldName(descriptor_);
  if (riftopts.c_type[0]) {
    vars["type"] = riftopts.c_type[0];
  } else {
    vars["type"] = FullNameToC(descriptor_->message_type()->full_name());
  }
  vars["deprecated"] = FieldDeprecated(descriptor_);
  vars["inline_max"] = SimpleItoa(riftopts.inline_max);
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      if (riftopts.flatinline) {
        printer->Print(vars, "$type$ $name$$deprecated$; /*rift_inline, required */\n");
      } else {
        printer->Print(vars, "$type$ *$name$$deprecated$;\n");
      }
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (riftopts.flatinline) {
        if (descriptor_->containing_oneof() == NULL)
          printer->Print(vars, "protobuf_c_boolean has_$name$$deprecated$;\n");
        printer->Print(vars, "$type$ $name$$deprecated$; /*rift_inline, optional */\n");
      } else {
        printer->Print(vars, "$type$ *$name$$deprecated$;\n");
      }
      break;
    case FieldDescriptor::LABEL_REPEATED:
      if (riftopts.flatinline) {
        /* this could stand to have a pointer to array form as well as an inline array[] */
        printer->Print(vars, "size_t n_$name$$deprecated$;\n");
        printer->Print(vars, "$type$ $name$$deprecated$[$inline_max$] /*rift_inline, repeated */;\n");
      } else {
        printer->Print(vars, "size_t n_$name$$deprecated$;\n");
        printer->Print(vars, "$type$ **$name$$deprecated$;\n");
      }
      break;
  }
}
string MessageFieldGenerator::GetDefaultValue(void) const
{
  /* XXX: update when protobuf gets support
   *   for default-values of message fields.
   */
  return "NULL";
}
void MessageFieldGenerator::GenerateStaticInit(io::Printer* printer) const
{
  map<string, string> vars;
  vars["ucclassname"] = FullNameToUpper(descriptor_->message_type()->full_name());
  vars["pclassname"] = FullNameToC(descriptor_->containing_type()->full_name());
  vars["fieldname"] = FieldName(descriptor_) + FieldDeprecated(descriptor_);
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      if (riftopts.flatinline) {
        printer->Print(vars, "__$ucclassname$__INTERNAL_INITIALIZER__(PROTOBUF_C_FLAG_REF_STATE_INNER, offsetof($pclassname$, $fieldname$))");
      } else {
        printer->Print("NULL");
      }
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (riftopts.flatinline) {
        printer->Print(vars, "0,__$ucclassname$__INTERNAL_INITIALIZER__(PROTOBUF_C_FLAG_REF_STATE_INNER, offsetof($pclassname$, $fieldname$))");
      } else {
        printer->Print("NULL");
      }
      break;
    case FieldDescriptor::LABEL_REPEATED:
      if (riftopts.flatinline) {
        printer->Print("0");
        if (!riftopts.inline_max) {
          fprintf(stderr, "*** protoc-c: error %s flat/inline with inline_max=0\n", descriptor_->full_name().c_str());
          exit(0);
        }
        printer->Print(", {");
        for (int i=0; i<riftopts.inline_max; i++) {
          vars["index"] = SimpleItoa(i);
          printer->Print(vars, "__$ucclassname$__INTERNAL_INITIALIZER__(PROTOBUF_C_FLAG_REF_STATE_INNER, offsetof($pclassname$, $fieldname$[$index$])),");
        }
        printer->Print(" }");
      } else {
        printer->Print("0,NULL");
      }
      break;
  }
}
void MessageFieldGenerator::GenerateDescriptorInitializer(io::Printer* printer) const
{
  string addr = "&" + FullNameToLower(descriptor_->message_type()->full_name()) + "__descriptor";

  int opt_uses_has = false;
  if (riftopts.flatinline) {
    opt_uses_has = true;
  }

  GenerateDescriptorInitializerGeneric(printer, opt_uses_has, "MESSAGE", addr);
}

string MessageFieldGenerator::GetTypeName() const
{
  if (riftopts.c_type[0]) {
    return "ctype";
  }
  return "message";
}

string MessageFieldGenerator::GetPointerType() const
{
  if (isCType()) {
    if (riftopts.flatinline) {
      return string(riftopts.c_type) + "*";
    }
    return string(riftopts.c_type) + "**";
  }

  string type = FullNameToC(descriptor_->message_type()->full_name());
  if (riftopts.flatinline) {
    return type + "*";
  }

  return type + "**";
}

string MessageFieldGenerator::GetGiTypeName(bool use_const) const
{
  return FullNameToGI(descriptor_->message_type()) + "*";
}

string MessageFieldGenerator::GetGiReturnAnnotations() const
{
  /*
   *  The following annotatons are used for return
   *  primitive types : None
   *  lists : (array length=len)(transfer full)
   *  not in a list : (transfer full))
   */
  string annotations = "";

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    annotations.append("(array length=len)");
    annotations.append("(transfer full)");
    annotations.append("(nullable)");
  } else {
    annotations.append("(transfer full)");
  }

  if (annotations.length()) {
    annotations.append(":");
  }
  return annotations;
}

string MessageFieldGenerator::GetGiGetterReturnType() const
{
  string return_type = GetGiTypeName();

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return_type.append("*");
  }
  return_type.append(" ");

  return return_type;
}

string MessageFieldGenerator::GetGiGetterParameterList() const
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

string MessageFieldGenerator::GetGiSetterAnnotations() const
{
  std::string annotations = "(transfer none)";

  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REPEATED:
      annotations.append("(array length=len)");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (!riftopts.flatinline) {
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

string MessageFieldGenerator::GetGiSetterParameterList() const
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

void MessageFieldGenerator::GenerateGiCGetterMethod(io::Printer* printer) const
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

  vars["m_gi_tname"] = FullNameToGI(descriptor_->message_type());
  vars["m_new_adopt_fn"] = GetGiCIdentifier("new_adopt");
  vars["classname"] = FullNameToC(descriptor_->message_type()->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->message_type()->full_name());

  printer->Print(vars, "$getter_rt$ $getter_fn$");
  printer->Print(GetGiGetterParameterList().c_str());
  printer->Print("\n");

  // Begining Brace of the function
  printer->Print(vars, "{\n"
                       "  PROTOBUF_C_GI_MUTEX_GUARD_AUTO_RELEASE();\n"
                       "  /* Return 0 value for any return type */\n"
                       "  PROTOBUF_C_GI_CHECK_FOR_ERROR_RETVAL(boxed, err, $domain$, ($getter_rt$)0);\n\n");
  printer->Indent();

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    printer->Print(vars, "*len = $pb_qfield$;\n"
                         "size_t i; \n\n"
                         "if ($boxed_field$ == NULL) {\n"
                         "  $boxed_field$ = protobuf_c_gi_boxed_zalloc(sizeof($m_gi_tname$*)*(*len+1));\n"
                         "  for (i = 0; i < *len; i++) {\n");
    if (riftopts.flatinline) {
      printer->Print(vars, "    $boxed_field$[i] = $m_new_adopt_fn$(\n"
                           "        &$pb_field$[i], &boxed->box);\n" );
    } else {
      printer->Print(vars, "    $boxed_field$[i] = $m_new_adopt_fn$(\n"
                           "        $pb_field$[i], &boxed->box);\n" );
    }

    printer->Print(vars, "    protobuf_c_message_gi_ref(&boxed->box);\n"
                         "    protobuf_c_message_gi_ref(&$boxed_field$[i]->box);\n"
                         "  }\n"
                         "} else {\n"
                         "  for (i = 0; i < *len; i++) {\n"
                         "    ProtobufCGiMessageBase *gi_base = &$boxed_field$[i]->s.gi_base;\n"
                         "    RW_ASSERT(PROTOBUF_C_GI_BASE_IS_GOOD(gi_base));\n"
                         "    if (PROTOBUF_C_GI_IS_ZOMBIE(gi_base)) {\n"
                         "      protobuf_c_message_gi_revive_zombie(&$boxed_field$[i]->box);\n"
                         "    }\n"
                         "    protobuf_c_message_gi_ref(&$boxed_field$[i]->box);\n"
                         "  }\n"
                         "}\n"
                         "$m_gi_tname$** dup = g_new0($m_gi_tname$*, *len+1);\n"
                         "memcpy(dup, $boxed_field$, sizeof($m_gi_tname$*)*(*len+1));\n"
                         "return dup;\n");
  } else {
    printer->Print(vars, "if ($boxed_field$ != NULL) {\n"
                         "  ProtobufCGiMessageBase *gi_base = &$boxed_field$->s.gi_base;\n"
                         "  RW_ASSERT(PROTOBUF_C_GI_BASE_IS_GOOD(gi_base));\n"
                         "  if (PROTOBUF_C_GI_IS_ZOMBIE(gi_base)) {\n"
                         "    protobuf_c_message_gi_revive_zombie(&$boxed_field$->box);\n"
                         "  }\n"
                         "  protobuf_c_message_gi_ref(&$boxed_field$->box);\n"
                         "  return $boxed_field$;\n"
                         "}\n");

    if (riftopts.flatinline) {
      if (HasQuantifierField()) {
        printer->Print(vars, "if (!$pb_qfield$) {\n"
                             "  /* ATTN init protobuf */\n"
                             "  $pb_qfield$ = TRUE;\n"
                             "}\n");
      }
      printer->Print(vars, "$boxed_field$ = $m_new_adopt_fn$(\n"
                           "    &$pb_field$,\n"
                           "    &boxed->box);\n");
    } else {
      printer->Print(vars, "if ($pb_field$ == NULL) {\n"
                           "  $pb_field$ = ($classname$*)\n"
                           "    protobuf_c_message_create(NULL, &$lcclassname$__descriptor);\n"
                           "}\n"
                           "$boxed_field$ = $m_new_adopt_fn$(\n"
                           "    $pb_field$,\n"
                           "    &boxed->box);\n");
    }
    printer->Print(vars, "protobuf_c_message_gi_ref(&boxed->box);\n"
                         "protobuf_c_message_gi_ref(&$boxed_field$->box);\n"
                         "return $boxed_field$;\n");
  }

  printer->Outdent();
  printer->Print("}\n\n");
  return;
}

void MessageFieldGenerator::GenerateGiCSetterMethod(io::Printer* printer) const
{
  std::map<string, string> vars;

  vars["pb_field"] = GetParentGiCIdentifier("helper_macro") + "(boxed)->" + FieldName(descriptor_);
  vars["boxed_field"] = std::string("boxed->") + GetGiBoxedFieldName();
  vars["pb_qfield"] =  GetParentGiCIdentifier("helper_macro") + "(boxed)->" + GetQuantifierField();
  vars["new_boxed_l"] = string("new_") + FieldName(descriptor_);
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
  printer->Print("{\n"
                 "  PROTOBUF_C_GI_MUTEX_GUARD_AUTO_RELEASE();\n");

  printer->Indent();
  vars["pb_field_in"] = GetGiCIdentifier("helper_macro") + "(" + FieldName(descriptor_) + ")";
  vars["helper_macro"] = GetGiCIdentifier("helper_macro");
  vars["gi_tname"] = FullNameToGI(descriptor_->message_type());
  vars["invalidate_fn"] = GetGiCIdentifier("invalidate");
  vars["classname"] = FullNameToC(descriptor_->message_type()->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->message_type()->full_name());

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    printer->Print(vars, "size_t i = 0;\n"
                         "size_t j = 0;\n"
                         "g_return_if_fail($fname$ != NULL || len == 0);\n");

    if (riftopts.flatinline) {
      printer->Print(vars, "g_return_if_fail(len <= G_N_ELEMENTS($pb_field$));\n");
    }

    printer->Print("\n");

    printer->Print(vars, "for (i = 0; i < len; i++) {\n"
                         "  g_return_if_fail($fname$[i] != NULL);\n"
                         "  PROTOBUF_C_GI_CHECK_FOR_PARENT_ERROR(boxed, $fname$[i], err, $domain$);\n"
                         "}\n"
                         "PROTOBUF_C_GI_CHECK_FOR_ERROR(boxed, err, $domain$);\n"
                         "\n"
                         "$gi_tname$** $new_boxed_l$ = protobuf_c_gi_boxed_zalloc(sizeof($gi_tname$*)*(len+1));\n"
                         "int n_listy_children = 0;\n"
                         "if ($boxed_field$ != NULL) {\n"
                            /* Match incoming list to existing list, to determine what to free */
                         "  if ($pb_field$  != NULL) {\n"
                         "    for (i = 0; $boxed_field$[i] != NULL; i++) {\n"
                         "      n_listy_children++;\n"
                         "      $gi_tname$ *_old_child_ = $boxed_field$[i];\n"
                         "      gboolean found = FALSE;\n"
                         "      for (j = 0; j < len; j++) {\n"
                         "        $gi_tname$ *_new_child_ = $fname$[j];\n"
                         "        if (_old_child_ == _new_child_) {\n"
                         "          found = TRUE;\n"
                         "          RW_ASSERT($new_boxed_l$[j] == NULL);\n"
                         "          $new_boxed_l$[j] = _old_child_;\n"
                         "          break;\n"
                         "        }\n"
                         "      }\n"
                         "\n"
                                /* Setting it to NULL so that we dont call invalidate below. */
                         "      if (found) {\n"
                         "        $boxed_field$[i] = NULL;\n"
                         "      }\n"
                         "    }\n"
                         "  }\n"
                         "\n"
                            /* Invalidate the references that get overwritten. */
                            /* Invalidate calls unref, both on this box, and itself. */
                         "  $gi_tname$** tmp = $boxed_field$;\n"
                         "  $boxed_field$ = NULL;\n"
                         "  for (i = 0; i < n_listy_children; i++) {\n"
                         "    if (tmp[i] != NULL) {\n"
                         "      ProtobufCMessage* freemsg = tmp[i]->box.message;\n"
                         "      $invalidate_fn$(tmp[i]);\n");

    if (riftopts.flatinline) {
      printer->Print(vars, "      protobuf_c_message_free_unpacked_usebody(NULL, freemsg);\n"
                           "      protobuf_c_message_init_inner(\n"
                           "          boxed->box.message,\n"
                           "          freemsg,\n"
                           "          boxed->s.message->base_concrete.descriptor->fields.$fname$);\n");
    } else {
      printer->Print(vars, "      protobuf_c_message_free_unpacked(NULL, freemsg);\n");
    }

    printer->Print(vars, "    }\n"
                         "  }\n"
                         "  protobuf_c_gi_boxed_free(tmp);\n"
                         "}\n"
                         "\n"
                         "$boxed_field$ = $new_boxed_l$;\n"
                         "$pb_qfield$ = len;\n");

    if (!riftopts.flatinline) {
      printer->Print(vars, "$classname$** new_proto_list\n"
                           "  = ($classname$**)protobuf_c_instance_zalloc(NULL,\n"
                           "      sizeof($classname$*)*(len + 1));\n"
                           "if ($pb_field$ != NULL) {\n"
                           "  protobuf_c_instance_free(NULL, $pb_field$);\n"
                           "}\n"
                           "$pb_field$ = new_proto_list;\n");
    } else {
      printer->Print(vars, "$classname$* new_proto_list\n"
                           "  = ($classname$*)protobuf_c_gi_boxed_zalloc(\n"
                           "      sizeof($classname$)*(G_N_ELEMENTS($pb_field$)));\n");
    }

    printer->Print(vars, "\n"
                         "for (i = 0; i < len; i++) {\n"
                         "  $gi_tname$ *tmp = $fname$[i];\n"
                         "  ProtobufCGiMessageBase *gi_base = &$fname$[i]->s.gi_base;\n"
                         "  RW_ASSERT(PROTOBUF_C_GI_BASE_IS_GOOD(gi_base));\n" );

    if (riftopts.flatinline) {
      printer->Print(vars, "  memcpy(&new_proto_list[i],\n"
                           "         $helper_macro$(tmp),\n"
                           "         sizeof(new_proto_list[0]));\n"
                           "  if ($boxed_field$[i] == NULL) {\n"
                           "    protobuf_c_instance_free(NULL, $helper_macro$(tmp));\n"
                           "    $boxed_field$[i] = tmp;\n"
                           "  }\n"
                           "  tmp->s.message = &($pb_field$[i]);\n");
    } else {
      printer->Print(vars, "  $pb_field$[i] = tmp->s.message;\n"
                           "  if ($boxed_field$[i] == NULL) {\n"
                           "    $boxed_field$[i] = tmp;\n"
                           "  }\n");
    }

    printer->Print("  protobuf_c_message_gi_reparent(&boxed->box, &tmp->box);\n"
                   "}\n");

    if (riftopts.flatinline) {
      printer->Print(vars, "\nfor (i = 0; i < len; i++) {\n"
                           "  protobuf_c_message_memcpy(&($pb_field$[i].base), &(new_proto_list[i].base));\n"
                           "}\n"
                           "for (; i < G_N_ELEMENTS($pb_field$); i++) {\n"
                           "  if (PROTOBUF_C_IS_MESSAGE(&($pb_field$[i].base))) { \n"
                           "    protobuf_c_message_init_inner(\n"
                           "      boxed->box.message,\n"
                           "      &($pb_field$[i].base),\n"
                           "      boxed->s.message->base_concrete.descriptor->fields.$fname$);\n"
                           "  }\n"
                           "}\n"
                           "protobuf_c_gi_boxed_free(new_proto_list);\n");
    }

  } else { // not listy
    printer->Print(vars,  /* Detect pure self-assignment. */
                         "if ($fname$ == $boxed_field$) {\n"
                         "  return;\n"
                         "}\n"
                         "\n"
                         "if ($fname$) {\n"
                         "  ProtobufCGiMessageBase* gi_base = &$fname$->s.gi_base;\n"
                         "  RW_ASSERT(PROTOBUF_C_GI_BASE_IS_GOOD(gi_base));\n"
                         "  PROTOBUF_C_GI_CHECK_FOR_PARENT_ERROR(boxed, $fname$, err, $domain$);\n"
                         "  protobuf_c_message_gi_reparent(&boxed->box, &$fname$->box);\n"
                         "}\n"
                         "\n"
                          /* Detect aliased self-assignment and invalidate old reference. */
                         "if (NULL != $boxed_field$) {\n"
                         "  ProtobufCMessage* old_msg = $boxed_field$->box.message;\n"
                         "  $invalidate_fn$($boxed_field$);\n"
                         "  $boxed_field$ = NULL;\n"
                         "  if ($fname$ && old_msg == $fname$->box.message) {\n"
                         "    $boxed_field$ = $fname$;\n"
                         "    return;\n"
                         "  }\n"
                         "}\n"
                         "\n");

    /* Destroy old message */
    if (HasQuantifierField()) {
      printer->Print(vars, "if ($pb_qfield$) {\n");
      printer->Indent();
    } else if (!riftopts.flatinline) {
      printer->Print(vars, "if ($pb_field$ != NULL) {\n");
      printer->Indent();
    }

    if (riftopts.flatinline) {
      printer->Print(vars, "protobuf_c_message_free_unpacked_usebody(NULL, &$pb_field$.base);\n"
                           "protobuf_c_message_init_inner(\n"
                           "    boxed->box.message,\n"
                           "    &$pb_field$.base,\n"
                           "    boxed->s.message->base_concrete.descriptor->fields.$fname$);\n");
      if (HasQuantifierField()) {
        printer->Print(vars, "$pb_qfield$ = FALSE;\n");
      }
    } else {
      printer->Print(vars, "protobuf_c_message_free_unpacked(NULL, &$pb_field$->base);\n");
      printer->Print(vars, "$pb_field$ = NULL;\n");
    }
    /* Destroy old message */
    if (HasQuantifierField()) {
      printer->Outdent();
      printer->Print("}\n");
    } else if (!riftopts.flatinline) {
      printer->Outdent();
      printer->Print("}\n");
    }

    /* Take ownership of new message message */
    printer->Print(vars, "if (NULL != $fname$) {\n"
                         "  $boxed_field$ = $fname$;\n");
    if (riftopts.flatinline) {
      printer->Print(vars, "  protobuf_c_message_memcpy(&$pb_field$.base, &$pb_field_in$->base);\n"
                           "  protobuf_c_instance_free(NULL, $pb_field_in$);\n");
      if (HasQuantifierField()) {
        printer->Print(vars, "  $pb_qfield$ = TRUE;\n");
      }
      printer->Print(vars, "  $fname$->s.message = &($pb_field$);\n");
    } else {
      printer->Print(vars, "  $pb_field$ = $pb_field_in$;\n");
    }
    printer->Print("}\n");
  }

  printer->Outdent();
  printer->Print("}\n");
}

string MessageFieldGenerator::GetGiCreateParameterList() const
{
  string gi_type_ptr = std::string("(") + FullNameToGI(FieldScope(descriptor_)) + "*";
  string param_list = gi_type_ptr + " boxed";
  param_list.append(", GError **err");
  param_list.append(")");

  return param_list;
}

void MessageFieldGenerator::GenerateGiCreateMethod(io::Printer* printer) const
{
  std::map<string, string> vars;
  vars["gi_tname"] = GetGiTypeName();
  vars["create_fn"] = GetParentGiCIdentifier("create", FieldName(descriptor_));
  vars["new_fn"] = GetGiCIdentifier("new");

  // Output the getter definitions
  printer->Print(vars, "$gi_tname$ $create_fn$");
  printer->Print(GetGiCreateParameterList().c_str());
  printer->Print("\n");

  // Begining Brace of the function
  printer->Print("{\n");
  printer->Print(vars, "  return $new_fn$();\n"
                       "}\n");
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
