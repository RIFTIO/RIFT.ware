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

#include <protoc-c/c_field.h>
#include <protoc-c/c_primitive_field.h>
#include <protoc-c/c_string_field.h>
#include <protoc-c/c_bytes_field.h>
#include <protoc-c/c_enum_field.h>
#include <protoc-c/c_message_field.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/printer.h>

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

void FieldGenerator::rift_parseopts(const FieldDescriptor *descriptor_) {

  const char *re = getenv("RIFT_ENFORCEOPTS");
  if (re && 0==strcmp(re, "1")) {
    riftopts.enforce = 1;
  }

  const UnknownFieldSet &unkfs = descriptor_->options().unknown_fields();
  UnknownFieldSet* unks = (UnknownFieldSet*) &unkfs;//descriptor_->options().mutable_unknown_fields();
  //  FieldDescriptorProto fdp;
  //  descriptor_->CopyTo(&fdp);
  //  UnknownFieldSet *unks = fdp.mutable_unknown_fields();

  if (!unks->empty()) {
    int eoct = unks->field_count();
    RIFTDBG3("+++field descriptor '%s' has %d extension options (!empty)\n",
            descriptor_->full_name().c_str(),
            eoct);
    for (int i=0; i<eoct; i++) {
      UnknownField *unkf = unks->mutable_field(i);
      RIFTDBG3("+++  ext opt field number %d type %d\n", (int)unkf->number(), (int)unkf->type());
      switch (unkf->number()) {
      case 50003:
        if (UnknownField::TYPE_LENGTH_DELIMITED == unkf->type()) {
          RIFTDBG1("field descriptor with rift_msgoptions of type_length_delimited!!\n");
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
                  RIFTDBG2("+++ agg = %s\n", ropt->varint() ? "true" : "false");
                  if (ropt->varint()) {
                    riftopts.agg = 1;
                    riftopts.rw_flags |= RW_PROTOBUF_FOPT_AGG;
                  } else {
                    riftopts.agg = 0;
                    riftopts.rw_flags &= ~RW_PROTOBUF_FOPT_AGG;
                  }
                } else {
                  RIFTDBG3("+++ unknown data type for RiftFieldOptions option %d for field '%s'\n", ropt->number(), descriptor_->full_name().c_str());
                }
                break;

              case 2:
                if (ropt->type() == UnknownField::TYPE_VARINT) {
                  RIFTDBG2("+++ inline = %s\n", ropt->varint() ? "true" : "false");
                  if (ropt->varint()) {
                    riftopts.flatinline = 1;
                    riftopts.rw_flags |= RW_PROTOBUF_FOPT_INLINE;
                  } else {
                    riftopts.flatinline = 0;
                    riftopts.rw_flags &= ~RW_PROTOBUF_FOPT_INLINE;
                  }
                } else {
                  RIFTDBG3("+++ unknown data type for RiftFieldOptions option %d for field '%d'\n", ropt->number(), descriptor_->full_name().c_str());
                }
                break;

              case 3:
                if (ropt->type() == UnknownField::TYPE_VARINT) {
                  RIFTDBG2("+++ inline_max = %u\n", (unsigned)ropt->varint());
                  riftopts.inline_max = (uint32_t)ropt->varint();
                } else {
                  fprintf(stderr, "+++ unknown data type for RiftFieldOptions option %d for field '%s'\n", ropt->number(), descriptor_->full_name().c_str());
                }
                break;

              case 4:
                if (ropt->type() == UnknownField::TYPE_VARINT) {
                  RIFTDBG2("+++ string_max = %u\n", (unsigned)ropt->varint());
                  riftopts.string_max = (uint32_t)ropt->varint();
                } else {
                  fprintf(stderr, "+++ unknown data type for RiftFieldOptions option %d for field '%s'\n", ropt->number(), descriptor_->full_name().c_str());
                }

                break;

              case 5:
                if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                  RIFTDBG2("+++ c_type = %s\n", (ropt->length_delimited().c_str()));
                  riftopts.rw_flags |= RW_PROTOBUF_FOPT_RW_CTYPE;
                  riftopts.c_type = ropt->length_delimited();
                } else {
                  fprintf(stderr, "+++ unknown data type for RiftFieldOptions option %d for field '%s'\n", ropt->number(), descriptor_->full_name().c_str());
                }
                break;

              case 6:
                if (ropt->type() == UnknownField::TYPE_VARINT) {
                  RIFTDBG2("+++ key = %s\n", ropt->varint() ? "true" : "false");
                  if (ropt->varint()) {
                    riftopts.rw_flags |= RW_PROTOBUF_FOPT_KEY;
                  } else {
                    riftopts.rw_flags &= ~RW_PROTOBUF_FOPT_KEY;
                  }
                } else {
                  RIFTDBG3("+++ unknown data type for RiftFieldOptions option %d for field '%d'\n", ropt->number(), descriptor_->full_name().c_str());
                }
                break;

              case 7:
                if (ropt->type() == UnknownField::TYPE_VARINT) {
                  RIFTDBG2("+++ key = %s\n", ropt->varint() ? "true" : "false");
                  if (ropt->varint()) {
                    riftopts.rw_flags |= RW_PROTOBUF_LOGEV_KEY;
                  } else {
                    riftopts.rw_flags &= ~RW_PROTOBUF_LOGEV_KEY;
                  }
                } else {
                  RIFTDBG3("+++ unknown data type for RiftFieldOptions option %d for field '%d'\n", ropt->number(), descriptor_->full_name().c_str());
                }
              break;

              case 9:
                if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    RIFTDBG2("+++ merge_behavior = %s\n", (ropt->length_delimited().c_str()));
                    riftopts.merge_behave = ropt->length_delimited();

                    if (!strcmp(riftopts.merge_behave.c_str(), "none")) {
                        riftopts.rw_flags |= RW_PROTOBUF_FOPT_MERGE_BEHAVIOR;
                    } else {
                        riftopts.rw_flags &= ~RW_PROTOBUF_FOPT_MERGE_BEHAVIOR;
                    }
                } else {
                  RIFTDBG3("+++ unknown data type for RiftFieldOptions option %d for field '%d'\n", ropt->number(), descriptor_->full_name().c_str());
                }
                break;

              default:
                fprintf(stderr, "+++ protoc-c: unknown option field in RiftFieldOptions for field '%s'\n", descriptor_->full_name().c_str());
                break;
              }
            }
          }
        }
      }
    }
  } else {
    RIFTDBG2("+++ field descriptor '%s' has 0 extension options\n",
             descriptor_->full_name().c_str());
  }
}

FieldGenerator::~FieldGenerator()
{
}
static bool is_packable_type(FieldDescriptor::Type type)
{
  return type == FieldDescriptor::TYPE_DOUBLE
      || type == FieldDescriptor::TYPE_FLOAT
      || type == FieldDescriptor::TYPE_INT64
      || type == FieldDescriptor::TYPE_UINT64
      || type == FieldDescriptor::TYPE_INT32
      || type == FieldDescriptor::TYPE_FIXED64
      || type == FieldDescriptor::TYPE_FIXED32
      || type == FieldDescriptor::TYPE_BOOL
      || type == FieldDescriptor::TYPE_UINT32
      || type == FieldDescriptor::TYPE_ENUM
      || type == FieldDescriptor::TYPE_SFIXED32
      || type == FieldDescriptor::TYPE_SFIXED64
      || type == FieldDescriptor::TYPE_SINT32
      || type == FieldDescriptor::TYPE_SINT64;
    //TYPE_BYTES
    //TYPE_STRING
    //TYPE_GROUP
    //TYPE_MESSAGE
}

void FieldGenerator::GenerateDescriptorInitializerGeneric(io::Printer* printer,
                                                          bool optional_uses_has,
                                                          const string &type_macro,
                                                          const string &descriptor_addr) const
{
  map<string, string> variables;
  variables["LABEL"] = CamelToUpper(GetLabelName(descriptor_->label()));
  variables["TYPE"] = type_macro;
  variables["classname"] = FullNameToC(FieldScope(descriptor_)->full_name());
  variables["name"] = FieldName(descriptor_);
  variables["proto_name"] = descriptor_->name();
  variables["descriptor_addr"] = descriptor_addr;
  variables["value"] = SimpleItoa(descriptor_->number());
  const OneofDescriptor *oneof = descriptor_->containing_oneof();
  if (oneof != NULL)
    variables["oneofname"] = FullNameToLower(oneof->name());

  variables["rw_flags"] = SimpleItoa(riftopts.rw_flags);
  variables["rw_inline_max"] = SimpleItoa(riftopts.inline_max);
  variables["rw_c_type"] = std::string(riftopts.c_type);

  if (riftopts.c_type[0]) {
    optional_uses_has = riftopts.flatinline;
    variables["ctype_helper"] = string("&") + riftopts.c_type + "_helper";
    variables["data_size"] = string("sizeof(") + riftopts.c_type + ")";
  } else  {
    variables["ctype_helper"] = string("NULL");

    switch (descriptor_->type()) {
      case FieldDescriptor::TYPE_MESSAGE:
        if (riftopts.flatinline) {
          if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + "," + variables["name"] + "[0])";
          } else {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + "," + variables["name"] + ")";
          }
        } else {
          if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + "," + variables["name"] + "[0][0])";
          } else {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + "," + variables["name"] + "[0])";
          }
        }
        break;
      case FieldDescriptor::TYPE_BYTES:
        if (riftopts.flatinline) {
          if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + "," + variables["name"] + "[0])";
          } else {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + ",_placeholder_" + variables["name"] + ")";
          }
          break;
        }
        variables["data_size"] = string("0");
        break;
      case FieldDescriptor::TYPE_STRING:
        if (riftopts.flatinline) {
          if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + "," + variables["name"] + "[0])";
          } else {
            variables["data_size"]
              = string("PROTOBUF_C_SIZEOF(") + variables["classname"] + "," + variables["name"] + ")";
          }
          break;
        }
        variables["data_size"] = string("0");
        break;
      default:
        variables["data_size"] = string("0");
        break;
    }
  }

  if (descriptor_->has_default_value() && !riftopts.c_type[0]) {
    variables["default_value"] = string("&")
                               + FullNameToLower(descriptor_->full_name())
                               + "__default_value";
  } else {
    variables["default_value"] = "NULL";
  }

  variables["flags"] = "0";

  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED
   && is_packable_type (descriptor_->type())
   && descriptor_->options().packed())
    variables["flags"] += " | PROTOBUF_C_FIELD_FLAG_PACKED";

  if (descriptor_->options().deprecated())
    variables["flags"] += " | PROTOBUF_C_FIELD_FLAG_DEPRECATED";

  if (oneof != NULL)
    variables["flags"] += " | PROTOBUF_C_FIELD_FLAG_ONEOF";

  printer->Print(variables,
    "{\n"
    "  \"$proto_name$\",\n"
    "  \"$name$\",\n"
    "  $value$,\n"
    "  PROTOBUF_C_LABEL_$LABEL$,\n"
    "  PROTOBUF_C_TYPE_$TYPE$,\n");
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
      printer->Print(variables, "  0,   /* quantifier_offset */\n");
      break;
    case FieldDescriptor::LABEL_OPTIONAL:
      if (oneof != NULL) {
        printer->Print(variables, "  PROTOBUF_C_OFFSETOF($classname$, $oneofname$_case),\n");
      } else if (optional_uses_has) {
	printer->Print(variables, "  PROTOBUF_C_OFFSETOF($classname$, has_$name$),\n");
      } else {
        printer->Print(variables, "  0, /* quantifier_offset */\n");
      }
      break;
    case FieldDescriptor::LABEL_REPEATED:
      printer->Print(variables, "  PROTOBUF_C_OFFSETOF($classname$, n_$name$),\n");
      break;
  }
  printer->Print(variables, "  PROTOBUF_C_OFFSETOF($classname$, $name$),\n");
  printer->Print(variables, "  { $descriptor_addr$, },\n");
  printer->Print(variables, "  $default_value$,\n");
  printer->Print(variables, "  $flags$, /* flags */\n");
  printer->Print(variables, "  0,NULL,NULL, /* reserved1,reserved2, etc */\n");
  printer->Print(variables, "  $rw_flags$, $rw_inline_max$, /* rift flags, inline max */\n");
  printer->Print(variables, "  $data_size$, /* data_size */\n");
  printer->Print(variables, "  $ctype_helper$ /* ctype */\n");
  printer->Print("},\n");
}

bool FieldGenerator::isCType(void) const
{
  if (riftopts.c_type[0])  {
   return true;
  } else {
   return false;
  }
}

void
FieldGenerator::GenerateMetaDataMacro(io::Printer* printer, unsigned index) const
{
  std::map<string, string> vars;
  vars["classname"] = FullNameToC(FieldScope(descriptor_)->full_name());
  vars["uclassname"] = FullNameToUpper(FieldScope(descriptor_)->full_name());
  vars["lclassname"] = FullNameToLower(FieldScope(descriptor_)->full_name());
  vars["name"] = FieldName(descriptor_);
  vars["type"] = GetTypeName();
  vars["label"] = GetLabelName(descriptor_->label());
  vars["index"] = SimpleItoa(index);
  vars["ptrtype"] = GetPointerType();
  vars["string_max"] = SimpleItoa(riftopts.string_max);
  vars["inline_max"] = SimpleItoa(riftopts.inline_max);

  string type = vars.find("type")->second;
  bool has_quant_field = false;
  bool has_primitive_type_field = false;

  if(type == "primitive") {
    switch (descriptor_->type()) {
      case FieldDescriptor::TYPE_SINT32  :
      case FieldDescriptor::TYPE_SFIXED32:
      case FieldDescriptor::TYPE_INT32   :
        has_primitive_type_field = true;
        vars["primitive_type"] = "int32_t";
        break;
      case FieldDescriptor::TYPE_SINT64  :
      case FieldDescriptor::TYPE_SFIXED64:
      case FieldDescriptor::TYPE_INT64   :
        has_primitive_type_field = true;
        vars["primitive_type"] = "int64_t";
        break;
      case FieldDescriptor::TYPE_UINT32  :
      case FieldDescriptor::TYPE_FIXED32 :
        has_primitive_type_field = true;
        vars["primitive_type"] = "uint32_t";
        break;
      case FieldDescriptor::TYPE_UINT64  :
      case FieldDescriptor::TYPE_FIXED64 :
        has_primitive_type_field = true;
        vars["primitive_type"] = "uint64_t";
        break;
      case FieldDescriptor::TYPE_FLOAT   :
        has_primitive_type_field = true;
        vars["primitive_type"] = "float";
        break;
      case FieldDescriptor::TYPE_DOUBLE  :
        has_primitive_type_field = true;
        vars["primitive_type"] = "double";
        break;
      case FieldDescriptor::TYPE_BOOL    :
        has_primitive_type_field = true;
        vars["primitive_type"] = "protobuf_c_boolean";
        break;
      case FieldDescriptor::TYPE_ENUM    :
      case FieldDescriptor::TYPE_STRING  :
      case FieldDescriptor::TYPE_BYTES   :
      case FieldDescriptor::TYPE_GROUP   :
      case FieldDescriptor::TYPE_MESSAGE : GOOGLE_LOG(FATAL) << "not a primitive type"; break;
    }
  }

  if ((type == "primitive") || (type == "enum")) {
    vars["style"] = "flat";
    if ((descriptor_->label() == FieldDescriptor::LABEL_REPEATED) &&
        !riftopts.flatinline) {
      vars["style"] = "pointy";
    }

    if (descriptor_->label() != FieldDescriptor::LABEL_REQUIRED) {
      has_quant_field = true;
    }
  } else {
    /* String, bytes, ctype and messages. */
    if (riftopts.flatinline) {
      if (descriptor_->label() != FieldDescriptor::LABEL_REQUIRED) {
        has_quant_field = true;
      }
      vars["style"] = "flat";
    } else {
      if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
        has_quant_field = true;
      }
      vars["style"] = "pointy";
    }
  }

  if (type == "message") {
    vars["fclassname"] = FullNameToC(descriptor_->message_type()->full_name());
  }

  printer->Print(vars, "#define $uclassname$__field_$name$() \\\n");
  printer->Indent();
  printer->Print(vars, "$classname$, \\\n");
  printer->Print(vars, "\"$name$\", \\\n");
  printer->Print(vars, "$name$, \\\n");
  printer->Print(vars, "$label$, \\\n");
  printer->Print(vars, "$type$, \\\n");
  printer->Print(vars, "$style$, \\\n");

  if (has_quant_field) {
    if (descriptor_->label() == FieldDescriptor::LABEL_OPTIONAL) {
      printer->Print(vars, "has_$name$, \\\n");
    } else {
      printer->Print(vars, "n_$name$, \\\n");
    }
  } else {
    printer->Print("void/*no quant field*/, \\\n");
  }

  printer->Print(vars, "&$lclassname$__field_descriptors[$index$], \\\n");
  printer->Print(vars, "$ptrtype$, \\\n");
  printer->Print(vars, "$string_max$, \\\n");
  printer->Print(vars, "$inline_max$, \\\n");
  if (has_primitive_type_field) {
      printer->Print(vars, "$primitive_type$, \\\n");
  }
  else {
    printer->Print("void,/*no primitive type field*/ \\\n");
  }

  // Output the default value for primitive, enum and string types.
  if (descriptor_->has_default_value()) {
    if (type == "primitive" || type == "enum") {
      vars["default_value"] = GetDefaultValue();
      printer->Print(vars, "$default_value$ \n");
    } else if (type == "string" || type == "bytes") {
      vars["default_value"] = "\"" + CEscape(descriptor_->default_value_string()) + "\"";
      printer->Print(vars, "$default_value$ \n");
    } else {
      printer->Print("void/* no default value*/ \n");
    }
  } else {
    printer->Print("void/* no default value*/ \n");
  }

  printer->Outdent();
  printer->Print("\n\n");
}

/*
 * Gi code generation.
 */
string FieldGenerator::GetParentGiCIdentifier(const char *operation) const
{
  return FullNameToGICBase(FieldScope(descriptor_)->full_name()) + "_" + operation;
}

string FieldGenerator::GetParentGiCIdentifier(const char *operation, const string &field) const
{
  return FullNameToGICBase(FieldScope(descriptor_)->full_name()) + "_" + operation + "_" + field;
}

string FieldGenerator::GetGiCIdentifier(const char *operation) const
{
  if (descriptor_->message_type()) {
    return FullNameToGICBase(descriptor_->message_type()->full_name()) + "_" + operation;
  }
  return "";
}

void FieldGenerator::GenerateGiHSupportMethodDecls(io::Printer* printer) const
{
  if (   descriptor_->type() == FieldDescriptor::TYPE_BYTES
      && isCType()) {
    // ATTN: RIFT-5608
    return;
  }
  if (   descriptor_->label() == FieldDescriptor::LABEL_REPEATED
      && descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    // ATTN: RIFT-5593
    return;
  }

  std::map<string, string> vars;
  string getter_func = GetParentGiCIdentifier("get", FieldName(descriptor_));
  if (FieldName(descriptor_) == "type") {
    getter_func.append("_yang"); // Disambiguate with an _yang
  }
  vars["getter_fn"] = getter_func;
  string setter_func = GetParentGiCIdentifier("set", FieldName(descriptor_));
  if (FieldName(descriptor_) == "type") {
    setter_func.append("_yang"); // Disambiguate with an _yang
  }
  vars["setter_fn"] = setter_func;
  vars["create_fn"] = GetParentGiCIdentifier("create", FieldName(descriptor_));
  vars["boxed_parent"] = FullNameToGI(FieldScope(descriptor_));
  vars["field_name"] = FieldName(descriptor_);
  vars["gi_tname"] = GetGiTypeName();
  vars["gi_type_ptr"] = FullNameToGI(FieldScope(descriptor_)) + "*";

  // Print the getter Comment block
  printer->Print("\n");
  printer->Print(vars, "/**\n"
                       " * $getter_fn$:\n"
                       " * @boxed:\n");
  if (HasLengthOut()) {
    printer->Print(" * @len:(out)\n");
  }
  printer->Print(" *\n"
                 " * Returns: $annotations$\n"
                 " **/\n", "annotations", GetGiReturnAnnotations());

  // Print the getter proto
  vars["getter_rt"] = GetGiGetterReturnType();
  printer->Print(vars, "$getter_rt$ $getter_fn$");
  printer->Print(GetGiGetterParameterList().c_str());
  printer->Print(";\n");

  // Print the setter Comment block
  printer->Print("\n");

  vars["annotations"] = GetGiSetterAnnotations();
  printer->Print(vars, "/**\n"
                       " * $setter_fn$: \n"
                       " * @boxed:\n"
                       " * @$field_name$: $annotations$\n"
                       " * @err:\n"
                       " **/\n");

  // Print the setter proto
  printer->Print(vars, "void $setter_fn$");
  printer->Print(GetGiSetterParameterList().c_str());
  printer->Print(";\n");

  // Print the create Comment block
  if (descriptor_->type() == FieldDescriptor::TYPE_MESSAGE) {
    printer->Print("\n");
    printer->Print(vars, "/**\n"
                         " * $create_fn$: \n"
                         " * @boxed:\n"
                         " * @err:\n"
                         " * Returns: (transfer full)\n"
                         " **/\n");

    // Print the create proto
    printer->Print(vars, "$gi_tname$ $create_fn$");
    printer->Print(GetGiCreateParameterList().c_str());
    printer->Print(";\n");
  }
}

std::string FieldGenerator::GetGiBoxedFieldName(void) const
{
  return std::string("boxed_") + FieldName(descriptor_);
}

bool FieldGenerator::HasLengthOut() const
{
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return true;
  }
  return false;
}

bool FieldGenerator::HasQuantifierField() const
{
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return true;
  }

  if (descriptor_->label() == FieldDescriptor::LABEL_OPTIONAL) {
    if (riftopts.flatinline) {
      return true;
    }
    if (isCType()) {
      return false;
    }
    switch (descriptor_->type()) {
      case FieldDescriptor::TYPE_MESSAGE:
      case FieldDescriptor::TYPE_STRING:
        break;
      default:
        return true;
    }
  }
  return false;
}

string FieldGenerator::GetQuantifierField() const
{
  if (descriptor_->label() == FieldDescriptor::LABEL_REPEATED) {
    return string("n_") + FieldName(descriptor_);
  }

  if (HasQuantifierField()) {
    return string("has_") + FieldName(descriptor_);
  }

  return "";
}

void FieldGenerator::GenerateGiCSupportMethodDefs(io::Printer* printer) const
{
  GenerateGiCGetterMethod(printer);
  GenerateGiCSetterMethod(printer);
  if (descriptor_->message_type()) {
    GenerateGiCreateMethod(printer);
  }
}

FieldGeneratorMap::FieldGeneratorMap(const Descriptor* descriptor)
  : descriptor_(descriptor),
    field_generators_(
      new scoped_ptr<FieldGenerator>[descriptor->field_count()]) {
  // Construct all the FieldGenerators.
  for (int i = 0; i < descriptor->field_count(); i++) {
    field_generators_[i].reset(MakeGenerator(descriptor->field(i)));
  }
}

FieldGenerator* FieldGeneratorMap::MakeGenerator(const FieldDescriptor* field) {
  switch (field->type()) {
    case FieldDescriptor::TYPE_MESSAGE:
      return new MessageFieldGenerator(field);
    case FieldDescriptor::TYPE_STRING:
      return new StringFieldGenerator(field);
    case FieldDescriptor::TYPE_BYTES:
      return new BytesFieldGenerator(field);
    case FieldDescriptor::TYPE_ENUM:
      return new EnumFieldGenerator(field);
    case FieldDescriptor::TYPE_GROUP:
      return 0;                        // XXX
    default:
      return new PrimitiveFieldGenerator(field);
  }
}

FieldGeneratorMap::~FieldGeneratorMap() {}

const FieldGenerator& FieldGeneratorMap::get(
    const FieldDescriptor* field) const {
  GOOGLE_CHECK_EQ(field->containing_type(), descriptor_);
  return *field_generators_[field->index()];
}



}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
