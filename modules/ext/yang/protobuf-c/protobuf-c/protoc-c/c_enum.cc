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

#include <set>
#include <map>

#include <protoc-c/c_enum.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>

#if 0
#define RIFTDBG(arg1) fprintf(stderr, arg1...)
#else
#define RIFTDBG(arg1...)
#endif


namespace google {
namespace protobuf {
namespace compiler {
namespace c {

void get_enum_riftopts(const EnumDescriptor* descriptor, rw_eopts_t *rw_eopts) {
  EnumOptions mopts(descriptor->options());
  UnknownFieldSet *unks = mopts.mutable_unknown_fields();

  if (!unks->empty()) {
    RIFTDBG("+++ enum descriptor '%s' has %d extension options\n", 
             descriptor->full_name().c_str(), unks->field_count());

    for (int i=0; i<unks->field_count(); i++) {
      UnknownField *unkf = unks->mutable_field(i);
      RIFTDBG("+++ ext opt field number %d type %d\n", (int)unkf->number(), (int)unkf->type());
      switch (unkf->number()) {
      case 50004:
        if (UnknownField::TYPE_LENGTH_DELIMITED == unkf->type()) {
          RIFTDBG("+++ enum descriptor with rw_enumopts of type_length_delimited!!\n");
          RIFTDBG("  buf len=%d: ", unkf->length_delimited().length());
          for (int k=0; k<unkf->length_delimited().length(); k++) {
            RIFTDBG("%0X ", (uint32_t) *(const char*)unkf->length_delimited().substr(k, 1).c_str());
          }
          RIFTDBG("\n");
          UnknownFieldSet ropts;
          if (ropts.ParseFromString(unkf->length_delimited())) {
            for (int j=0; j<ropts.field_count(); j++) {
              UnknownField *ropt = ropts.mutable_field(j);
              switch (ropt->number()) {
                case 1:
                  if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    rw_eopts->rw_c_prefix = ropt->length_delimited();
                  } else {
                    RIFTDBG("+++ enum '%s' has incorrect data type for RwEnumOptions.c_prefix %d: %d\n",
                             descriptor->full_name().c_str(), ropt->number(), (int)ropt->type());
                  }
                  break;

                case 2:
                  if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                    rw_eopts->rw_yang_enum = ropt->length_delimited();
                  } else {
                    RIFTDBG("+++ enum '%s' has incorrect data type for RwEnumOptions.ypbc_enum option %d: %d\n",
                             descriptor->full_name().c_str(), ropt->number(), (int)ropt->type());
                  }
                  break;

                default:
                  fprintf(stderr, "+++ enum '%s' has unknown RwEnumOptions option %d\n", 
                          descriptor->full_name().c_str(), ropt->number());
                  break;
              }
            }
          }
        } else {
          fprintf(stderr, "+++ enum '%s' has unknown type %d for field %d\n", 
                  descriptor->full_name().c_str(), 
                  (int)unkf->type(), (int)unkf->number());
        }
        break;

      default:
        fprintf(stderr, "+++ enum '%s' has unknown message option field %d\n", 
                descriptor->full_name().c_str(), 
                unkf->number());
        break;
      }
    }
  }
}

EnumGenerator::EnumGenerator(const EnumDescriptor* descriptor,
                             const string& dllexport_decl)
  : descriptor_(descriptor),
    dllexport_decl_(dllexport_decl) {
  get_enum_riftopts(descriptor_, &rw_eopts_);
}

EnumGenerator::~EnumGenerator() {}

void EnumGenerator::GenerateDefinition(io::Printer* printer) {
  map<string, string> vars;
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["shortname"] = descriptor_->name();
  vars["uc_name"] = FullNameToUpper(descriptor_->full_name());

  SourceLocation sourceLoc;
  descriptor_->GetSourceLocation(&sourceLoc);
  PrintComment (printer, sourceLoc.leading_comments);

  printer->Print(vars, "typedef enum $classname$ {\n");
  printer->Indent();

  const EnumValueDescriptor* min_value = descriptor_->value(0);
  const EnumValueDescriptor* max_value = descriptor_->value(0);

  vars["opt_comma"] = ",";

  if (rw_eopts_.rw_c_prefix.length()) {
    vars["prefix"] = rw_eopts_.rw_c_prefix + "_";
  } else if (descriptor_->containing_type() == NULL) {
    vars["prefix"] = "";
  } else {
    vars["prefix"] = FullNameToUpper(descriptor_->full_name()) + "__";
  }
  for (int i = 0; i < descriptor_->value_count(); i++) {
    if (rw_eopts_.rw_c_prefix.length()) {
      vars["name"] = ToUpper(descriptor_->value(i)->name());
    } else {
      vars["name"] = descriptor_->value(i)->name();
    }
    vars["number"] = SimpleItoa(descriptor_->value(i)->number());
    if (i + 1 == descriptor_->value_count())
      vars["opt_comma"] = "";

    SourceLocation valSourceLoc;
    descriptor_->value(i)->GetSourceLocation(&valSourceLoc);

    PrintComment (printer, valSourceLoc.leading_comments);
    PrintComment (printer, valSourceLoc.trailing_comments);
    printer->Print(vars, "$prefix$$name$ = $number$$opt_comma$\n");

    if (descriptor_->value(i)->number() < min_value->number()) {
      min_value = descriptor_->value(i);
    }
    if (descriptor_->value(i)->number() > max_value->number()) {
      max_value = descriptor_->value(i);
    }
  }

  // hide macros from gir parser
  printer->Print("#ifndef __GI_SCANNER__\n");

  if (descriptor_->containing_type() != NULL) {
    vars["max_value"] = SimpleItoa(1+max_value->number());
    printer->Print(vars, "  _PROTOBUF_C_MAX_ENUM($prefix$, $max_value$)\n");
  }
  printer->Print(vars, "  _PROTOBUF_C_FORCE_ENUM_TO_BE_INT_SIZE($uc_name$)\n");
  printer->Print("#endif // __GI_SCANNER__\n");
  printer->Outdent();
  printer->Print(vars, "} $classname$;\n");
}

void EnumGenerator::GenerateDescriptorDeclarations(io::Printer* printer) {
  map<string, string> vars;
  if (dllexport_decl_.empty()) {
    vars["dllexport"] = "";
  } else {
    vars["dllexport"] = dllexport_decl_ + " ";
  }
  vars["classname"] = FullNameToC(descriptor_->full_name());
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());

  printer->Print(vars,
    "extern $dllexport$const ProtobufCEnumDescriptor    $lcclassname$__descriptor;\n");
}

struct ValueIndex
{
  int value;
  unsigned index;
  unsigned final_index;		/* index in uniqified array of values */
  const char *name;
};
void EnumGenerator::GenerateValueInitializer(io::Printer *printer, int index)
{
  const EnumValueDescriptor *vd = descriptor_->value(index);
  map<string, string> vars;
  vars["enum_value_name"] = vd->name();

  if (rw_eopts_.rw_c_prefix.length()) {
    vars["c_enum_value_name"] = rw_eopts_.rw_c_prefix + "_" + ToUpper(vd->name());
  } else if (descriptor_->containing_type() == NULL) {
    vars["c_enum_value_name"] = vd->name();
  } else {
    vars["c_enum_value_name"] = FullNameToUpper(descriptor_->full_name()) + "__" + vd->name();
  }

  vars["value"] = SimpleItoa(vd->number());
  printer->Print(vars,
   "  { \"$enum_value_name$\", \"$c_enum_value_name$\", $value$ },\n");
}

static int compare_value_indices_by_value_then_index(const void *a, const void *b)
{
  const ValueIndex *vi_a = (const ValueIndex *) a;
  const ValueIndex *vi_b = (const ValueIndex *) b;
  if (vi_a->value < vi_b->value) return -1;
  if (vi_a->value > vi_b->value) return +1;
  if (vi_a->index < vi_b->index) return -1;
  if (vi_a->index > vi_b->index) return +1;
  return 0;
}

static int compare_value_indices_by_name(const void *a, const void *b)
{
  const ValueIndex *vi_a = (const ValueIndex *) a;
  const ValueIndex *vi_b = (const ValueIndex *) b;
  return strcmp (vi_a->name, vi_b->name);
}

void EnumGenerator::GenerateEnumDescriptor(io::Printer* printer) {
  map<string, string> vars;
  vars["fullname"] = descriptor_->full_name();
  vars["lcclassname"] = FullNameToLower(descriptor_->full_name());
  vars["cname"] = FullNameToC(descriptor_->full_name());
  vars["shortname"] = descriptor_->name();
  vars["packagename"] = descriptor_->file()->package();
  vars["value_count"] = SimpleItoa(descriptor_->value_count());

  // Sort by name and value, dropping duplicate values if they appear later.
  // TODO: use a c++ paradigm for this!
  NameIndex *name_index = new NameIndex[descriptor_->value_count()];
  ValueIndex *value_index = new ValueIndex[descriptor_->value_count()];
  for (int j = 0; j < descriptor_->value_count(); j++) {
    const EnumValueDescriptor *vd = descriptor_->value(j);
    name_index[j].index = j;
    name_index[j].name = vd->name().c_str();
    value_index[j].index = j;
    value_index[j].value = vd->number();
    value_index[j].name = vd->name().c_str();
  }
  qsort(value_index, descriptor_->value_count(),
	sizeof(ValueIndex), compare_value_indices_by_value_then_index);

  // only record unique values
  int n_unique_values;
  if (descriptor_->value_count() == 0) {
    n_unique_values = 0; // should never happen
  } else {
    n_unique_values = 1;
    value_index[0].final_index = 0;
    for (int j = 1; j < descriptor_->value_count(); j++) {
      if (value_index[j-1].value != value_index[j].value)
	value_index[j].final_index = n_unique_values++;
      else
	value_index[j].final_index = n_unique_values - 1;
    }
  }

  vars["unique_value_count"] = SimpleItoa(n_unique_values);
  printer->Print(vars,
    "const ProtobufCEnumValue $lcclassname$__enum_values_by_number[$unique_value_count$] =\n"
    "{\n");
  if (descriptor_->value_count() > 0) {
    GenerateValueInitializer(printer, value_index[0].index);
    for (int j = 1; j < descriptor_->value_count(); j++) {
      if (value_index[j-1].value != value_index[j].value) {
	GenerateValueInitializer(printer, value_index[j].index);
      }
    }
  }
  printer->Print(vars, "};\n");
  printer->Print(vars, "static const ProtobufCIntRange $lcclassname$__value_ranges[] = {\n");
  unsigned n_ranges = 0;
  if (descriptor_->value_count() > 0) {
    unsigned range_start = 0;
    unsigned range_len = 1;
    int range_start_value = value_index[0].value;
    int last_value = range_start_value;
    for (int j = 1; j < descriptor_->value_count(); j++) {
      if (value_index[j-1].value != value_index[j].value) {
	if (last_value + 1 == value_index[j].value) {
	  range_len++;
	} else {
	  // output range
	  vars["range_start_value"] = SimpleItoa(range_start_value);
	  vars["orig_index"] = SimpleItoa(range_start);
	  printer->Print (vars, "{$range_start_value$, $orig_index$},");
	  range_start_value = value_index[j].value;
	  range_start += range_len;
	  range_len = 1;
	  n_ranges++;
	}
	last_value = value_index[j].value;
      }
    }
    {
    vars["range_start_value"] = SimpleItoa(range_start_value);
    vars["orig_index"] = SimpleItoa(range_start);
    printer->Print (vars, "{$range_start_value$, $orig_index$},");
    range_start += range_len;
    n_ranges++;
    }
    {
    vars["range_start_value"] = SimpleItoa(0);
    vars["orig_index"] = SimpleItoa(range_start);
    printer->Print (vars, "{$range_start_value$, $orig_index$}\n};\n");
    }
  }
  vars["n_ranges"] = SimpleItoa(n_ranges);

  qsort(value_index, descriptor_->value_count(),
        sizeof(ValueIndex), compare_value_indices_by_name);
  printer->Print(vars,
    "const ProtobufCEnumValueIndex $lcclassname$__enum_values_by_name[$value_count$] =\n"
    "{\n");
  for (int j = 0; j < descriptor_->value_count(); j++) {
    vars["index"] = SimpleItoa(value_index[j].final_index);
    vars["name"] = value_index[j].name;
    printer->Print (vars, "  { \"$name$\", $index$ },\n");
  }
  printer->Print(vars, "};\n");

  if (rw_eopts_.rw_yang_enum.length()) {
    vars["helper"] = std::string("(const struct rw_yang_pb_enumdesc_t*)(&") + rw_eopts_.rw_yang_enum + ")";
  } else {
    vars["helper"] = "NULL";
  }

  printer->Print(vars,
    "const ProtobufCEnumDescriptor $lcclassname$__descriptor =\n"
    "{\n"
    "  PROTOBUF_C_ENUM_DESCRIPTOR_MAGIC,\n"
    "  \"$fullname$\",\n"
    "  \"$shortname$\",\n"
    "  \"$cname$\",\n"
    "  \"$packagename$\",\n"
    "  $unique_value_count$,\n"
    "  $lcclassname$__enum_values_by_number,\n"
    "  $value_count$,\n"
    "  $lcclassname$__enum_values_by_name,\n"
    "  $n_ranges$,\n"
    "  $lcclassname$__value_ranges,\n"
    "  NULL,NULL,NULL,NULL, /* reserved[1234] */\n"
    "  $helper$,\n"
    "};\n");

  delete[] value_index;
  delete[] name_index;
}

string EnumGenerator::MakeGiName(const char *discriminator)
{
  return FullNameToGICBase(descriptor_->full_name()) + "_" + discriminator;
}

void EnumGenerator::GenerateGiHEnums(io::Printer* printer)
{
  string to_str_f = MakeGiName("to_str");
  string from_str_f = MakeGiName("from_str");

  printer->Print("const char* $to_str_f$(gint enum_value) G_GNUC_UNUSED;\n", "to_str_f", to_str_f);
  printer->Print("gint $from_str_f$(const char *enum_str, GError **error) G_GNUC_UNUSED;\n", "from_str_f", from_str_f);
}

void EnumGenerator::GenerateGiCEnumDefs(io::Printer* printer)
{
  std::map<string, string> vars;

  vars["to_str_f"] = MakeGiName("to_str");
  vars["from_str_f"] = MakeGiName("from_str");
  if (descriptor_->file()->package().length()) {
    vars["domain"] = MangleNameToUpper(descriptor_->file()->package());
  } else {
    vars["domain"] = MangleNameToUpper(StripProto(descriptor_->file()->name()));
  }

  printer->Print(vars, "const char* $to_str_f$(gint enum_value) \n"
                       "{ \n");
  printer->Indent();
  for (int j = 0; j < descriptor_->value_count(); j++) {
    const EnumValueDescriptor *vd = descriptor_->value(j);
    vars["value"] = SimpleItoa(vd->number());
    vars["name"] = vd->name();
                            
    printer->Print(vars, "if (enum_value == $value$) return \"$name$\"; \n");
  }
  printer->Print("return \"\";\n");
  printer->Outdent();
  printer->Print("}\n\n");

  printer->Print(vars, "gint $from_str_f$(const char *enum_str, GError **error)\n"
                       "{ \n");

  printer->Indent();
  for (int j = 0; j < descriptor_->value_count(); j++) {
    const EnumValueDescriptor *vd = descriptor_->value(j);
    vars["value"] = SimpleItoa(vd->number());
    vars["name"] = vd->name();
    printer->Print(vars, "if (!strcmp(enum_str, \"$name$\")) return $value$; \n");
  }
  printer->Print(vars, "PROTOBUF_C_GI_RAISE_EXCEPTION(error, $domain$, PROTO_GI_ERROR_INVALID_ENUM, "
                       "\"Invalid Enum string passed %s\\n\", enum_str ); \n"
                       "return -1; \n");
  printer->Outdent();
  printer->Print("}\n\n");
}



}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
