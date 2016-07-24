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

// Copyright (c) 2008-2014, Dave Benson and the protobuf-c authors.
// All rights reserved.
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
#include <protoc-c/c_file.h>
#include <protoc-c/c_enum.h>
#include <protoc-c/c_service.h>
#include <protoc-c/c_extension.h>
#include <protoc-c/c_helpers.h>
#include <protoc-c/c_message.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>

#include "rift-protobuf-c.h"

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
static void fdesc_to_rift_fileopts(const FileDescriptor* descriptor_,
			                             rift_fileopts *rw_fileopts)
{
  FileOptions fopts(descriptor_->options());
  UnknownFieldSet *unks = fopts.mutable_unknown_fields();

  rw_fileopts->c_includes.clear();
  rw_fileopts->generate_gi = false;

  if (!unks->empty()) {

    RIFTDBG3("+++ file descriptor '%s' has %d extension options\n",
             descriptor_->name().c_str(), unks->field_count());

    for (int i = 0; i < unks->field_count(); i++) {

      UnknownField *unkf = unks->mutable_field(i);
      RIFTDBG3("+++ ext opt field number %d type %d\n", (int)unkf->number(), (int)unkf->type());

      switch (unkf->number()) {
        case 50005:
          {
            if (UnknownField::TYPE_LENGTH_DELIMITED == unkf->type()) {

              RIFTDBG1("+++ message descriptor with rift_msgoptions of type_length_delimited!!\n");
              RIFTDBG2("  buf len = %d: ", unkf->length_delimited().length());

              for (int k=0; k<unkf->length_delimited().length(); k++) {
                RIFTDBG2("%0X ", (uint32_t) *(const char*)unkf->length_delimited().substr(k, 1).c_str());
              }
              RIFTDBG1("\n");

              UnknownFieldSet ropts;
              if (ropts.ParseFromString(unkf->length_delimited())) {
                RIFTDBG2("+++ ldelim parsed, %d fields\n", ropts.field_count());
                for (int j = 0; j < ropts.field_count(); j++) {
                  UnknownField *ropt = ropts.mutable_field(j);
                  switch (ropt->number()) {
                    case 1:
                      if (ropt->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
                        RIFTDBG2("+++ RiftFileOptions c_include = %s\n", ropt->length_delimited().c_str());
                        rw_fileopts->c_includes.push_back(string(ropt->length_delimited().c_str()));
                      } else {
                        RIFTDBG2("+++ unknown data type for RiftFileOptions option %d\n", ropt->number());
                      }
                      break;
                    case 2:
                      if (ropt->type() == UnknownField::TYPE_VARINT) {
                        if (ropt->varint()) {
                          rw_fileopts->generate_gi = true;
                        } else {
                          rw_fileopts->generate_gi = false;
                        }
                      } else {
                        RIFTDBG2("+++ unknown data type for RiftFileOptions option %d\n", ropt->number());
                      }
                      break;
                    default:
                      fprintf(stderr, "*** File '%s' has unknown RiftFileOptions option field %d\n",
                              descriptor_->name().c_str(), ropt->number());
                      break;
                  }
                }
              }
            }
          }
          break;
        default:
          fprintf(stderr, "*** File '%s' has unknown file option field %d\n",
                  descriptor_->name().c_str(), unkf->number());
          break;
      }
    }
  }
}

FileGenerator::FileGenerator(const FileDescriptor* file,
                             const string& dllexport_decl)
  : file_(file),
    message_generators_(
      new scoped_ptr<MessageGenerator>[file->message_type_count()]),
    enum_generators_(
      new scoped_ptr<EnumGenerator>[file->enum_type_count()]),
    service_generators_(
      new scoped_ptr<ServiceGenerator>[file->service_count()]),
    extension_generators_(
      new scoped_ptr<ExtensionGenerator>[file->extension_count()]) {

  fdesc_to_rift_fileopts(file, &rw_fileopts_);
  include_ypbc_header_ = false;

  for (int i = 0; i < file->message_type_count(); i++) {
    message_generators_[i].reset(
      new MessageGenerator(file->message_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->enum_type_count(); i++) {
    enum_generators_[i].reset(
      new EnumGenerator(file->enum_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->service_count(); i++) {
    service_generators_[i].reset(
      new ServiceGenerator(file->service(i), dllexport_decl));
  }

  for (int i = 0; i < file->extension_count(); i++) {
    extension_generators_[i].reset(
      new ExtensionGenerator(file->extension(i), dllexport_decl));
  }

  SplitStringUsing(file_->package(), ".", &package_parts_);
}

FileGenerator::~FileGenerator() {}

void FileGenerator::GenerateSource(io::Printer* printer, bool const generateGI)
{
  printer->Print(
    "/* Generated by the protocol buffer compiler.  DO NOT EDIT! */\n"
    "/* Generated from: $filename$ */\n"
    "\n"
    "/* Do not generate deprecated warnings for self */\n"
    "#ifndef PROTOBUF_C_NO_DEPRECATED\n"
    "#define PROTOBUF_C_NO_DEPRECATED\n"
    "#endif\n"
    "\n"
    "#include \"$basename$.pb-c.h\"\n",
    "filename", file_->name(),
    "basename", StripProto(file_->name()));
  if (rw_fileopts_.generate_gi) {
    printer->Print(
        "#include \"$basename$.pb-c.h\"\n",
      "basename", StripProto(file_->name()));
  }

  printer->Print("\n/* --- minikey descriptors definition start --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateMinikeyDescriptor(printer, hide_from_gi);
  }
  printer->Print("\n/* --- minikey descriptors definition end --- */\n\n");

  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateHelperFunctionDefinitions(printer, false, hide_from_gi);
  }
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateMessageDescriptor(printer, rw_fileopts_.generate_gi, hide_from_gi);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateEnumDescriptor(printer);
  }
  for (int i = 0; i < file_->service_count(); i++) {
    service_generators_[i]->GenerateCFile(printer);
  }

  if (generateGI) {
    // Generate what used to be in the .gi.c files
    std::map<string, string> vars;

    vars["filename"] = StripProto(file_->name());
    vars["filename_full"] = file_->name();
    vars["gi_prefix"] = GetGiPrefix("gi");
    vars["g_func_name"] = GetGiCGlobalFuncName("get_schema");


    printer->Print(vars, "// ** AUTO GENERTATED FILE (by yangpbc) from $filename_full$\n"
                   "// ** DO NOT EDIT **\n"
                   "#include \"$filename$.pb-c.h\"\n\n"
                   "#include <glib.h>\n");

    /*
      ATTN: The error enum belongs in a common file and library
    */

    printer->Print(vars, "GQuark\n"
                   "$gi_prefix$error_quark (void)\n"
                   "{\n"
                   "  static volatile gsize quark = 0;\n\n"
                   "  if (g_once_init_enter (&quark))\n"
                   "    g_once_init_leave (&quark,\n"
                   "                    g_quark_from_static_string (\"$gi_prefix$Error\"));\n"
                   "  return quark;\n"
                   "}\n\n");

    if (file_->package().length()) {
      vars["module"] = MangleNameToUpper(file_->package());
      vars["cmodule"] = MangleNameToCamel(file_->package());
    } else {
      vars["module"] = MangleNameToUpper(StripProto(file_->name()));
      vars["cmodule"] = MangleNameToCamel(StripProto(file_->name()));
    }
    printer->Print(vars, "static const GEnumValue ProtobufCGiError_values[] = {\n"
                   " {$module$_PROTO_GI_ERROR_STALE_REFERENCE, \"STALE_REFERENCE\", \"STALE_REFERENCE\"},\n"
                   " {$module$_PROTO_GI_ERROR_HAS_PARENT, \"HAS_PARENT\", \"HAS_PARENT\"},\n"
                   " {0, NULL, NULL},\n"
                   "}; \n\n");

    printer->Print(vars, "GType $gi_prefix$error_get_type(void)\n"
                   "{\n"
                   "  static GType type = 0; /* G_TYPE_INVALID */ \n\n"
                   "  if (!type)\n"
                   "    type = g_enum_register_static(\"$gi_prefix$Error\", ProtobufCGiError_values);\n"
                   "  return type;\n"
                   "}\n\n");

    for (int i = 0; i < file_->message_type_count(); i++) {
      const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
      message_generators_[i]->GenerateGiCEnumDefs(printer, hide_from_gi);
    }

    for (int i = 0; i < file_->enum_type_count(); i++) {
      enum_generators_[i]->GenerateGiCEnumDefs(printer); // from pbmod->output_gi_c_enums
    }

    for (int i = 0; i < file_->message_type_count(); i++) {
      const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
      message_generators_[i]->GenerateGiCDefs(printer, hide_from_gi);
    }
  }
  
}

/*
 * GI code generation. Maybe move to a separate file??
 */
string FileGenerator::GetGiPrefix(const char* discriminator)
{
  if (file_->package().length()) {
    string name = MangleNameToCamel(file_->package());
    return string("rwpb_") + discriminator + "_" + name;
  } else {
    string name = StripProto(file_->name());
    return string("rwpb_") + discriminator + "_" + ToCamel(name);
  }
}

string FileGenerator::GetGiCGlobalFuncName(const char* operation)
{
  string name = (file_->package());
  return string("rwpb_gi__") + FullNameToLower(name) + "_" + operation;
}


string FileGenerator::GetYpbcGlobal(const char* discriminator)
{
  string name = file_->package();
  return string("rw_ypbc_") + ToCamel(name) + "_" + discriminator;
}

void FileGenerator::GenerateHeader(io::Printer* printer, bool const generateGI)
{
  string filename_identifier = FilenameIdentifier(file_->name());
  static const int min_header_version = 1000000;
  const char *re = getenv("RIFT_ENFORCEOPTS");

  // Generate top of header.
  printer->Print(
    "/* Generated by the protocol buffer compiler. with My change  DO NOT EDIT! */\n"
    "/* Generated from: $filename$ */\n"
    "\n"
    "#ifndef PROTOBUF_C_$filename_identifier$__INCLUDED\n"
    "#define PROTOBUF_C_$filename_identifier$__INCLUDED\n"
    "\n",
    "filename", file_->name(),
    "filename_identifier", filename_identifier);

  if (re && 0==strcmp(re, "1")) {
    printer->Print("#include <rwlib.h>\n");
  }

  printer->Print("#include <protobuf-c/rift-protobuf-c.h>\n\n");

  // Verify the protobuf-c library header version is compatible with the
  // protoc-c version before going any further.
  printer->Print(
    "#if PROTOBUF_C_VERSION_NUMBER < $min_header_version$\n"
    "# error This file was generated by a newer version of protoc-c which is "
    "incompatible with your libprotobuf-c headers. Please update your headers.\n"
    "#elif $protoc_version$ < PROTOBUF_C_MIN_COMPILER_VERSION\n"
    "# error This file was generated by an older version of protoc-c which is "
    "incompatible with your libprotobuf-c headers. Please regenerate this file "
    "with a newer version of protoc-c.\n"
    "#endif\n"
    "\n",
    "min_header_version", SimpleItoa(min_header_version),
    "protoc_version", SimpleItoa(PROTOBUF_C_VERSION_NUMBER));

  printer->Print("#ifndef __GI_SCANNER__\n");
  for (int i = 0; i < file_->dependency_count(); i++) {
    std::string dep_fn = StripProto(file_->dependency(i)->name());
    printer->Print(
      "#include \"$dependency$.pb-c.h\"\n",
      "dependency", dep_fn);
    if (dep_fn == "rw_schema") { // ATTN: Check this.
      printer->Print("#include <rw_keyspec.h>\n");
      printer->Print("#include <rw_keyspec_mk.h>\n");
    }
  }

  
  // Include any files from the yang include extension.
  for (unsigned i = 0; i < rw_fileopts_.c_includes.size(); i++) {
    printer->Print("#include \"$incfile$\"\n",
                   "incfile", rw_fileopts_.c_includes[i].c_str());
  }
  printer->Print("\n");

  printer->Print(
    "\n"
    "PROTOBUF_C_BEGIN_DECLS\n");
  printer->Print("\n");


  // Generate forward declarations of classes.
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateStructTypedef(printer, hide_from_gi);
  }

  printer->Print("\n/* --- Protobuf Concrete Message Descriptors typedefs. --- */\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;

    message_generators_[i]->GeneratePBCMDStructTypedef(printer, hide_from_gi);
  }
  printer->Print("#endif //__GI_SCANNER__\n");

  // Generate enum definitions.
  printer->Print("\n/* --- enums --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateEnumDefinitions(printer, hide_from_gi);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    if (enum_generators_[i]->has_rw_yang_enum()) {
      include_ypbc_header_ = true;
    }
    enum_generators_[i]->GenerateDefinition(printer);
  }

  printer->Print("#ifndef __GI_SCANNER__\n");

  printer->Print("\n/* --- minikeys --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateMiniKeyStructDefinition(printer, hide_from_gi);
  }

  // Generate class definitions.
  printer->Print("\n/* --- messages --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateStructDefinition(printer, hide_from_gi);
    if (message_generators_[i]->riftmopts_.need_ypbc_include) {
      include_ypbc_header_ = true;
    }
  }


  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateHelperFunctionDeclarations(printer, false, hide_from_gi);
  }

  printer->Print("/* --- per-message closures --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateClosureTypedef(printer, hide_from_gi);
  }

  // Generate service definitions.
  printer->Print("\n/* --- services --- */\n\n");
  for (int i = 0; i < file_->service_count(); i++) {
    service_generators_[i]->GenerateMainHFile(printer);
  }

  // Declare extension identifiers.
  for (int i = 0; i < file_->extension_count(); i++) {
    extension_generators_[i]->GenerateDeclaration(printer);
  }

  printer->Print("\n/* --- descriptors --- */\n\n");
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDescriptorDeclarations(printer);
  }
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateDescriptorDeclarations(printer, hide_from_gi);
  }
  for (int i = 0; i < file_->service_count(); i++) {
    service_generators_[i]->GenerateDescriptorDeclarations(printer);
  }

  printer->Print("\n/* --- MetaData Macros --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateMetaDataMacro(printer, rw_fileopts_.generate_gi, hide_from_gi);
  }

  printer->Print("#endif //__GI_SCANNER__\n");

  // Generate what used to be in the .gi.h files

  printer->Print("PROTOBUF_C_END_DECLS\n\n\n");
  printer->Print("#endif  /* PROTOBUF_$filename_identifier$__INCLUDED */\n",
                 "filename_identifier", filename_identifier);

  if (!generateGI) {
    return;
  }

  printer->Print("// The following used to be in the .gi.h files\n");
  printer->Print("#ifndef PROTOBUF_C_GI_$filename_identifier$__INCLUDED\n"
                 "#define PROTOBUF_C_GI_$filename_identifier$__INCLUDED\n", "filename_identifier", filename_identifier);

  std::map<string, string> vars;

  vars["filename"] = StripProto(file_->name());
  vars["incgaurd"] = MangleNameToUpper(file_->package());
  vars["filename_full"] = file_->name();
  vars["gi_prefix"] = GetGiPrefix("gi");
  vars["g_func_name"] = GetGiCGlobalFuncName("get_schema");

  printer->Print(vars, "// ** AUTO GENERTATED FILE(by protoc-c) from $filename_full$.\n"
                 "// ** DO NOT EDIT **\n");


  printer->Print("#ifdef __GI_SCANNER__\n");
  printer->Print("#include <sys/cdefs.h>\n"
                 "#include <stdint.h>\n"
                 "#include <glib-object.h>\n");
  printer->Print("#endif // __GI_SCANNER__\n");                 
  printer->Print("#include <rw_gi.h>\n"
                 "#include <yangmodel_gi.h>\n"
                 "#include <rw_keyspec_gi.h>\n"
                 "#include <yangpb_gi.h>\n");


  for (int i = 0; i < file_->dependency_count(); i++) {
    std::string dep_fn = StripProto(file_->dependency(i)->name());
    if (dep_fn == "descriptor" || dep_fn == "rwpbapi") {
      continue;
    }
    printer->Print("#include \"$dependency$.pb-c.h\"\n", "dependency", dep_fn);
  }

  printer->Print("PROTOBUF_C_BEGIN_DECLS \n\n");

  printer->Print("\n");

  printer->Print(vars, "#ifndef __GI_SCANNER__\n"
                 "#include <$filename$.pb-c.h>\n"
                 "#else\n");

  // ATTN: Generate dependency .gi.h includes
  printer->Print("typedef struct _meh ProtobufCMessage;\n"
                 "#endif  /* __GI_SCANNER__ */ \n\n");

  // ATTN: Generate GIR dependencies

  // Proto GI Error
  if (file_->package().length()) {
    vars["module"] = MangleNameToUpper(file_->package());
    vars["cmodule"] = MangleNameToCamel(file_->package());
  } else {
    vars["module"] = MangleNameToUpper(StripProto(file_->name()));
    vars["cmodule"] = MangleNameToCamel(StripProto(file_->name()));
  }



  printer->Print(vars, "/**\n"
                 " * $module$_PROTO_GI_ERROR:\n"
                 " *\n"
                 " * Error domain for ProtobufCGiMessageBase. Errors in this domain will\n"
                 " * be from the ProtobufCGiError enumeration. See #GError for\n"
                 " * more information on error domains\n"
                 " */\n"
                 "#define $module$_PROTO_GI_ERROR $gi_prefix$error_quark () \n\n");

  printer->Print(vars, "/**\n"
                 " * $cmodule$ProtobufCGiError:\n"
                 " * @$module$_PROTO_GI_ERROR_STALE_REFERENCE: The originally referenced protobuf has\n"
                 " *                                  changed and this reference is now stale.\n"
                 " * These identify the various errors that can occur while using ProtobufCGiMessageBase.\n"
                 " */\n");

  printer->Print(vars, "typedef enum {\n"
                 "  $module$_PROTO_GI_ERROR_STALE_REFERENCE = 10001,\n"
                 "  $module$_PROTO_GI_ERROR_HAS_PARENT = 10002,\n"
                 "  $module$_PROTO_GI_ERROR_XML_TO_PB_FAILURE = 10003,\n"
                 "  $module$_PROTO_GI_ERROR_INVALID_ENUM = 10004,\n"
                 "  $module$_PROTO_GI_ERROR_FAILURE = 10005,\n" // Generic error code.
                 "} $cmodule$ProtobufCGiError;\n\n");

  printer->Print(vars, "GQuark $gi_prefix$error_quark(void); \n\n");



  printer->Print("\n\n\n");

  // String-to-int and vice-versa utility functions for gi and victims thereof
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateGiHEnums(printer, hide_from_gi);
  }

  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateGiHEnums(printer);
  }

  printer->Print("\n");
  printer->Print("// Typedef declarations for message types exported through GI\n");

  // Iterate and generate the typedefs for all the messages
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateGiTypeDefDecls(printer, hide_from_gi);
  }

  printer->Print("#ifndef __GI_SCANNER__\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateGiCBoxedStructDefs(printer, hide_from_gi);
  }

  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateGiRefHelperDecls(printer, hide_from_gi);
  }

  printer->Print("#endif  /* __GI_SCANNER__ */\n");

  // Iterate and generate the static declarations for all the messages
  printer->Print("// Static proto declarations for message types exported through GI\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    const bool hide_from_gi = message_generators_[i]->riftmopts_.hide_from_gi;
    message_generators_[i]->GenerateGiHDecls(printer, hide_from_gi);
  }

  printer->Print("\n PROTOBUF_C_END_DECLS\n");

  if (include_ypbc_header_) {
    printer->Print(
        "\n"

        "#include \"$basename$.ypbc.h\"\n\n",
        "filename_identifier", filename_identifier,
        "basename", StripProto(file_->name()));
  } 


  
  printer->Print("#endif // PROTOBUF_C_GI_$filename_identifier$__INCLUDED\n",
                 "filename_identifier", filename_identifier);


}


}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
