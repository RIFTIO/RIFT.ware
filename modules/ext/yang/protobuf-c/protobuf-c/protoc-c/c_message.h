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

#ifndef GOOGLE_PROTOBUF_COMPILER_C_MESSAGE_H__
#define GOOGLE_PROTOBUF_COMPILER_C_MESSAGE_H__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <protoc-c/c_field.h>

namespace google {
namespace protobuf {
  namespace io {
    class Printer;             // printer.h
  }
}

namespace protobuf {
namespace compiler {
namespace c {

class EnumGenerator;           // enum.h
class ExtensionGenerator;      // extension.h

struct RiftMopts {
  int enforce;
  int agg;
  int suppress;
  int flatinline;
  uint32_t inline_max;
  uint32_t rw_flags;
  std::string ypbc_msg;
  bool need_ypbc_include;
  std::string c_typedef;
  std::string c_typename;
  std::string base_typename; /* ATTN: Should be repeated */
  std::string msg_new_path;
  bool hide_from_gi;
  RiftMopts()
      : enforce(),
      agg(),
      suppress(),
      flatinline(),
      inline_max(),
      rw_flags(),
      need_ypbc_include(),
      hide_from_gi()
  {}
};

void mdesc_to_riftmopts(const Descriptor* descriptor_, RiftMopts* riftmopts);

class MessageGenerator {
 public:
  // See generator.cc for the meaning of dllexport_decl.
  explicit MessageGenerator(const Descriptor* descriptor,
                            const string& dllexport_decl);
  ~MessageGenerator();

  // Header stuff.

  // Generate typedef.
  void GenerateStructTypedef(io::Printer* printer, const bool hide_from_gi);

  // Generate descriptor prototype
  void GenerateDescriptorDeclarations(io::Printer* printer, const bool hide_from_gi);

  // Generate descriptor prototype
  void GenerateClosureTypedef(io::Printer* printer, const bool hide_from_gi);

  // Generate definitions of all nested enums (must come before class
  // definitions because those classes use the enums definitions).
  void GenerateEnumDefinitions(io::Printer* printer, const bool hide_from_gi);

  // Generate definitions for this class and all its nested types.
  void GenerateStructDefinition(io::Printer* printer, const bool hide_from_gi);

  // Generate standard helper functions declarations for this message.
  void GenerateHelperFunctionDeclarations(io::Printer* printer, bool is_submessage, const bool hide_from_gi);

  // Source file stuff.

  // Generate code that initializes the global variable storing the message's
  // descriptor.
  void GenerateMessageDescriptor(io::Printer* printer, bool generate_gi, const bool hide_from_gi);
  void GenerateHelperFunctionDefinitions(io::Printer* printer, bool is_submessage, const bool hide_from_gi);

  void GenerateMiniKeyStructDefinition(io::Printer* printer, const bool hide_from_gi);
  void GenerateMinikeyDescriptor(io::Printer* printer, const bool hide_from_gi);
  
  // Generate the MetaData Macro for this message and its fields.
  void GenerateMetaDataMacro(io::Printer* printer, bool generate_gi, const bool hide_from_gi);

  void GeneratePBCMDStructDefinition(io::Printer* printer);
  void GeneratePBCMDStructTypedef(io::Printer* printer, const bool hide_from_gi);


  RiftMopts riftmopts_; /* ATTN: SHOULD BE PRIVATE! */

  // Gi code generation support functions.
  string GetGiCIdentifier(const char *operation = NULL);
  string GetGiDescTypeName();
  string GetGiDescIdentifier(const char *operation = NULL);

  void GenerateGiTypeDefDecls(io::Printer* printer, const bool hide_from_gi);
  void GenerateGiCBoxedStructDefs(io::Printer* printer, const bool hide_from_gi);
  void GenerateGiRefHelperDecls(io::Printer* printer, const bool hide_from_gi);
  void GenerateGiHDecls(io::Printer* printer, const bool hide_from_gi);
  void GenerateGiToPbufMethodDecl(io::Printer* printer);
  void GenerateGiFromPbufMethodDecl(io::Printer* printer);
  void GenerateGiToPtrMethodDecl(io::Printer* printer);
  void GenerateGiRetrieveDescriptorMethodDecl(io::Printer* printer);
  void GenerateGiToPbcmMethodDecl(io::Printer* printer);
  void GenerateGiFromPbcmMethodDecl(io::Printer* printer);
  void GenerateGiSchemaMethodDecl(io::Printer* printer);
  void GenerateGiSchemaChangeToDescriptorMethodDecl(io::Printer* printer);
  void GenerateGiCopyFromMethodDecl(io::Printer* printer);
  void GenerateGiHasFieldMethodDecl(io::Printer* printer);
  void GenerateGiCDefs(io::Printer* printer, const bool hide_from_gi);
  void GenerateGiBoxedRegistration(io::Printer* printer);
  void GenerateGiBoxedUnrefDefinition(io::Printer* printer);
  void GenerateGiNewMethod(io::Printer* printer);
  void GenerateGiInvalidateMethod(io::Printer* printer);
  void GenerateGiToPbufMethod(io::Printer* printer);
  void GenerateGiFromPbufMethod(io::Printer* printer);
  void GenerateGiToPtrMethod(io::Printer* printer);
  void GenerateGiRetrieveDescriptorMethod(io::Printer* printer);
  void GenerateGiToPbcmMethod(io::Printer* printer);
  void GenerateGiFromPbcmMethod(io::Printer* printer);
  void GenerateGiSchemaMethod(io::Printer* printer);
  void GenerateGiSchemaChangeToDescriptorMethod(io::Printer* printer);
  void GenerateGiCopyFromMethod(io::Printer* printer);
  void GenerateGiHasFieldMethod(io::Printer* printer);
  void GenerateGiHEnums(io::Printer* printer, const bool hide_from_gi);
  void GenerateGiCEnumDefs(io::Printer* printer, const bool hide_from_gi);

  string gi_typename_;

 private:

  string GetDefaultValueC(const FieldDescriptor *fd);


  const Descriptor* descriptor_;
  string dllexport_decl_;
  FieldGeneratorMap field_generators_;
  scoped_array<scoped_ptr<MessageGenerator> > nested_generators_;
  scoped_array<scoped_ptr<EnumGenerator> > enum_generators_;
  scoped_array<scoped_ptr<ExtensionGenerator> > extension_generators_;
  bool gen_minikey_type_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MessageGenerator);
};

}  // namespace c
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // GOOGLE_PROTOBUF_COMPILER_C_MESSAGE_H__
