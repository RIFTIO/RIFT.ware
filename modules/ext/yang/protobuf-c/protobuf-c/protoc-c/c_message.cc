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

#include <algorithm>
#include <map>
#include <protoc-c/c_message.h>
#include <protoc-c/c_file.h>
#include <protoc-c/c_message_field.h>
#include <protoc-c/c_enum.h>
#include <protoc-c/c_extension.h>
#include <protoc-c/c_helpers.h>
#include <protoc-c/c_bytes_field.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/descriptor.pb.h>

#include "../protobuf-c/rift-protobuf-c.h"

#if 0
#define RIFTDBG1(arg1) fprintf(stderr, arg1)
#define RIFTDBG2(arg1,arg2) fprintf(stderr, arg1,arg2)
#define RIFTDBG3(arg1,arg2,arg3) fprintf(stderr, arg1,arg2,arg3)
#else
#define RIFTDBG1(a)
#define RIFTDBG2(a,b)
#define RIFTDBG3(a,b,c)
#endif

namespace google {
namespace protobuf {
namespace compiler {
namespace c {


// ===================================================================

void mdesc_to_riftmopts(const Descriptor* descriptor, RiftMopts* riftmopts)
{
  MessageOptions mopts(descriptor->options());
  UnknownFieldSet *unks = mopts.mutable_unknown_fields();

  unsigned riftflags = 0;

  if (!unks->empty()) {
    RIFTDBG3("+++ message descriptor '%s' has %d extension options\n",
             descriptor->full_name().c_str(), unks->field_count());

    for (int i=0; i<unks->field_count(); i++) {
      UnknownField *unkf = unks->mutable_field(i);
      RIFTDBG3("+++ ext opt field number %d type %d\n", (int)unkf->number(), (int)unkf->type());
      switch (unkf->number()) {
      case 50001:
        if (UnknownField::TYPE_LENGTH_DELIMITED == unkf->type()) {
          RIFTDBG1("+++ message descriptor with rift_msgoptions of type_length_delimited!!\n");
          RIFTDBG2("  buf len=%d: ", unkf->length_delimited().length());
          for (int k=0; k<unkf->length_delimited().length(); k++) {
            RIFTDBG2("%0X ", (uint32_t) *(const char*)unkf->length_delimited().substr(k, 1).c_str());
          }
          RIFTDBG1("\n");
          UnknownFieldSet ropts;
          if (ropts.ParseFromString(unkf->length_delimited())) {
            RIFTDBG2("+++ ldelim parsed, %d fields\n", ropts.field_count());
            for (int j=0; j<ropts.field_count(); j++) {
              UnknownField *ropt = ropts.mutable_field(j);
              switch (ropt->number()) {
                case 1:
                  if (ropt->type() == UnknownField::TYPE_VARINT) {
                    RIFTDBG2("++ flat = %s\n", ropt->varint() ? "true" : "false");
                    if (ropt->varint()) {
                      riftflags |= RW_PROTOBUF_MOPT_FLAT;
                    } else {
                      riftflags &= ~RW_PROTOBUF_MOPT_FLAT;
                    }
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 2:
                  if (ropt->type() == UnknownField::TYPE_VARINT) {
                    RIFTDBG2("+++ comp = %s\n", ropt->varint() ? "true" : "false");
                    if (ropt->varint()) {
                      riftflags |= RW_PROTOBUF_MOPT_COMP;
                    } else {
                      riftflags &= ~RW_PROTOBUF_MOPT_COMP;
                    }
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 3:
                  if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    riftmopts->ypbc_msg = ropt->length_delimited();
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 4:
                  if (ropt->type() == UnknownField::TYPE_VARINT) {
                    RIFTDBG2("++ suppress = %s\n", ropt->varint() ? "true" : "false");
                    if (ropt->varint()) {
                      riftmopts->suppress = 1;
                    } else {
                      riftmopts->suppress = 0;
                    }
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 5:
                  if (ropt->type() == UnknownField::TYPE_VARINT) {
                    RIFTDBG2("++ has_keys = %s\n", ropt->varint() ? "true" : "false");
                    if (ropt->varint()) {
                      riftflags |= RW_PROTOBUF_MOPT_HAS_KEYS;
                    } else {
                      riftflags &= ~RW_PROTOBUF_MOPT_HAS_KEYS;
                    }
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 6:
                  if (ropt->type() == UnknownField::TYPE_VARINT) {
                    RIFTDBG2("++ log_event_type = %s\n", ropt->varint() ? "true" : "false");
                    if (ropt->varint()) {
                      riftflags |= RW_PROTOBUF_MOPT_LOG_EVENT;
                    } else {
                      riftflags &= ~RW_PROTOBUF_MOPT_LOG_EVENT;
                    }
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 7:
                  if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    riftmopts->c_typedef = ropt->length_delimited();
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 8:
                  if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    riftmopts->c_typename = ropt->length_delimited();
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 9:
                  if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    riftmopts->base_typename = ropt->length_delimited();
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 10:
                  if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    riftmopts->msg_new_path = ropt->length_delimited();
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                case 11:
                  if (ropt->type() == UnknownField::TYPE_VARINT) {
                    RIFTDBG2("++ hide_from_gi = %s\n", ropt->varint() ? "true" : "false");
                    if (ropt->varint()) {
                      riftmopts->hide_from_gi = 1;
                    } else {
                      riftmopts->hide_from_gi = 0;
                    }
                  } else {
                    RIFTDBG2("+++ unknown data type for RiftMessageOptions option %d\n", ropt->number());
                  }
                  break;
                default:
                  fprintf(stderr, "*** Message '%s' has unknown RiftMessageOptions option field %d\n",
                          descriptor->full_name().c_str(), ropt->number());
                  break;
              }
            }
          }
        } else {
          fprintf(stderr, "*** Message '%s' has unknown type %d for field %d\n",
                  descriptor->full_name().c_str(),
                  (int)unkf->type(), (int)unkf->number());
        }
        break;

      default:
        fprintf(stderr, "*** Message '%s' has unknown message option field %d\n",
                descriptor->full_name().c_str(),
                unkf->number());
        break;
      }
    }
  }

  const char *re = getenv("RIFT_ENFORCEOPTS");
  if (re && 0==strcmp(re, "1")) {
    riftmopts->enforce = 1;
  }
  riftmopts->rw_flags = riftflags;
  riftmopts->flatinline = !!(riftflags & RW_PROTOBUF_MOPT_FLAT);
  if (riftmopts->ypbc_msg.length()) {
    riftmopts->need_ypbc_include = true;
  }
}

MessageGenerator::MessageGenerator(const Descriptor* descriptor,
                                   const string& dllexport_decl)
  : descriptor_(descriptor),
    dllexport_decl_(dllexport_decl),
    field_generators_(descriptor),
    nested_generators_(new scoped_ptr<MessageGenerator>[
      descriptor->nested_type_count()]),
    enum_generators_(new scoped_ptr<EnumGenerator>[
      descriptor->enum_type_count()]),
    extension_generators_(new scoped_ptr<ExtensionGenerator>[
      descriptor->extension_count()])
{
  mdesc_to_riftmopts(descriptor_, &riftmopts_);
  gi_typename_ = FullNameToGI(descriptor_, &riftmopts_);

  gen_minikey_type_ = false; // ATTN: Should be in riftmopts?

  for (int i = 0; i < descriptor->nested_type_count(); i++) {
    nested_generators_[i].reset(
      new MessageGenerator(descriptor->nested_type(i), dllexport_decl));
  }

  /* Message Suppress option should not be specified for the messages that
   * contains fields. It should be specified only for empty messages.  */
  if ((descriptor->field_count()) && (riftmopts_.suppress)) {
    fprintf(stderr, "*** Suppress Message flag is specified for '%s' which is not empty. It contains %d fields\n",
      descriptor_->full_name().c_str(), descriptor->field_count());
    exit(1);
  }

  /* Plug rift opts into particular field generators as needed.  Notably, flat/inline... */
  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    FieldGenerator *fg = (FieldGenerator*)(const FieldGenerator *)&(field_generators_.get(field));
    if (riftmopts_.flatinline || fg->riftopts.flatinline) {
      if (fg->riftopts.ismsg && riftmopts_.flatinline) {

	/* Message fields to be done inline in flat messages must be
	   declared flat already.  There is no provision for having
	   like-named flat and nonflat descriptors.  OTOH, message
	   fields to be inline in nonflat messages may themselves be
	   nonflat. This latter case is slightly risque in that later
	   conversion from inline to flat may be difficult if there is
	   much nonflatness in the inlined but nonflat children.  */

	RiftMopts submsgopt;
	mdesc_to_riftmopts(field->message_type(), &submsgopt);
	if (!submsgopt.flatinline) {
	  fprintf(stderr, "*** Flat message '%s' attempts to have a nonflat message '%s' as inline field '%s'\n",
		  descriptor_->full_name().c_str(), field->message_type()->full_name().c_str(), field->full_name().c_str());
	  /* the field's message descriptor will be wrong if it's to
	     be flat here and was not flat when described */
	  exit(1);
	}
      }
      if (!fg->riftopts.inline_max
	  && (field->label() == FieldDescriptor::LABEL_REPEATED)) {
	fprintf(stderr, "*** Message '%s' inline repeated field '%s' must have rift option inline_max with a value > 0.\n",
		descriptor_->full_name().c_str(), field->full_name().c_str());
	exit(1);
      }
      if (   fg->riftopts.string_max < 2
	  && (   field->type() == FieldDescriptor::TYPE_STRING
	      || field->type() == FieldDescriptor::TYPE_BYTES)
	  && !fg->isCType()) {
	fprintf(stderr, "*** Message '%s' inline string field '%s' must have rift option string_max with a value >= 2.\n",
		descriptor_->full_name().c_str(), field->full_name().c_str());
	exit(1);
      }

      /* In any case, this field must now be inline */
      fg->riftopts.flatinline = true;
      fg->riftopts.rw_flags |= RW_PROTOBUF_FOPT_INLINE;
    }
    if (fg->isCType()) {
      fg->riftopts.rw_flags |= RW_PROTOBUF_FOPT_RW_CTYPE;
    }
  }

  for (int i = 0; i < descriptor->enum_type_count(); i++) {
    enum_generators_[i].reset(
      new EnumGenerator(descriptor->enum_type(i), dllexport_decl));
  }

  for (int i = 0; i < descriptor->extension_count(); i++) {
    extension_generators_[i].reset(
      new ExtensionGenerator(descriptor->extension(i), dllexport_decl));
  }
}

MessageGenerator::~MessageGenerator() {}

void MessageGenerator::GenerateStructTypedef(io::Printer* printer, const bool hide_from_gi)
{

  if (!(riftmopts_.suppress)) {

    if (hide_from_gi) {
      printer->Print("#ifndef __GI_SCANNER__\n");
    }

    if (riftmopts_.c_typename.length()) {
      printer->Print("typedef struct $typename$ $typename$;\n",
                     "typename", riftmopts_.c_typedef);
      printer->Print("typedef struct $typename$ $classname$;\n",
                     "typename", riftmopts_.c_typename,
                     "classname", FullNameToC(descriptor_->full_name()));

    } else if (riftmopts_.c_typedef.length()) {
      printer->Print("typedef struct $typedef$ $classname$;\n",
                     "typedef", riftmopts_.c_typedef,
                     "classname", FullNameToC(descriptor_->full_name()));
    } else {
      printer->Print("typedef struct $classname$ $classname$;\n",
                     "classname", FullNameToC(descriptor_->full_name()));
    }

    if (hide_from_gi) {
      printer->Print("#endif // __GI_SCANNER__\n");
    }

  }

  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateStructTypedef(printer, hide_child_from_gi);
  }
}



void MessageGenerator::
GeneratePBCMDStructTypedef(io::Printer* printer, const bool hide_from_gi)
{
  if (!(riftmopts_.suppress)) {

    if (hide_from_gi) {
      printer->Print("#ifndef __GI_SCANNER__\n");
    }

    printer->Print("typedef struct RwProtobufCMessageDescriptor_$classname$ RwProtobufCMessageDescriptor_$classname$;\n",
                   "classname", FullNameToC(descriptor_->full_name()));

    printer->Print("typedef struct RwProtobufCMessageBase_$classname$ RwProtobufCMessageBase_$classname$;\n",
                   "classname", FullNameToC(descriptor_->full_name()));

    if (hide_from_gi) {
      printer->Print("#endif // __GI_SCANNER__\n");
    }

  }
  
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GeneratePBCMDStructTypedef(printer, hide_child_from_gi);
  }
}

void MessageGenerator::
GenerateEnumDefinitions(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateEnumDefinitions(printer, hide_child_from_gi);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDefinition(printer);
    if (enum_generators_[i]->has_rw_yang_enum()) {
      riftmopts_.need_ypbc_include = true;
    }
  }
}

void MessageGenerator::
GenerateStructDefinition(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateStructDefinition(printer, hide_child_from_gi);
    if (nested_generators_[i]->riftmopts_.need_ypbc_include) {
      riftmopts_.need_ypbc_include = true;
    }
  }

  if (riftmopts_.suppress) {
    return; // suppress this message.
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  if (riftmopts_.c_typename.length()) {
    vars["classname"] = riftmopts_.c_typename;
  } else {
    vars["classname"] = FullNameToC(descriptor_->full_name());
  }
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
  vars["ucclassname"] = FullNameToUpper(descriptor_->full_name());
  vars["field_count"] = SimpleItoa(descriptor_->field_count());
  if (dllexport_decl_.empty()) {
    vars["dllexport"] = "";
  } else {
    vars["dllexport"] = dllexport_decl_ + " ";
  }

  /* Generate Protobuf Concrete Message Descriptor structure definition
   * for this message. */
  // ATTN: should be suppressed for typedef?
  GeneratePBCMDStructDefinition(printer);
  printer->Print("\n");

  // ATTN: should generate aliases for c_typedef
  // Generate the case enums for unions
  for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
    const OneofDescriptor *oneof = descriptor_->oneof_decl(i);
    vars["oneofname"] = FullNameToUpper(oneof->name());
    vars["foneofname"] = FullNameToC(oneof->full_name());

    printer->Print("typedef enum {\n");
    printer->Indent();
    printer->Print(vars, "$ucclassname$__$oneofname$__NOT_SET = 0,\n");
    for (int j = 0; j < oneof->field_count(); j++) {
      const FieldDescriptor *field = oneof->field(j);
      vars["fieldname"] = FullNameToUpper(field->name());
      vars["fieldnum"] = SimpleItoa(field->number());
      printer->Print(vars, "$ucclassname$__$oneofname$_$fieldname$ = $fieldnum$,\n");
    }
    printer->Outdent();
    printer->Print(vars, "} $foneofname$Case;\n\n");
  }

  if (!riftmopts_.c_typedef.length()) {

    SourceLocation msgSourceLoc;
    descriptor_->GetSourceLocation(&msgSourceLoc);

    PrintComment (printer, msgSourceLoc.leading_comments);
    printer->Print(vars,
                   "struct $dllexport$ $classname$\n"
                   "{\n");
    printer->Indent();
    printer->Print("union {\n");

    printer->Indent();
    printer->Print(vars, "ProtobufCMessage base;\n"
                   "RwProtobufCMessageBase_$classname$ base_concrete;\n");

    if (riftmopts_.base_typename.length()) {
      vars["base_typename"] = riftmopts_.base_typename;
      printer->Print(vars, "$base_typename$ $base_typename$;\n");
    }
    printer->Outdent();
    printer->Print("};\n");
    printer->Outdent();

    // Generate fields.
    printer->Indent();
    for (int i = 0; i < descriptor_->field_count(); i++) {
      const FieldDescriptor *field = descriptor_->field(i);
      if (field->containing_oneof() == NULL) {
        SourceLocation fieldSourceLoc;
        field->GetSourceLocation(&fieldSourceLoc);

        PrintComment (printer, fieldSourceLoc.leading_comments);
        PrintComment (printer, fieldSourceLoc.trailing_comments);
        field_generators_.get(field).GenerateStructMembers(printer);
      }
    }

    // Generate unions from oneofs.
    for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
      const OneofDescriptor *oneof = descriptor_->oneof_decl(i);
      vars["oneofname"] = FullNameToLower(oneof->name());
      vars["foneofname"] = FullNameToC(oneof->full_name());

      printer->Print(vars, "$foneofname$Case $oneofname$_case;\n");

      printer->Print("union {\n");
      printer->Indent();
      for (int j = 0; j < oneof->field_count(); j++) {
        const FieldDescriptor *field = oneof->field(j);
        SourceLocation fieldSourceLoc;
        field->GetSourceLocation(&fieldSourceLoc);

        PrintComment (printer, fieldSourceLoc.leading_comments);
        PrintComment (printer, fieldSourceLoc.trailing_comments);
        field_generators_.get(field).GenerateStructMembers(printer);
      }
      printer->Outdent();
      printer->Print(vars, "};\n");
    }
    printer->Outdent();

    printer->Print(vars, "};\n");
  }

  // ATTN: Should generate aliases when c_typedef
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    if (field->has_default_value()) {
      field_generators_.get(field).GenerateDefaultValueDeclarations(printer);
    }
  }

  printer->Print(vars, "#define __$ucclassname$__INTERNAL_INITIALIZER__(ref_state, offset) \\\n"
		       " { { PROTOBUF_C_MESSAGE_INIT (&$lcclassname$__descriptor, ref_state, offset) }\\\n    ");
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    if (field->containing_oneof() == NULL) {
      printer->Print(", ");
      field_generators_.get(field).GenerateStaticInit(printer);
    }
  }
  for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
    const OneofDescriptor *oneof = descriptor_->oneof_decl(i);
    vars["foneofname"] = FullNameToUpper(oneof->full_name());
    // Initialize the case enum
    printer->Print(vars, ", $foneofname$__NOT_SET");
    // Initialize the enum
    printer->Print(", {}");
  }
  printer->Print(" }\n\n\n");

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::
GenerateMiniKeyStructDefinition(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateMiniKeyStructDefinition(printer, hide_child_from_gi);
  }

  if (!(riftmopts_.rw_flags & RW_PROTOBUF_MOPT_HAS_KEYS)) {
    return;
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  string mk_sname = descriptor_->full_name()+".mini_key";
  vars["classname"] = FullNameToC(mk_sname);
  vars["lcclassname"] = FullNameToLower(mk_sname);
  vars["ucclassname"] = FullNameToUpper(mk_sname);
  vars["mlcclassname"] = FullNameToLower(descriptor_->full_name());

  std::vector<const FieldDescriptor *> key_fields;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    if (field_generators_.get(field).riftopts.rw_flags & RW_PROTOBUF_FOPT_KEY) {
      key_fields.push_back(field);
    }
  }

  if (!key_fields.size()) {
    fprintf(stderr, "*** Proto Message '%s' is marked as having keys, but none of its fields are keys.\n",
            descriptor_->full_name().c_str());
    exit(1);
  }

  gen_minikey_type_ = true;
  if (key_fields.size() == 1) {
    const FieldDescriptor *field = key_fields[0];
    gen_minikey_type_ = false;
    switch(field->type()) {
      case FieldDescriptor::TYPE_SINT32  :
      case FieldDescriptor::TYPE_SFIXED32:
      case FieldDescriptor::TYPE_INT32   :
        {
          printer->Print(vars, "typedef rw_minikey_basic_int32_t $classname$;\n\n");
          vars["basicmk"] = "rw_minikey_basic_int32_t";
          vars["basicmkinit"] = "rw_minikey_basic_int32_init";
          vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_INT32";
        }
        break;
      case FieldDescriptor::TYPE_SINT64  :
      case FieldDescriptor::TYPE_SFIXED64:
      case FieldDescriptor::TYPE_INT64   :
        {
          printer->Print(vars, "typedef rw_minikey_basic_int64_t $classname$;\n\n");
          vars["basicmk"] = "rw_minikey_basic_int64_t";
          vars["basicmkinit"] = "rw_minikey_basic_int64_init";
          vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_INT64";
        }
        break;
      case FieldDescriptor::TYPE_UINT32  :
      case FieldDescriptor::TYPE_FIXED32 :
        {
          printer->Print(vars, "typedef rw_minikey_basic_uint32_t $classname$;\n\n");
          vars["basicmk"] = "rw_minikey_basic_uint32_t";
          vars["basicmkinit"] = "rw_minikey_basic_uint32_init";
          vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_UINT32";
        }
        break;
      case FieldDescriptor::TYPE_UINT64  :
      case FieldDescriptor::TYPE_FIXED64 :
        {
          printer->Print(vars, "typedef rw_minikey_basic_uint64_t $classname$;\n\n");
          vars["basicmk"] = "rw_minikey_basic_uint64_t";
          vars["basicmkinit"] = "rw_minikey_basic_uint64_init";
          vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_UINT64";
        }
        break;
      case FieldDescriptor::TYPE_FLOAT   :
        {
          printer->Print(vars, "typedef rw_minikey_basic_float_t $classname$;\n\n");
          vars["basicmk"] = "rw_minikey_basic_float_t";
          vars["basicmkinit"] = "rw_minikey_basic_float_init";
          vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_FLOAT";
        }
        break;
      case FieldDescriptor::TYPE_DOUBLE  :
        {
          printer->Print(vars, "typedef rw_minikey_basic_double_t $classname$;\n\n");
          vars["basicmk"] = "rw_minikey_basic_double_t";
          vars["basicmkinit"] = "rw_minikey_basic_double_init";
          vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_DOUBLE";
        }
        break;
      case FieldDescriptor::TYPE_BOOL    :
        {
          printer->Print(vars, "typedef rw_minikey_basic_bool_t $classname$;\n\n");
          vars["basicmk"] = "rw_minikey_basic_bool_t";
          vars["basicmkinit"] = "rw_minikey_basic_bool_init";
          vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_BOOL";
        }
        break;
      case FieldDescriptor::TYPE_STRING  :
        {
          if (field_generators_.get(field).riftopts.flatinline) {
            if (field_generators_.get(field).riftopts.string_max <= RWPB_MINIKEY_MAX_SKEY_LEN) {
              printer->Print(vars, "typedef rw_minikey_basic_string_t $classname$;\n\n");
              vars["basicmk"] = "rw_minikey_basic_string_t";
              vars["basicmkinit"] = "rw_minikey_basic_string_init";
              vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_STRING";
            } else {
              gen_minikey_type_ = true;
            }
          } else {
            printer->Print(vars, "typedef rw_minikey_basic_string_pointy_t $classname$;\n\n");
            vars["basicmk"] = "rw_minikey_basic_string_pointy_t";
            vars["basicmkinit"] = "rw_minikey_basic_string_pointy_init";
            vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_STRING_POINTY";
          }
        }
        break;
      case FieldDescriptor::TYPE_BYTES:
        if (!(field_generators_.get(field).riftopts.c_type[0])) {
          if (field_generators_.get(field).riftopts.flatinline) {
            if (field_generators_.get(field).riftopts.string_max <= RWPB_MINIKEY_MAX_BKEY_LEN) {
              printer->Print(vars, "typedef rw_minikey_basic_bytes_t $classname$;\n\n");
              vars["basicmk"] = "rw_minikey_basic_bytes_t";
              vars["basicmkinit"] = "rw_minikey_basic_bytes_init";
              vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_BYTES";
            } else {
              gen_minikey_type_ = true;
            }
          } else {
            printer->Print(vars, "typedef rw_minikey_basic_bytes_pointy_t $classname$;\n\n");
            vars["basicmk"] = "rw_minikey_basic_bytes_pointy_t";
            vars["basicmkinit"] = "rw_minikey_basic_bytes_pointy_init";
            vars["basicmkdef"] = "RWPB_MINIKEY_DEF_INIT_BYTES_POINTY";
          }
        } else {
          gen_minikey_type_ = true;
        }
        break;
      default:
        gen_minikey_type_ = true;
        break;
    }
  }

  if (gen_minikey_type_) {
    printer->Print(vars,
                   "typedef union $classname$\n"
                   "{\n"
                   "  struct {\n"
                   "    const rw_minikey_desc_t *desc;\n");

    printer->Indent();
    printer->Indent();
    for (int i = 0; i < key_fields.size(); i++) {
      const FieldDescriptor *field = key_fields[i];
      if (field->type() == FieldDescriptor::TYPE_BYTES) {
        const BytesFieldGenerator &bfield = (const BytesFieldGenerator &)(field_generators_.get(field));
        bfield.GenerateMiniKeyStructMembers(printer);
      } else {
        field_generators_.get(field).GenerateStructMembers(printer);
      }
    }
    printer->Outdent();
    printer->Print("} k;\n");
    printer->Print("rw_schema_minikey_opaque_t opaque;\n");
    printer->Outdent();
    printer->Print(vars, "} $classname$;\n\n");
  }

  // Generate descriptor, init function and the def and init macro.
  if (gen_minikey_type_) {
    printer->Print(vars, "extern const rw_minikey_desc_t $lcclassname$__desc;\n");
  }
  printer->Print("static inline\n");
  printer->Print(vars, "void $lcclassname$__init($classname$ *mk)\n");
  printer->Print(vars, "{\n");
  printer->Indent();
  if (!gen_minikey_type_) {
    printer->Print(vars, "$basicmkinit$(mk);\n");
  } else {
    printer->Print(vars, "memset(mk, 0, sizeof($classname$));\n");
    printer->Print(vars, "mk->k.desc = &$lcclassname$__desc;\n");
  }
  printer->Outdent();
  printer->Print("}\n\n");

  // Generate the DEF_INIT macro
  if (!gen_minikey_type_) {
    printer->Print(vars, "#define $ucclassname$__DEF_INIT(var_, key_) \\\n");
    printer->Indent();
    printer->Print(vars, "$basicmkdef$(var_, key_)\n\n");
    printer->Outdent();
  } else {
    std::ostringstream buffer;
    if (key_fields.size() == 1) {
      buffer << "#define $ucclassname$__DEF_INIT(var_, key_";
    } else {
      buffer << "#define $ucclassname$__DEF_INIT(var_";
      for (int k = 0; k < key_fields.size(); k++) {
        buffer << ", key" << k+1 << "_";
        if (key_fields[k]->type() == FieldDescriptor::TYPE_BYTES) {
          buffer << ", keylen" << k+1 << "_";
        }
      }
    }
    buffer << ")\\\n";

    printer->Print(vars, buffer.str().c_str());
    printer->Indent();

    printer->Print(vars, "$classname$ var_;\\\n");
    printer->Print(vars, "$lcclassname$__init(&var_);\\\n");

    for (int i = 0; i < key_fields.size(); i++) {
      const FieldDescriptor *field = key_fields[i];
      if (key_fields.size() == 1) {
        field_generators_.get(field).AssignStructMembers(printer, -1);
      } else {
        field_generators_.get(field).AssignStructMembers(printer, i+1);
        if (i != (key_fields.size()-1)) {
          printer->Print("\\");
        }
      }
      printer->Print("\n");
    }

    printer->Outdent();
    printer->Print("\n\n");
  }

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::
GenerateHelperFunctionDeclarations(io::Printer* printer, bool is_submessage, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateHelperFunctionDeclarations(printer, true, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return; // Suppress.
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
  printer->Print(vars,
		 "/* $classname$ methods */\n"
		 "void   $lcclassname$__init\n"
		 "                     ($classname$         *message);\n"
		);

  if (!is_submessage) {
    printer->Print(vars,
		 "size_t $lcclassname$__get_packed_size\n"
		 "                     (ProtobufCInstance* instance, \n"
         "                      const $classname$   *message);\n"
		 "size_t $lcclassname$__pack\n"
		 "                     (ProtobufCInstance* instance, \n"
         "                      const $classname$   *message,\n"
		 "                      uint8_t             *out);\n"
		 "size_t $lcclassname$__pack_to_buffer\n"
		 "                     (const $classname$   *message,\n"
		 "                      ProtobufCBuffer     *buffer);\n"
		 "$classname$ *\n"
		 "       $lcclassname$__unpack\n"
		 "                     (ProtobufCInstance   *instance,\n"
                 "                      size_t               len,\n"
                 "                      const uint8_t       *data);\n"
		 "$classname$ *\n"
		 "       $lcclassname$__unpack_usebody\n"
		 "                     (ProtobufCInstance   *instance,\n"
                 "                      size_t               len,\n"
                 "                      const uint8_t       *data,\n"
		 "                      $classname$         *body);\n"
		 "void   $lcclassname$__free_unpacked\n"
		 "                     (ProtobufCInstance *instance,\n"
                 "                      $classname$ *message);\n"
		 "void   $lcclassname$__free_unpacked_usebody\n"
		 "                     (ProtobufCInstance *instance,\n"
                 "                      $classname$ *message);\n"
		);
  }

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::
GenerateDescriptorDeclarations(io::Printer* printer, const bool hide_from_gi)
{

  if (!riftmopts_.suppress) {

    if (hide_from_gi) {
      printer->Print("#ifndef __GI_SCANNER__\n");
    }

    std::map<string, string> vars;
    vars["classname"] = FullNameToC(descriptor_->full_name());
    vars["name"] = FullNameToLower(descriptor_->full_name());
    vars["nfields"] = SimpleItoa(descriptor_->field_count());

    //printer->Print(vars, "extern const ProtobufCMessageDescriptor $name$__descriptor;\n");
    printer->Print(vars, "#define $name$__descriptor $name$__concrete_descriptor.base\n");

    /* Extern declaration for concrete Protobuf Message descriptor. */
    printer->Print(vars, "extern const RwProtobufCMessageDescriptor_$classname$ $name$__concrete_descriptor;\n");

    /* Extern declarations for field descriptor is also required for meta-data macros. */
    printer->Print(vars, "extern const ProtobufCFieldDescriptor $name$__field_descriptors[$nfields$];\n");

    if (hide_from_gi) {
      printer->Print("#endif // __GI_SCANNER__\n");
    }

  }

  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateDescriptorDeclarations(printer, hide_child_from_gi);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDescriptorDeclarations(printer);
  }
}

void MessageGenerator::GenerateClosureTypedef(io::Printer* printer, const bool hide_from_gi)
{
  const char *re = getenv("RIFT_ENFORCEOPTS");
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateClosureTypedef(printer, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return; // Suppress.
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  vars["name"] = FullNameToC(descriptor_->full_name());

  if (re && 0==strcmp(re, "1")) {
    printer->Print(vars,
                   "typedef rw_status_t (*$name$_Closure)(const $name$ *message, void *closure_data);\n");
  } else {
    printer->Print(vars,
                   "typedef void (*$name$_Closure)(const $name$ *message, void *closure_data);\n");
  }

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }


}

static int
compare_pfields_by_number (const void *a, const void *b)
{
  const FieldDescriptor *pa = *(const FieldDescriptor **)a;
  const FieldDescriptor *pb = *(const FieldDescriptor **)b;
  if (pa->number() < pb->number()) return -1;
  if (pa->number() > pb->number()) return +1;
  return 0;
}

void MessageGenerator::GenerateMetaDataMacro(io::Printer* printer, bool generate_gi, const bool hide_from_gi)
{
  if (!riftmopts_.suppress) {

    if (hide_from_gi) {
      printer->Print("#ifndef __GI_SCANNER__\n");
    }

    std::map<string, string> vars;
    vars["classname"] = FullNameToC(descriptor_->full_name());
    vars["ucclassname"] = FullNameToUpper(descriptor_->full_name());
    vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
    vars["n_fields"] = SimpleItoa(descriptor_->field_count());
    if (descriptor_->file()->package().length()) {
      vars["ucmodule"] = FullNameToUpper(descriptor_->file()->package());
    } else {
      vars["ucmodule"] = FullNameToUpper(StripProto(descriptor_->file()->name()));
    }
    vars["gi_tname"] = FullNameToGI(descriptor_, &riftmopts_);
    vars["gi_bname"] = FullNameToGICBase(descriptor_->full_name());

    printer->Print(vars, "#define $ucclassname$__msg() \\\n");
    printer->Indent();
    printer->Print(vars, "$classname$, \\\n");
    printer->Print(vars, "$lcclassname$__descriptor, \\\n");
    printer->Print(vars, "$lcclassname$__init, \\\n");
    printer->Print(vars, "$ucmodule$, \\\n");

    // Output the Gi typename and the Gi basename.
    if (generate_gi) {
      printer->Print(vars, "$gi_tname$, \\\n");
      printer->Print(vars, "$gi_bname$, \\\n");
    } else {
      printer->Print(vars, "void,/*no gi_tname generated. */ \\\n");
      printer->Print(vars, "void,/*no gi_bname generated. */ \\\n");
    }

    printer->Print(vars, "$n_fields$, \\\n");

    const FieldDescriptor **sorted_fields = new const FieldDescriptor *[descriptor_->field_count()];
    for (unsigned i = 0; i < descriptor_->field_count(); i++) {
      sorted_fields[i] = descriptor_->field(i);
    }

    /* Comma separated field names enclosed in paranthesis */
    printer->Print(vars, "(");
    if(descriptor_->field_count()) {
      vars["fieldname"] = FieldName(sorted_fields[0]);
      printer->Print(vars, "$fieldname$");
      for(unsigned i=1; i < descriptor_->field_count(); i++) {
        vars["fieldname"] = FieldName(sorted_fields[i]);
        printer->Print(vars, ", $fieldname$");
      }
    }
    printer->Print(vars, ")\n");

    printer->Outdent();
    printer->Print("\n\n");

    qsort (sorted_fields, descriptor_->field_count(),
           sizeof (const FieldDescriptor *),
           compare_pfields_by_number);

    for (unsigned i = 0; i < descriptor_->field_count(); i++) {
      const FieldDescriptor *field = sorted_fields[i];
      field_generators_.get(field).GenerateMetaDataMacro(printer, i);
    }

    if (hide_from_gi) {
      printer->Print("#endif // __GI_SCANNER__\n");
    }

  }

  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateMetaDataMacro(printer, generate_gi, hide_child_from_gi);
  }
}


void MessageGenerator::
GenerateHelperFunctionDefinitions(io::Printer* printer, bool is_submessage, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateHelperFunctionDefinitions(printer, true, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return; // Suppress.
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
  vars["ucclassname"] = FullNameToUpper(descriptor_->full_name());
  printer->Print(vars,
		 "void   $lcclassname$__init\n"
		 "                     ($classname$         *message)\n"
		 "{\n"
         "  protobuf_c_message_init(&$lcclassname$__descriptor, &message->base);\n"
		 "}\n");
    if (!is_submessage) {
    printer->Print(vars,
		 "size_t $lcclassname$__get_packed_size\n"
		 "                     (ProtobufCInstance* instance, \n"
         "                      const $classname$ *message)\n"
		 "{\n"
		 "  PROTOBUF_C_ASSERT (message->base.descriptor == &$lcclassname$__descriptor);\n"
		 "  return protobuf_c_message_get_packed_size (instance, &message->base);\n"
		 "}\n"
		 "size_t $lcclassname$__pack\n"
		 "                     (ProtobufCInstance* instance,\n"
         "                      const $classname$ *message,\n"
		 "                      uint8_t       *out)\n"
		 "{\n"
		 "  PROTOBUF_C_ASSERT (message->base.descriptor == &$lcclassname$__descriptor);\n"
		 "  return protobuf_c_message_pack (instance, &message->base, out);\n"
		 "}\n"
		 "size_t $lcclassname$__pack_to_buffer\n"
		 "                     (const $classname$ *message,\n"
		 "                      ProtobufCBuffer *buffer)\n"
		 "{\n"
		 "  PROTOBUF_C_ASSERT (message->base.descriptor == &$lcclassname$__descriptor);\n"
		 "  return protobuf_c_message_pack_to_buffer (&message->base, buffer);\n"
		 "}\n"
		 "$classname$ *\n"
		 "       $lcclassname$__unpack\n"
		 "                     (ProtobufCInstance   *instance,\n"
		 "                      size_t               len,\n"
                 "                      const uint8_t       *data)\n"
		 "{\n"
		 "  return ($classname$ *)\n"
		 "     protobuf_c_message_unpack (instance, &$lcclassname$__descriptor,\n"
		 "                                len, data);\n"
		 "}\n"
		 "$classname$ *\n"
		 "       $lcclassname$__unpack_usebody\n"
		 "                     (ProtobufCInstance   *instance,\n"
		 "                      size_t               len,\n"
                 "                      const uint8_t       *data,\n"
		 "                      $classname$         *body)\n"
		 "{\n"
		 "  return ($classname$ *)\n"
		 "     protobuf_c_message_unpack_usebody (instance, &$lcclassname$__descriptor,\n"
		 "                                len, data, &body->base);\n"
		 "}\n"
		 "void   $lcclassname$__free_unpacked\n"
		 "                     (ProtobufCInstance *instance,\n"
		 "                      $classname$ *message)\n"
		 "{\n"
		 "  PROTOBUF_C_ASSERT (message->base.descriptor == &$lcclassname$__descriptor);\n"
		 "  protobuf_c_message_free_unpacked (instance, &message->base);\n"
		 "}\n"
		 "void   $lcclassname$__free_unpacked_usebody\n"
		 "                     (ProtobufCInstance *instance,\n"
		 "                      $classname$ *message)\n"
		 "{\n"
		 "  PROTOBUF_C_ASSERT (message->base.descriptor == &$lcclassname$__descriptor);\n"
		 "  protobuf_c_message_free_unpacked_usebody (instance, &message->base);\n"
		 "}\n"
		);
  }

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::
GenerateMessageDescriptor(io::Printer* printer, bool generate_gi, const bool hide_from_gi)
{
  map<string, string> vars;
  vars["fullname"] = descriptor_->full_name();
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
  vars["ucclassname"] = FullNameToUpper(descriptor_->full_name());
  vars["shortname"] = ToCamel(descriptor_->name());
  vars["n_fields"] = SimpleItoa(descriptor_->field_count());
  vars["packagename"] = descriptor_->file()->package();
  vars["riftflags"] = SimpleItoa(riftmopts_.rw_flags);
  if (riftmopts_.ypbc_msg.length()) {
    vars["helper"] = std::string("&") + riftmopts_.ypbc_msg;
  } else {
    vars["helper"] = "NULL";
  }

  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateMessageDescriptor(printer, generate_gi, hide_child_from_gi);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateEnumDescriptor(printer);
    if (enum_generators_[i]->has_rw_yang_enum()) {
      riftmopts_.need_ypbc_include = true;
    }
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *fd = descriptor_->field(i);
    if (fd->has_default_value()) {
      field_generators_.get(fd).GenerateDefaultValueImplementations(printer);
    }
  }

  if (riftmopts_.suppress) {
    return; // Suppress.
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *fd = descriptor_->field(i);
    if (fd->has_default_value()) {

      bool already_defined = false;
      vars["name"] = fd->name();
      vars["lcname"] = CamelToLower(fd->name());
      vars["maybe_static"] = "static ";
      vars["field_dv_ctype_suffix"] = "";
      vars["default_value"] = field_generators_.get(fd).GetDefaultValue();
      switch (fd->cpp_type()) {
        case FieldDescriptor::CPPTYPE_INT32:
          vars["field_dv_ctype"] = "int32_t";
          break;
        case FieldDescriptor::CPPTYPE_INT64:
          vars["field_dv_ctype"] = "int64_t";
          break;
        case FieldDescriptor::CPPTYPE_UINT32:
          vars["field_dv_ctype"] = "uint32_t";
          break;
        case FieldDescriptor::CPPTYPE_UINT64:
          vars["field_dv_ctype"] = "uint64_t";
          break;
        case FieldDescriptor::CPPTYPE_FLOAT:
          vars["field_dv_ctype"] = "float";
          break;
        case FieldDescriptor::CPPTYPE_DOUBLE:
          vars["field_dv_ctype"] = "double";
          break;
        case FieldDescriptor::CPPTYPE_BOOL:
          vars["field_dv_ctype"] = "protobuf_c_boolean";
          break;

        case FieldDescriptor::CPPTYPE_MESSAGE:
          // NOTE: not supported by protobuf
          vars["maybe_static"] = "";
          vars["field_dv_ctype"] = "{ ... }";
          GOOGLE_LOG(DFATAL) << "Messages can't have default values!";
          break;
        case FieldDescriptor::CPPTYPE_STRING:
          if (fd->type() == FieldDescriptor::TYPE_BYTES)
          {
            vars["field_dv_ctype"] = "ProtobufCBinaryData";
          }
          else   /* STRING type */
          {
            already_defined = true;
            vars["maybe_static"] = "";
            vars["field_dv_ctype"] = "char";
            vars["field_dv_ctype_suffix"] = "[]";
          }
          break;
        case FieldDescriptor::CPPTYPE_ENUM:
          {
            const EnumValueDescriptor *vd = fd->default_value_enum();
            vars["field_dv_ctype"] = FullNameToC(vd->type()->full_name());
            break;
          }
        default:
          GOOGLE_LOG(DFATAL) << "Unknown CPPTYPE";
          break;
      }
      if (!already_defined) {
        FieldGenerator *fg = (FieldGenerator*)(const FieldGenerator *)&(field_generators_.get(fd));
        if (!fg->isCType())  {
          printer->Print(vars, "$maybe_static$const $field_dv_ctype$ $lcclassname$__$lcname$__default_value$field_dv_ctype_suffix$ = $default_value$;\n");
        }
      }
    }
  }

  const FieldDescriptor **sorted_fields = NULL;

  if ( descriptor_->field_count() ) {

    sorted_fields = new const FieldDescriptor *[descriptor_->field_count()];
    for (int i = 0; i < descriptor_->field_count(); i++) {
      sorted_fields[i] = descriptor_->field(i);
    }
    qsort (sorted_fields, descriptor_->field_count(),
         sizeof (const FieldDescriptor *),
         compare_pfields_by_number);

    printer->Print(vars,
  	"const ProtobufCFieldDescriptor $lcclassname$__field_descriptors[$n_fields$] =\n"
  	"{\n");
    printer->Indent();
    for (int i = 0; i < descriptor_->field_count(); i++) {
      const FieldDescriptor *field = sorted_fields[i];
      field_generators_.get(field).GenerateDescriptorInitializer(printer);
    }
    printer->Outdent();
    printer->Print(vars, "};\n");

    NameIndex *field_indices = new NameIndex [descriptor_->field_count()];
    for (int i = 0; i < descriptor_->field_count(); i++) {
      field_indices[i].name = sorted_fields[i]->name().c_str();
      field_indices[i].index = i;
    }
    qsort (field_indices, descriptor_->field_count(), sizeof (NameIndex),
           compare_name_indices_by_name);
    printer->Print(vars, "static const unsigned $lcclassname$__field_indices_by_name[] = {\n");
    for (int i = 0; i < descriptor_->field_count(); i++) {
      vars["index"] = SimpleItoa(field_indices[i].index);
      vars["name"] = field_indices[i].name;
      printer->Print(vars, "  $index$,   /* field[$index$] = $name$ */\n");
    }
    printer->Print("};\n");
    delete[] field_indices;

    // create range initializers
    int *values = new int[descriptor_->field_count()];
    for (int i = 0; i < descriptor_->field_count(); i++) {
      values[i] = sorted_fields[i]->number();
    }
    int n_ranges = WriteIntRanges(printer,
          descriptor_->field_count(), values,
          vars["lcclassname"] + "__number_ranges");
    delete [] values;

    vars["n_ranges"] = SimpleItoa(n_ranges);
  } else {
      /* MS compiler can't handle arrays with zero size and empty
       * initialization list. Furthermore it is an extension of GCC only but
       * not a standard. */
    vars["n_ranges"] = "0";
    printer->Print(vars,
        "#define $lcclassname$__field_descriptors NULL\n"
        "#define $lcclassname$__field_indices_by_name NULL\n"
        "#define $lcclassname$__number_ranges NULL\n");
  }

  if (generate_gi) {
    vars["gi_c_base"] = GetGiCIdentifier();
    vars["gi_new_create_fn"] = GetGiCIdentifier("new");
    vars["gi_new_adopt_fn"] = GetGiCIdentifier("new_adopt");
    vars["gi_unref_fn"] = GetGiCIdentifier("unref");
    vars["gi_invalidate_fn"] = GetGiCIdentifier("invalidate");
    vars["gi_desc"] = "&" + vars["lcclassname"] + "__gi_descriptor";
    vars["gi_gi_tname"] = FullNameToGIType(descriptor_);
    vars["gi_tname"] = gi_typename_;
    printer->Print(vars, "static const ProtobufCMessageGiDescriptor $lcclassname$__gi_descriptor =\n"
                         "{\n"
                         "  .new_create = (ProtobufCMessageGiNewCreate)&$gi_new_create_fn$,\n"
                         "  .new_adopt = (ProtobufCMessageGiNewAdopt)&$gi_new_adopt_fn$,\n"
                         "  .unref = (ProtobufCMessageGiUnref)&$gi_unref_fn$,\n"
                         "  .invalidate = (ProtobufCMessageGiInvalidate)&$gi_invalidate_fn$,\n"
                         "  .name = \"$gi_gi_tname$\",\n"
                         "  .boxed_size = sizeof($gi_tname$),\n"
                         "};\n");
  } else {
    vars["gi_desc"] = "NULL";
  }

  /* Generate the file static init_value const */
  printer->Print(vars, "static const $classname$ $lcclassname$__init_value = \n"
                       "       __$ucclassname$__INTERNAL_INITIALIZER__(PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0);\n");

  printer->Print(vars, "ProtobufCMessageDebugStats $lcclassname$__debug_stats;\n");

  /* Generate Protobuf Concerete Message descriptor. */
  printer->Print(vars, "const RwProtobufCMessageDescriptor_$classname$ $lcclassname$__concrete_descriptor =\n"
                       "{\n");
  printer->Indent();
  printer->Print("{\n");

  printer->Print(vars,
  "  PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC,\n"
  "  \"$fullname$\",\n"
  "  \"$shortname$\",\n"
  "  \"$classname$\",\n"
  "  \"$packagename$\",\n"
  "  sizeof($classname$),\n"
  "  $n_fields$,\n"
  "  $lcclassname$__field_descriptors,\n"
  "  $lcclassname$__field_indices_by_name,\n"
  "  $n_ranges$,"
  "  $lcclassname$__number_ranges,\n"
  "  (const ProtobufCMessage *)&$lcclassname$__init_value,\n"
  "  $gi_desc$,\n"
  "  NULL,NULL,NULL,   /* reserved[123] */\n"
  "  $riftflags$,      /* riftflags */\n"
  "  (const struct rw_yang_pb_msgdesc_t*)($helper$),\n"
  "  &$lcclassname$__debug_stats,\n"
  "},\n");

  printer->Outdent();

  if (descriptor_->field_count() > 0) {
    printer->Indent();
    printer->Print("{\n");
    printer->Indent();
  }
  for (unsigned i = 0; i < descriptor_->field_count(); i++) {
    /* ATTN: Check this.. double for loop here.. */
    for (unsigned j = 0; j < descriptor_->field_count(); j++) {
      if (sorted_fields[j]->number() == descriptor_->field(i)->number()) {
        vars["index"] = SimpleItoa(j);
      }
    }
    printer->Print(vars, "&$lcclassname$__field_descriptors[$index$],\n");
  }
  if (descriptor_->field_count() > 0) {
    printer->Outdent();
    printer->Print("}\n");
    printer->Outdent();

    delete [] sorted_fields;
  }
  printer->Print("};\n\n");

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::
GenerateMinikeyDescriptor(io::Printer* printer, const bool hide_from_gi)
{

  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateMinikeyDescriptor(printer, hide_child_from_gi);
  }

  if (!gen_minikey_type_) {
    return;
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  std::string mk_sname = descriptor_->full_name()+".mini_key";
  vars["classname"] = FullNameToC(mk_sname);
  vars["lcclassname"] = FullNameToLower(mk_sname);
  vars["ucclassname"] = FullNameToUpper(mk_sname);
  vars["mlcclassname"] = FullNameToLower(descriptor_->full_name());

  std::vector<const FieldDescriptor *> key_fields;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    if (field_generators_.get(field).riftopts.rw_flags & RW_PROTOBUF_FOPT_KEY) {
      key_fields.push_back(field);
    }
  }

  printer->Print(vars, "const rw_minikey_desc_t $lcclassname$__desc =\n");
  printer->Print("{\n");
  printer->Indent();
  printer->Print(vars, ".pbc_mdesc = &$mlcclassname$__descriptor,\n");
  char type[20];
  sprintf(type, "%d", key_fields.size());
  printer->Print(".key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_$type$,\n",
                 "type", type);
  printer->Outdent();
  printer->Print("};\n");

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::
GeneratePBCMDStructDefinition(io::Printer* printer)
{
  map<string, string> vars;
  vars["fullname"] = descriptor_->full_name();
  vars["classname"] = FullNameToC(descriptor_->full_name());

  printer->Print(vars, "struct RwProtobufCMessageDescriptor_$classname$\n"
                 "{\n"
                 "  ProtobufCMessageDescriptor base;\n");

  if (descriptor_->field_count() > 0) {
    printer->Indent();
    printer->Print("struct\n"
                   "{\n");
    printer->Indent();
  }

  for (unsigned i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *fd = descriptor_->field(i);
    vars["fname"] = FieldName(fd);
    printer->Print(vars, "const ProtobufCFieldDescriptor* $fname$;\n");
  }

  if (descriptor_->field_count() > 0) {
    printer->Outdent();
    printer->Print("} fields;\n");
    printer->Outdent();
  }

  printer->Print("};\n");

  printer->Print(vars, "struct RwProtobufCMessageBase_$classname$\n"
                 "{\n"
                 " ProtobufCReferenceHeader ref_hdr; \n" 
                 " RwProtobufCMessageDescriptor_$classname$ *descriptor;\n"
                 "};\n");
}

/*
 * Rift GI code generation.
 */
string MessageGenerator::GetGiCIdentifier(const char *operation)
{
  if (operation) {
    return FullNameToGICBase(descriptor_->full_name()) + "_" + operation;
  } else {
    return FullNameToGICBase(descriptor_->full_name());
  }
}

string MessageGenerator::GetGiDescTypeName()
{
  vector<string> pieces;
  SplitStringUsing(descriptor_->full_name(), ".", &pieces);
  string rv = "";
  for (unsigned i = 0; i < pieces.size(); i++) {
    if (pieces[i] == "") continue;
    if (rv == "") {
      // Insert PBCMD after package name
      rv = ToCamel(pieces[i]) + "_" + "PBCMD";
    } else {
      rv = rv + "_" + ToCamel(pieces[i]);
    }
  }
  return "rwpb_gi_" + rv;
}

string MessageGenerator::GetGiDescIdentifier(const char *operation)
{
  vector<string> pieces;
  SplitStringUsing(descriptor_->full_name(), ".", &pieces);
  string rv = "";
  for (unsigned i = 0; i < pieces.size(); i++) {
    if (pieces[i] == "") continue;
    if (rv == "") {
      // Insert PBCMD after package name
      rv = CamelToLower(pieces[i]) + "__" + "pbcmd";
    } else {
      rv = rv + "__" + CamelToLower(pieces[i]);
    }
  }
  rv = rv + "__gi";
  if (operation) {
    rv = rv + "_" + operation;
  }
  return rv;
}

void MessageGenerator::GenerateGiTypeDefDecls(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateGiTypeDefDecls(printer, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return;
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  vars["gi_tn"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["gi_desc_tname"] = GetGiDescTypeName();

  printer->Print(vars, "typedef struct $gi_tn$ $gi_tn$;\n"
                       "typedef struct RwProtobufCMessageDescriptor_$classname$ $gi_desc_tname$;\n");

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::GenerateGiCBoxedStructDefs(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateGiCBoxedStructDefs(printer, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return;
  }

  std::map<string, string> vars;
  vars["gi_tn"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());

  // The union allows type-punning under strict aliasing.
  printer->Print(vars, "struct $gi_tn$ {\n"
                       "  union {\n"
                       "    ProtobufCGiMessageBox box;\n"
                       "    struct {\n"
                       "      ProtobufCGiMessageBase gi_base;\n"
                       "      $classname$* message;\n\n"
                       "    } s;\n"
                       "  };\n");
  printer->Indent();
  for (int i = 0; i < descriptor_->field_count(); i++) {
    if ((descriptor_->field(i)->type() == FieldDescriptor::TYPE_MESSAGE)) {
      vars["child_gi_tn"] = FullNameToGI(descriptor_->field(i)->message_type());
      printer->Print(vars, "$child_gi_tn$*");
      if (descriptor_->field(i)->label() == FieldDescriptor::LABEL_REPEATED) {
        printer->Print("*");
      }
      vars["field_name"] = FieldName(descriptor_->field(i));
      printer->Print(vars, " boxed_$field_name$;\n");
    }
  }

  printer->Outdent();
  printer->Print("};\n");
}

void MessageGenerator::GenerateGiRefHelperDecls(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateGiRefHelperDecls(printer, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return;
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  std::map<string, string> vars;
  vars["gi_type_ptr"] = gi_typename_ + "*";
  vars["invalidate_fn"] = GetGiCIdentifier("invalidate");
  vars["unref_fn"] = GetGiCIdentifier("unref");
  vars["new_adopt_fn"] = GetGiCIdentifier("new_adopt");
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["gi_type"] = gi_typename_;

  // Output the unref prototype
  printer->Print("\n");
  printer->Print(vars, "void $unref_fn$($gi_type_ptr$ boxed);\n");

  // Output the new prototype
  printer->Print(vars, "$gi_type$ * $new_adopt_fn$($classname$ *message, ProtobufCGiMessageBox *parent);\n");

  // Output the invalidate prototype
  printer->Print(vars, "void $invalidate_fn$($gi_type_ptr$ boxed)  G_GNUC_UNUSED;\n\n");

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::GenerateGiToPbufMethodDecl(io::Printer* printer)
{

  std::map<string, string> vars;
  vars["to_pbuf"] = GetGiCIdentifier("to_pbuf");
  vars["gi_tname"] = gi_typename_;

  // Output the comment block with annotation
  printer->Print(vars, "/**\n"
                       " * $to_pbuf$:\n"
                       " * @boxed:(in)\n"
                       " * @pbuf_len:(out)\n"
                       " * Returns : (transfer full)(array length=pbuf_len)\n"
                       " **/\n"
                       "guint8* $to_pbuf$($gi_tname$* boxed, guint32 *pbuf_len);\n");
}

void MessageGenerator::GenerateGiFromPbufMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["from_pbuf"] = GetGiCIdentifier("from_pbuf");
  vars["gi_tname"] = gi_typename_;

  // Output the comment block with annotation
  printer->Print(vars, "/**\n"
                       " * $from_pbuf$:\n"
                       " * @boxed:\n"
                       " * @pbuf_string: (in)(array length=pbuf_len):\n"
                       " * @pbuf_len: (in):\n"
                       " **/\n"
                       "void $from_pbuf$($gi_tname$* boxed, guint8* pbuf_string, guint32 pbuf_len, GError **error);\n");
}

void MessageGenerator::GenerateGiToPtrMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["to_ptr"] = GetGiCIdentifier("to_ptr");
  vars["gi_tname"] = gi_typename_;

  // Output the comment block with annotation
  printer->Print(vars, "/**\n"
                       " * $to_ptr$:\n"
                       " * @boxed:(in)\n"
                       " * Returns : (transfer none)\n"
                       " **/\n"
                       "gpointer $to_ptr$($gi_tname$ *boxed);\n");
}

void MessageGenerator::GenerateGiRetrieveDescriptorMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["retrieve_descriptor"] = GetGiCIdentifier("retrieve_descriptor");
  vars["gi_tname"] = gi_typename_;

  // Output the comment block with annotation
  printer->Print(vars, "/**\n"
                       " * $retrieve_descriptor$:\n"
                       " * @boxed:(in)\n"
                       " * Returns: (type ProtobufC.MessageDescriptor):\n"
                       " **/\n"
                       "const ProtobufCMessageDescriptor* $retrieve_descriptor$($gi_tname$ *boxed);\n");
}


void MessageGenerator::GenerateGiToPbcmMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["to_pbcm"] = GetGiCIdentifier("to_pbcm");
  vars["gi_tname"] = gi_typename_;

  printer->Print(vars, "/**\n"
                       " * $to_pbcm$\n"
                       " * @boxed:\n"
                       " * Returns: (type ProtobufC.Message):\n"
                       " */\n"
                       "const ProtobufCMessage* $to_pbcm$($gi_tname$ *boxed);\n");
}

void MessageGenerator::GenerateGiFromPbcmMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["from_pbcm"] = GetGiCIdentifier("from_pbcm");
  vars["gi_tname"] = gi_typename_;

  printer->Print(vars, "/**\n"
                       " * $from_pbcm$:\n"
                       " * @pbcm:   (transfer none)(type ProtobufC.Message)\n"
                       " * Returns: (transfer full)\n"
                       " **/\n"
                       " $gi_tname$* $from_pbcm$(const ProtobufCMessage* pbcm);\n");
}

void MessageGenerator::GenerateGiSchemaMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["schema"] = GetGiCIdentifier("schema");
  vars["gi_desc_tname"] = GetGiDescTypeName();

  // Output the comment block with annotation
  printer->Print(vars, "/**\n"
                       " * $schema$:\n"
                       " * Returns: (transfer none)\n"
                       " **/\n"
                       "const $gi_desc_tname$* $schema$(void);\n");
}

void MessageGenerator::GenerateGiSchemaChangeToDescriptorMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["change_to_descriptor"] = GetGiCIdentifier("change_to_descriptor");
  vars["gi_tname"] = gi_typename_;
  vars["gi_desc_tname"] = GetGiDescTypeName();
  // Output the comment block with annotation
  printer->Print(vars, "/**\n"
                       " * $change_to_descriptor$:\n"
                       " * Returns: (transfer none) (type ProtobufC.MessageDescriptor):\n"
                       " **/\n"
                       "const ProtobufCMessageDescriptor* $change_to_descriptor$(const $gi_desc_tname$ *schema);\n");
}


void MessageGenerator::GenerateGiHasFieldMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["has_field"] = GetGiCIdentifier("has_field");
  vars["gi_tname"] = gi_typename_;

  printer->Print(vars, "/**\n"
                       " * $has_field$:\n"
                       " * @boxed:\n"
                       " * @field: \n"
                       " * \n"
                       " * Returns:\n"
                       " **/\n"
                       "gboolean $has_field$($gi_tname$ *boxed, const gchar* field, GError** err);\n");
}


void MessageGenerator::GenerateGiCopyFromMethodDecl(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["copy_from"] = GetGiCIdentifier("copy_from");
  vars["gi_tname"] = gi_typename_;

  printer->Print(vars, "/**\n"
                       " * $copy_from$:\n"
                       " * @boxed:\n"
                       " * @other: (transfer none)\n"
                       " * \n"
                       " **/\n"
                       "void $copy_from$($gi_tname$* boxed, $gi_tname$* other, GError** err);\n");
}

void MessageGenerator::GenerateGiHDecls(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateGiHDecls(printer, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return;
  }

  std::map<string, string> vars;
  vars["new_fn"] = GetGiCIdentifier("new");
  vars["create_fn"] = GetGiCIdentifier("create");
  vars["h_macro"] = GetGiCIdentifier("helper_macro");
  vars["get_type"] = GetGiCIdentifier("get_type");
  vars["gi_tname"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["desc_get_type"] = GetGiDescIdentifier("get_type");


  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  printer->Print(vars, "#define $h_macro$(_boxed) \\\n"
                       "  PROTOBUF_C_GI_GET_PROTO((_boxed), $gi_tname$, $classname$)\n"
                       "GType $get_type$();\n"
                       "GType $desc_get_type$();\n\n");

  // Output the constructor with comments
  printer->Print(vars, "/**\n"
                       " * $new_fn$: (constructor)\n"
                       " **/\n");
  printer->Print(vars, "$gi_tname$* $new_fn$();\n");

  GenerateGiToPbufMethodDecl(printer);
  GenerateGiFromPbufMethodDecl(printer);
  GenerateGiToPtrMethodDecl(printer);
  GenerateGiToPbcmMethodDecl(printer);
  GenerateGiFromPbcmMethodDecl(printer);
  GenerateGiSchemaMethodDecl(printer);
  GenerateGiSchemaChangeToDescriptorMethodDecl(printer);
  GenerateGiCopyFromMethodDecl(printer);
  GenerateGiHasFieldMethodDecl(printer);
  GenerateGiRetrieveDescriptorMethodDecl(printer);
  
  for (unsigned i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    field_generators_.get(field).GenerateGiHSupportMethodDecls(printer);
  }

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::GenerateGiBoxedUnrefDefinition(io::Printer* printer)
{
  std::map<string, string> vars;

  vars["gi_type_ptr"] = gi_typename_ + "*";
  vars["unref_fn"] = GetGiCIdentifier("unref");

  // Output the unref definitions
  printer->Print("\n");
  printer->Print(vars, "void $unref_fn$($gi_type_ptr$ boxed)\n{\n");
  printer->Indent();

  printer->Print( "protobuf_c_boolean delete_me = protobuf_c_message_gi_unref(&boxed->box);\n"
                  "if (!delete_me) {\n"
                  "  return;\n"
                  "}\n" );

  // Unref all the children.
  for (unsigned i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    FieldGenerator *fg = (FieldGenerator*)(const FieldGenerator *)&(field_generators_.get(field));

    if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
      vars["child_unref_fn"] = FullNameToGICBase(field->message_type()->full_name()) + "_unref";
      vars["boxed_fname"] = "boxed_"+field->name();
      vars["gi_ctname"] =  FullNameToGI(field->message_type());
      printer->Print(vars, "\nif (boxed->$boxed_fname$ != NULL) {\n");
      printer->Indent();

      if (field->label() == FieldDescriptor::LABEL_REPEATED) {
        printer->Print(vars, "$gi_ctname$** child_boxedp = boxed->$boxed_fname$;\n"
                             "while (*child_boxedp != NULL) {\n"
                             "  $child_unref_fn$(*child_boxedp);\n"
                             "  ++child_boxedp;\n" /* Assumed NULL terminate list */
                             "}\n"
                             "protobuf_c_gi_boxed_free(boxed->$boxed_fname$);\n");
      } else {
        printer->Print(vars, "$child_unref_fn$(boxed->$boxed_fname$);\n");
      }
      printer->Outdent();
      printer->Print("}\n");
    }
  }

  printer->Print("\nif (boxed->s.message != NULL) {\n"
                 "  protobuf_c_message_free_unpacked(NULL, boxed->box.message);\n"
                 "  boxed->s.message = NULL;\n"
                 "}\n"
                 "boxed->s.gi_base.magic++;\n" /* make invalid, but pretty close */
                 "protobuf_c_gi_boxed_free(boxed);\n");
  printer->Outdent();
  printer->Print("}\n");
}

void MessageGenerator::GenerateGiBoxedRegistration(io::Printer* printer)
{
  GenerateGiBoxedUnrefDefinition(printer);

  std::map<string, string> vars;
  vars["gi_tname"] = gi_typename_;
  vars["c_base"] = GetGiCIdentifier();
  vars["unref_fn"] = GetGiCIdentifier("unref");

  vars["gi_desc_tname"] = GetGiDescTypeName();
  vars["c_desc_base"] = GetGiDescIdentifier();
  vars["desc_ref_fn"] = "protobuf_c_message_descriptor_gi_ref";
  vars["desc_unref_fn"] = "protobuf_c_message_descriptor_gi_unref";

  // Output the boxed type registrations
  printer->Print(vars, "G_DEFINE_BOXED_TYPE(\n"
                       "                   $gi_tname$,\n"
                       "                   $c_base$,\n"
                       "                   ($gi_tname$*(*)($gi_tname$*))protobuf_c_message_gi_ref,\n"
                       "                   $unref_fn$)\n");
  printer->Print(vars, "G_DEFINE_BOXED_TYPE(\n"
                       "                   $gi_desc_tname$,\n"
                       "                   $c_desc_base$,\n"
                       "                   ($gi_desc_tname$*(*)($gi_desc_tname$*))$desc_ref_fn$,\n"
                       "                   (void(*)($gi_desc_tname$*))$desc_unref_fn$)\n\n");
}

void MessageGenerator::GenerateGiNewMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["gi_tname"] = gi_typename_;
  vars["new_fn"] = GetGiCIdentifier("new");
  vars["new_adopt_fn"] = GetGiCIdentifier("new_adopt");
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());

  printer->Print(vars, "$gi_tname$* $new_fn$(void)\n"
                       "{\n"
                       "  $classname$* message = ($classname$*)\n"
                       "    protobuf_c_message_create(NULL, &$lcclassname$__descriptor);\n");

  for (unsigned i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    if (   field->label() == FieldDescriptor::LABEL_REPEATED
        && field->type() == FieldDescriptor::TYPE_BYTES) {
      // ATTN: RIFT-5593
      continue;
    }
    FieldGenerator *fg = (FieldGenerator*)(const FieldGenerator *)&(field_generators_.get(field));
    if (   field->type() == FieldDescriptor::TYPE_BYTES
        && fg->isCType()) {
      // ATTN: RIFT-5608
      continue;
    }
    vars["fname"] = FieldName(field);
    if ((field->type() == FieldDescriptor::TYPE_BYTES ||
         field->type() == FieldDescriptor::TYPE_STRING)  && !fg->riftopts.flatinline) {
      if (field->label() != FieldDescriptor::LABEL_REPEATED) {
        if (field->type() == FieldDescriptor::TYPE_BYTES) {
          printer->Print(vars, "  message->$fname$.len = 0;\n"
                               "  message->$fname$.data = NULL;\n");
        } else {
          printer->Print(vars, "  message->$fname$ = NULL;\n");
        }
      } else {
        printer->Print(vars, "  message->n_$fname$ = 0; \n"
                             "  message->$fname$ = NULL; \n");
      }
    }
  }

  printer->Print(vars, "  return $new_adopt_fn$(message, NULL);\n"
                       "}\n\n");

  printer->Print(vars, "$gi_tname$* $new_adopt_fn$($classname$ *message,\n"
                       "     ProtobufCGiMessageBox *parent)\n"
                       "{\n"
                       "  return ($gi_tname$*)\n"
                       "    protobuf_c_message_gi_new_adopt(&message->base, parent);\n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiInvalidateMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["invalidate_fn"] = GetGiCIdentifier("invalidate");
  vars["unref_fn"] = GetGiCIdentifier("unref");
  vars["gi_tname"] = gi_typename_;

  printer->Print(vars, "void $invalidate_fn$($gi_tname$* boxed)\n"
                       "{\n");

  printer->Indent();
  printer->Print(vars, "protobuf_c_message_gi_invalidate_start(&boxed->box);\n");

  for (unsigned i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    FieldGenerator *fg = (FieldGenerator*)(const FieldGenerator *)&(field_generators_.get(field));
    if ((field->type() == FieldDescriptor::TYPE_MESSAGE) && (fg->riftopts.flatinline == true)) {
      vars["boxed_fname"] = "boxed_"+field->name();
      vars["child_invalidate_fn"] = FullNameToGICBase(field->message_type()->full_name()) + "_invalidate";
      if (field->label() == FieldDescriptor::LABEL_REPEATED) {
          printer->Print(vars, "if (boxed->$boxed_fname$ != NULL) {\n"
                               "  size_t i;\n"
                               "  for (i = 0; boxed->$boxed_fname$[i] != NULL; i++) {\n"
                               "    $child_invalidate_fn$(boxed->$boxed_fname$[i]);\n"
                               "  }\n"
                               "}\n");
      } else {
        printer->Print(vars, "if (boxed->$boxed_fname$ != NULL) {\n"
                             "  $child_invalidate_fn$(boxed->$boxed_fname$);\n"
                             "}\n");
      }
    }
  }

  printer->Print(vars, "protobuf_c_message_gi_invalidate_complete(&boxed->box);\n"
                       "$unref_fn$(boxed);\n");
  printer->Outdent();
  printer->Print("}\n\n");
}

void MessageGenerator::GenerateGiToPbufMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["to_pbuf_fn"] = GetGiCIdentifier("to_pbuf");
  vars["gi_tname"] = gi_typename_;

  printer->Print(vars, "guint8* $to_pbuf_fn$($gi_tname$* boxed, guint32 *pbuf_len)\n"
                       "{\n"
                       "  return ((guint8*)protobuf_c_message_serialize(NULL, boxed->box.message, (size_t*)pbuf_len));\n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiFromPbufMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["from_pbuf_fn"] = GetGiCIdentifier("from_pbuf");
  vars["gi_tname"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  if (descriptor_->file()->package().length()) {
    vars["domain"] = MangleNameToUpper(descriptor_->file()->package());
  } else {
    vars["domain"] = MangleNameToUpper(StripProto(descriptor_->file()->name()));
  }

  printer->Print(vars, "void $from_pbuf_fn$($gi_tname$ *boxed, guint8* pbuf_string, guint32 pbuf_len, GError **error)\n"
                       "{\n"
                       "  $classname$ *old_msg = boxed->s.message;\n"
                       "  ProtobufCGiMessageBase *gi_base = &boxed->s.gi_base;\n"
                       "  RW_ASSERT(PROTOBUF_C_GI_BASE_IS_GOOD(gi_base));\n" 
                       "  if (gi_base->ref_count > 1) {\n"
                       "   PROTOBUF_C_GI_RAISE_EXCEPTION(error, $domain$, PROTO_GI_ERROR_XML_TO_PB_FAILURE, \"Failed PBUF->PB current refcount [%u]  \\n\", gi_base->ref_count);\n"
                       "   return;\n"
                       "  }\n"
                       "  RW_ASSERT(old_msg);\n"
                       "  boxed->s.message = ($classname$ *) protobuf_c_message_unpack(NULL, old_msg->base.descriptor, pbuf_len, pbuf_string);\n"
                       "  if (boxed->s.message == NULL) {\n"
                       "    boxed->s.message = old_msg; // restore old message\n"
                       "    PROTOBUF_C_GI_RAISE_EXCEPTION(error, $domain$, PROTO_GI_ERROR_XML_TO_PB_FAILURE,\"Failed PBUF->PB %u bytes \\n\", pbuf_len);\n"
                       "    return;\n"
                       "  } else {\n"
                       "     protobuf_c_message_free_unpacked(NULL, &old_msg->base);\n"
                       "  }\n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiToPtrMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["to_ptr_fn"] = GetGiCIdentifier("to_ptr");
  vars["gi_tname"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());

  printer->Print(vars, "gpointer $to_ptr_fn$($gi_tname$ *boxed)\n"
                       "{\n"
                       "  return ((gpointer)boxed->box.message);\n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiRetrieveDescriptorMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["retrieve_descriptor"] = GetGiCIdentifier("retrieve_descriptor");
  vars["gi_tname"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());

  printer->Print(vars, "const ProtobufCMessageDescriptor* $retrieve_descriptor$($gi_tname$* boxed)\n"
                       "{\n"
                       "  return boxed->box.message->descriptor;\n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiToPbcmMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["to_pbcm_fn"] = GetGiCIdentifier("to_pbcm");
  vars["gi_tname"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());

  printer->Print(vars, "const ProtobufCMessage* $to_pbcm_fn$($gi_tname$* boxed)\n"
                       "{\n"
                       "  return boxed->box.message;\n"
                       "}\n\n");
}


void MessageGenerator::GenerateGiFromPbcmMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["from_pbcm_fn"] = GetGiCIdentifier("from_pbcm");
  vars["new_adopt_fn"] = GetGiCIdentifier("new_adopt");
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
  vars["gi_tname"] = gi_typename_;

  printer->Print(vars, "$gi_tname$* $from_pbcm_fn$(const ProtobufCMessage* pbcm)\n"
                       "{\n"
                       "  $classname$ *message = ($classname$ *) protobuf_c_message_duplicate_allow_deltas(NULL, pbcm, &$lcclassname$__descriptor);\n"
                       "  return $new_adopt_fn$(message, NULL);\n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiSchemaMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["schema"] = GetGiCIdentifier("schema");
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
  vars["gi_desc_tname"] = GetGiDescTypeName();

  printer->Print(vars, "const $gi_desc_tname$* $schema$(void)\n"
                       "{\n"
                       "  return &$lcclassname$__concrete_descriptor;\n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiSchemaChangeToDescriptorMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["change_to_descriptor"] = GetGiCIdentifier("change_to_descriptor");
  vars["gi_tname"] = gi_typename_;
  vars["gi_desc_tname"] = GetGiDescTypeName();
  

  printer->Print(vars, "const ProtobufCMessageDescriptor* $change_to_descriptor$(const $gi_desc_tname$* schema)\n"
                       "{\n"
                         "  return (const ProtobufCMessageDescriptor *) schema; \n"
                       "}\n\n");
}

void MessageGenerator::GenerateGiHasFieldMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["has_field"] = GetGiCIdentifier("has_field");
  vars["gi_tname"] = gi_typename_;

  if (descriptor_->file()->package().length()) {
    vars["domain"] = MangleNameToUpper(descriptor_->file()->package());
  } else {
    vars["domain"] = MangleNameToUpper(StripProto(descriptor_->file()->name()));
  }

  printer->Print(vars, "gboolean $has_field$($gi_tname$ *boxed, const gchar* field, GError **err)\n"
                       "{\n"
                       "  PROTOBUF_C_GI_CHECK_FOR_ERROR_RETVAL(boxed, err, $domain$, 0);\n"
                       "  return protobuf_c_message_has_field_by_name(NULL, boxed->box.message, field); \n"
                       "}\n\n");
}


void MessageGenerator::GenerateGiCopyFromMethod(io::Printer* printer)
{
  std::map<string, string> vars;
  vars["copy_from"] = GetGiCIdentifier("copy_from");
  vars["gi_tname"] = gi_typename_;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());

  if (descriptor_->file()->package().length()) {
    vars["domain"] = MangleNameToUpper(descriptor_->file()->package());
  } else {
    vars["domain"] = MangleNameToUpper(StripProto(descriptor_->file()->name()));
  }

  printer->Print(vars, "void $copy_from$($gi_tname$ *boxed, $gi_tname$* other, GError **err)\n"
                       "{\n"
                       "  PROTOBUF_C_GI_CHECK_FOR_ERROR(other, err, $domain$);\n\n"
                       "  ProtobufCGiMessageBase *gi_base = &boxed->s.gi_base;\n"
                       "  RW_ASSERT(PROTOBUF_C_GI_BASE_IS_GOOD(gi_base));\n" 
                       "  if (gi_base->ref_count > 1) {\n"
                       "     PROTOBUF_C_GI_RAISE_EXCEPTION(err, $domain$, PROTO_GI_ERROR_FAILURE, \"Failed CopyFrom current refcount [%u]  \\n\", gi_base->ref_count);\n"
                       "    return;\n"
                       "  }\n"
                       "  $classname$ *old_msg = boxed->s.message;\n"
                       "  RW_ASSERT(old_msg);\n"
                       "  boxed->s.message = ($classname$ *) protobuf_c_message_duplicate(NULL, other->box.message, &$lcclassname$__descriptor);\n"
                       "  if (boxed->s.message == NULL) {\n"
                       "    boxed->s.message = old_msg; // Restore back. \n"
                       "     PROTOBUF_C_GI_RAISE_EXCEPTION(err, $domain$, PROTO_GI_ERROR_FAILURE, \"Failed to CopyFrom [%u]\\n\", 0);\n"
                       "    return; \n"
                       "  } else {\n"
                       "    protobuf_c_message_free_unpacked(NULL, &old_msg->base);\n"
                       "  }\n"
                       "}\n\n");

}

void MessageGenerator::GenerateGiCDefs(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateGiCDefs(printer, hide_child_from_gi);
  }

  if (riftmopts_.suppress) {
    return;
  }

  if (hide_from_gi) {
    printer->Print("#ifndef __GI_SCANNER__\n");
  }

  GenerateGiBoxedRegistration(printer);
  GenerateGiNewMethod(printer);
  GenerateGiInvalidateMethod(printer);
  GenerateGiToPbufMethod(printer);
  GenerateGiFromPbufMethod(printer);
  GenerateGiToPtrMethod(printer);
  GenerateGiToPbcmMethod(printer);
  GenerateGiFromPbcmMethod(printer);
  GenerateGiSchemaMethod(printer);
  GenerateGiCopyFromMethod(printer);
  GenerateGiSchemaChangeToDescriptorMethod(printer);
  GenerateGiHasFieldMethod(printer);
  GenerateGiRetrieveDescriptorMethod(printer);
  for (unsigned i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor *field = descriptor_->field(i);
    field_generators_.get(field).GenerateGiCSupportMethodDefs(printer);
  }

  printer->Print("\n");

  if (hide_from_gi) {
    printer->Print("#endif // __GI_SCANNER__\n");
  }

}

void MessageGenerator::GenerateGiHEnums(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateGiHEnums(printer, hide_child_from_gi);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateGiHEnums(printer);
  }
}

void MessageGenerator::GenerateGiCEnumDefs(io::Printer* printer, const bool hide_from_gi)
{
  const bool hide_child_from_gi = riftmopts_.hide_from_gi || hide_from_gi;
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->GenerateGiCEnumDefs(printer, hide_child_from_gi);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateGiCEnumDefs(printer);
  }
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
