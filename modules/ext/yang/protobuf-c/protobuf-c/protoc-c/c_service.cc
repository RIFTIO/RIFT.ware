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

#if 0
#define RIFTTRACE(args...) printf(args)
#else
#define RIFTTRACE(args...)
#endif

#include <protoc-c/c_service.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

ServiceGenerator::ServiceGenerator(const ServiceDescriptor* descriptor,
                                   const string& dllexport_decl)
  : descriptor_(descriptor) {
  vars_["name"] = descriptor_->name();
  vars_["fullname"] = descriptor_->full_name();
  vars_["cname"] = FullNameToC(descriptor_->full_name());
  vars_["lcfullname"] = FullNameToLower(descriptor_->full_name());
  vars_["ucfullname"] = FullNameToUpper(descriptor_->full_name());
  vars_["lcfullpadd"] = ConvertToSpaces(vars_["lcfullname"]);
  vars_["package"] = descriptor_->file()->package();
  if (dllexport_decl.empty()) {
    vars_["dllexport"] = "";
  } else {
    vars_["dllexport"] = dllexport_decl + " ";
  }
}

ServiceGenerator::~ServiceGenerator() {}

// Header stuff.
void ServiceGenerator::GenerateMainHFile(io::Printer* printer)
{
  GenerateVfuncs(printer);
  GenerateInitMacros(printer);
  GenerateCallersDeclarations(printer);
}
void ServiceGenerator::GenerateVfuncs(io::Printer* printer)
{
  printer->Print(vars_,
		 "/*  $cname$ Service.\n"
		 " *\n"
		 " *  For server side, declare:\n"
                 " *        $cname$_Service myapisrv;\n"
		 " *        $ucfullname$__INITSERVER(&myapisrv, myapp_);\n"
		 " *    AND provide myapp_foo(...) functions for each method foo\n"
		 " *    AND call rwmsg_srvchan_addservice(..., &myapisrv)\n"
		 " *\n"  
		 " *  For client side, declare:\n"
		 " *        $cname$_Client myapicli;\n"
		 " *        $ucfullname$__INITCLIENT(&myapicli);\n"
		 " *    AND call rwmsg_clichan_addservice(..., &myapicli)\n"
		 " *    THEN call eg $cname$__methodname(&myapicli, rwmsgdest, &input, clofunc, ud);\n"
		 " */\n"
		 "typedef struct $cname$_Service $cname$_Service;\n"
		 "struct $cname$_Service\n"
		 "{\n"
		 "  ProtobufCService base;\n");
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor *method = descriptor_->method(i);
    string lcname = CamelToLower(method->name());
    vars_["method"] = lcname;
    vars_["metpad"] = ConvertToSpaces(lcname);
    vars_["input_typename"] = FullNameToC(method->input_type()->full_name());
    vars_["output_typename"] = FullNameToC(method->output_type()->full_name());
    printer->Print(vars_,
                   "  void (*$method$)($cname$_Service *service,\n"
                   "         $metpad$  const $input_typename$ *input,\n"
		   "         $metpad$  void *usercontext,\n"
                   "         $metpad$  $output_typename$_Closure closure,\n"
                   "         $metpad$  void *closure_data);\n");
  }
  printer->Print(vars_,
		 "};\n"
		 "typedef struct $cname$_Client $cname$_Client;\n"
		 "struct $cname$_Client\n"
		 "{\n"
		 "  ProtobufCService base;\n");
  printer->Print(vars_,
		 "};\n");
}
void ServiceGenerator::GenerateInitMacros(io::Printer* printer)
{
  printer->Print(vars_,
		 "#define __$ucfullname$__BASE_INTERNAL_INITIALIZER__ \\\n"
		 "    { &$lcfullname$__descriptor, NULL/*riftctx*/, NULL/*usrctx*/, protobuf_c_service_invoke_internal, protobuf_c_service_invoke_internal_b, NULL  }\n"
		 "#define $ucfullname$__INITSERVER(srv, function_prefix__) \\\n"
		 "  do {  \\\n"
		 "    (srv)->base.descriptor = &$lcfullname$__descriptor;\\\n"
		 "    (srv)->base.rw_context = NULL;\\\n"
		 "    (srv)->base.rw_usercontext = NULL;\\\n"
		 "    (srv)->base.invoke = protobuf_c_service_invoke_internal; \\\n"
		 "    (srv)->base.invoke_b = protobuf_c_service_invoke_internal_b; \\\n"
		 "    (srv)->base.destroy = NULL; \\\n"
		 "    (srv)->base.rw_context = NULL;\\\n");
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor *method = descriptor_->method(i);
    string lcname = CamelToLower(method->name());
    vars_["method"] = lcname;
    vars_["metpad"] = ConvertToSpaces(lcname);
    vars_["methidx"] = SimpleItoa(i);
    vars_["input_typename"] = FullNameToC(method->input_type()->full_name());
    vars_["output_typename"] = FullNameToC(method->output_type()->full_name());
    printer->Print(vars_,
                   "    (srv)->$method$ = (void (*)($cname$_Service *, const $input_typename$ *, void *, $output_typename$_Closure, void *))function_prefix__ ## $method$;  \\\n");
  }
  printer->Print(vars_,
		 "  } while(0)\n"
		 "#define $ucfullname$__INITCLIENT(srv) \\\n"
		 "  do {\\\n"
		 "    (srv)->base.descriptor = &$lcfullname$__descriptor;\\\n"
		 "    (srv)->base.rw_context = NULL;\\\n"
		 "    (srv)->base.rw_usercontext = NULL;\\\n"
		 "    (srv)->base.invoke = protobuf_c_service_invoke_internal; \\\n"
		 "    (srv)->base.invoke_b = protobuf_c_service_invoke_internal_b; \\\n"
		 "    (srv)->base.destroy = NULL; \\\n"
		 "    (srv)->base.rw_context = NULL;\\\n"
		 "  } while(0)\n");

}

struct riftmethopts {
  uint32_t methno;
  uint32_t pri;
  uint32_t flags;

  uint32_t blocking:1;
};
static void parseriftopts(const MethodDescriptor *method, const ServiceDescriptor *descriptor_, struct riftmethopts *rifto) {
  MethodOptions mopts(method->options());
  UnknownFieldSet *unks = mopts.mutable_unknown_fields();
  if (!unks->empty()) {
    RIFTTRACE ("**method descriptor '%s' opt count %d\n",
	       method->full_name().c_str(), unks->field_count());
    for (int i=0; i<unks->field_count(); i++) {
      UnknownField *unkf = unks->mutable_field(i);
      RIFTTRACE ("**  ext opt field number %d type %d\n", (int)unkf->number(), (int)unkf->type());
      switch (unkf->number()) {
      case 50000:
	if (UnknownField::TYPE_LENGTH_DELIMITED == unkf->type()) {
	  RIFTTRACE ("method descriptor with rift_methoptions of type ldelim\n");
	  RIFTTRACE("  buf len=%d: ", unkf->length_delimited().length());
	  for (int k=0; k<unkf->length_delimited().length(); k++) {
	    RIFTTRACE("%0X ", (uint32_t) *(const char*)unkf->length_delimited().substr(k, 1).c_str());
	  }
	  RIFTTRACE("\n");
	  UnknownFieldSet ropts;
	  if (ropts.ParseFromString(unkf->length_delimited())) {
	    RIFTTRACE("***ldelim parsed, %d fields\n", ropts.field_count());
	    for (int j=0; j<ropts.field_count(); j++) {
	      UnknownField *ropt = ropts.mutable_field(j);
	      switch (ropt->number()) {
	      case 1:
		if (ropt->type() == UnknownField::TYPE_VARINT) {
		  RIFTTRACE("method number %u\n", (unsigned int)ropt->varint());
		  if (ropt->varint() > 0xffff) {
		    fprintf(stderr, 
			    "*** protoc-c: method number %Lu too big for method '%s' of service '%s'\n",
			    (uint64_t)ropt->varint(), 
			    method->full_name().c_str(),
			    descriptor_->full_name().c_str());
		    exit(1);
		  }
		  if (ropt->varint()==0) {
		    fprintf(stderr, 
			    "*** protoc-c: method number %Lu not allowed for method '%s' of service '%s'\n",
			    (uint64_t)ropt->varint(), 
			    method->full_name().c_str(),
			    descriptor_->full_name().c_str());
		    exit(1);
		  }
		  rifto->methno = (uint16_t)ropt->varint();
		} else {
		  fprintf(stderr, 
			  "*** protoc-c: unk field datatype %d for srv desc riftopt field 1 (methno) of method '%s' service '%s'\n",
			  (int)ropt->type(),
			  method->full_name().c_str(),
			  descriptor_->full_name().c_str());
		  exit(1);
		}
		break;

	      case 2:
		if (ropt->type() == UnknownField::TYPE_VARINT) {
		  RIFTTRACE("pri value %u\n", (unsigned int)ropt->varint());
		  if (ropt->varint() >= 0 && ropt->varint() <= 3) {
		    rifto->pri = (uint8_t)ropt->varint();
		  } else {
		    fprintf(stderr, 
			    "*** protoc-c: Unsupported rift method priority value %u on method '%s' service '%s'\n",
			    (unsigned int)ropt->varint(),
			    method->full_name().c_str(),
			    descriptor_->full_name().c_str());
		    exit(1);
		  }
		} else {
		  fprintf(stderr, 
			  "*** protoc-c: unk field datatype %d for srv desc riftopt field 2 (pri) of method '%s' service '%s'\n",
			  (int)ropt->type(),
			  method->full_name().c_str(),
			  descriptor_->full_name().c_str());
		  exit(1);
		}
		break;


	      case 3:
		if (ropt->type() == UnknownField::TYPE_VARINT) {
		  RIFTTRACE("blk value %u\n", (unsigned int)ropt->varint());
		  rifto->blocking = !!ropt->varint();
		} else {
		  fprintf(stderr, 
			  "*** protoc-c: unk field datatype %d for srv desc riftopt field 3 (blocking) of method '%s' service '%s'\n",
			  (int)ropt->type(),
			  method->full_name().c_str(),
			  descriptor_->full_name().c_str());
		  exit(1);
		}
		break;
		  
	      default:
		fprintf(stderr, "unk srv desc riftopt field number %u on method '%s' service '%s'\n",
			(uint32_t)ropt->number(),
			method->full_name().c_str(),
			descriptor_->full_name().c_str());
		exit(1);
		break;
	      }
	    }
	  }
	}
	break;
      default:
	RIFTTRACE("unk ext option %d on service\n", unkf->number());
	break;
      }
    }
  }
}

void ServiceGenerator::GenerateCallersDeclarations(io::Printer* printer)
{
  const char *re = getenv("RIFT_ENFORCEOPTS");
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor *method = descriptor_->method(i);
    string lcname = CamelToLower(method->name());
    string lcfullname = FullNameToLower(descriptor_->full_name());
    vars_["srvname"] = FullNameToC(descriptor_->full_name());
    vars_["method"] = lcname;
    vars_["metpad"] = ConvertToSpaces(lcname);
    vars_["input_typename"] = FullNameToC(method->input_type()->full_name());
    vars_["output_typename"] = FullNameToC(method->output_type()->full_name());
    vars_["padddddddddddddddddd"] = ConvertToSpaces(lcfullname + "__" + lcname);
    if (re && 0==strcmp(re, "1")) {
      printer->Print(vars_,
		     "#ifndef RIFT_OMIT_PROTOC_RWMSG_BINDINGS\n"
		     "rw_status_t $lcfullname$__$method$($srvname$_Client *service,\n"
		     "            $padddddddddddddddddd$ rwmsg_destination_t *dest,\n"
		     "            $padddddddddddddddddd$ const $input_typename$ *input,\n"
		     "            $padddddddddddddddddd$ rwmsg_closure_t *closure,\n"
		     "            $padddddddddddddddddd$ rwmsg_request_t **req_out);\n");

      struct riftmethopts riftopts = {};
      parseriftopts(method, descriptor_, &riftopts);
      if (riftopts.blocking) {
	printer->Print(vars_,
		       "rw_status_t $lcfullname$__$method$_b($srvname$_Client *service,\n"
		       "            $padddddddddddddddddd$   rwmsg_destination_t *dest,\n"
		       "            $padddddddddddddddddd$   const $input_typename$ *input,\n"
		       "            $padddddddddddddddddd$   rwmsg_request_t **req_out);\n");
      }
      printer->Print(vars_,
		     "#endif\n");
    }
  }
}

void ServiceGenerator::GenerateDescriptorDeclarations(io::Printer* printer)
{
  printer->Print(vars_, "extern const ProtobufCServiceDescriptor $lcfullname$__descriptor;\n");
}


// Source file stuff.
void ServiceGenerator::GenerateCFile(io::Printer* printer)
{
  GenerateServiceDescriptor(printer);
  GenerateCallersImplementations(printer);
  GenerateInit(printer);
}
void ServiceGenerator::GenerateInit(io::Printer* printer)
{
}

struct MethodIndexAndName { unsigned i; const char *name; };
static int
compare_method_index_and_name_by_name (const void *a, const void *b)
{
  const MethodIndexAndName *ma = (const MethodIndexAndName *) a;
  const MethodIndexAndName *mb = (const MethodIndexAndName *) b;
  return strcmp (ma->name, mb->name);
}


void ServiceGenerator::GenerateServiceDescriptor(io::Printer* printer)
{
  const char *re = getenv("RIFT_ENFORCEOPTS");
  int n_methods = descriptor_->method_count();
  MethodIndexAndName *mi_array = new MethodIndexAndName[n_methods];
  
  if (re && 0!=strcmp(re, "1")) {
    re=NULL;
  }

  vars_["n_methods"] = SimpleItoa(n_methods);
  printer->Print(vars_, "static const ProtobufCMethodDescriptor $lcfullname$__method_descriptors[$n_methods$] =\n"
                       "{\n");
  for (int i = 0; i < n_methods; i++) {
    const MethodDescriptor *method = descriptor_->method(i);

    struct riftmethopts riftopts = {};
    parseriftopts(method, descriptor_, &riftopts);
    if (re && !riftopts.methno) {
      fprintf(stderr, "*** No rift method number defined for method %s service %s\n", 
	      method->name().c_str(), descriptor_->full_name().c_str());
      exit(1);
    }

    vars_["methno"] = SimpleItoa(riftopts.methno);
    vars_["method"] = method->name();
    vars_["input_descriptor"] = "&" + FullNameToLower(method->input_type()->full_name()) + "__descriptor";
    vars_["output_descriptor"] = "&" + FullNameToLower(method->output_type()->full_name()) + "__descriptor";
    vars_["riftpri"] = SimpleItoa(riftopts.pri);
    vars_["riftflags"] = SimpleItoa(riftopts.flags);
    printer->Print(vars_,
		   "  { \"$method$\", $input_descriptor$, $output_descriptor$,\n"
		   "    $methno$/*rw_methno*/, $riftflags$/*rw_flags*/, $riftpri$/*rw_pri*/ },\n");
    mi_array[i].i = i;
    mi_array[i].name = method->name().c_str();
  }
  printer->Print(vars_, "};\n");

  qsort ((void*)mi_array, n_methods, sizeof (MethodIndexAndName),
         compare_method_index_and_name_by_name);
  printer->Print(vars_, "const unsigned $lcfullname$__method_indices_by_name[] = {\n");
  for (int i = 0; i < n_methods; i++) {
    vars_["i"] = SimpleItoa(mi_array[i].i);
    vars_["name"] = mi_array[i].name;
    vars_["comma"] = (i + 1 < n_methods) ? "," : " ";
    printer->Print(vars_, "  $i$$comma$        /* $name$ */\n");
  }
  printer->Print(vars_, "};\n");

  vars_["srvno"] = "0";
  ServiceOptions sopts(descriptor_->options());
  UnknownFieldSet *unks = sopts.mutable_unknown_fields();
  if (!unks->empty()) {
    RIFTTRACE ("**service descriptor '%s' opt count %d\n",
	    descriptor_->full_name().c_str(), unks->field_count());
    for (int i=0; i<unks->field_count(); i++) {
      UnknownField *unkf = unks->mutable_field(i);
      RIFTTRACE ("**  ext opt field number %d type %d\n", (int)unkf->number(), (int)unkf->type());
      switch (unkf->number()) {
      case 50002:
	if (UnknownField::TYPE_LENGTH_DELIMITED == unkf->type()) {
	  RIFTTRACE ("service descriptor with rift_srvoptions of type ldelim\n");
	  RIFTTRACE("  buf len=%d: ", unkf->length_delimited().length());
	  for (int k=0; k<unkf->length_delimited().length(); k++) {
	    RIFTTRACE("%0X ", (uint32_t) *(const char*)unkf->length_delimited().substr(k, 1).c_str());
	  }
	  RIFTTRACE("\n");
	  UnknownFieldSet ropts;
	  if (ropts.ParseFromString(unkf->length_delimited())) {
	    RIFTTRACE("***ldelim parsed, %d fields\n", ropts.field_count());
	    for (int j=0; j<ropts.field_count(); j++) {
	      UnknownField *ropt = ropts.mutable_field(j);
	      switch (ropt->number()) {
	      case 1:
		if (ropt->type() == UnknownField::TYPE_VARINT) {
		  RIFTTRACE("service number %u\n", (unsigned int)ropt->varint());
		  vars_["srvno"] = SimpleItoa(ropt->varint());
		} else {
		  RIFTTRACE("unk type %d for srv desc riftopt field 1\n", (int)ropt->type());
		}
		break;
	      default:
		RIFTTRACE("unk srv desc riftopt field number %d\n", (int)ropt->number());
		break;
	      }
	    }
	  }
	}
	break;
      default:
	RIFTTRACE("unk ext option %d on service\n", unkf->number());
	break;
      }
    }
  }
  if (re && vars_["srvno"] == "0") {
    fprintf(stderr, "*** No rift service number defined for service %s\n", 
	    descriptor_->full_name().c_str());
    exit(1);
  }

  printer->Print(vars_, "const ProtobufCServiceDescriptor $lcfullname$__descriptor =\n"
                       "{\n"
		       "  PROTOBUF_C_SERVICE_DESCRIPTOR_MAGIC,\n"
   		       "  $srvno$,     /* rift srvno */\n"
		       "  \"$fullname$\",\n"
		       "  \"$name$\",\n"
		       "  \"$cname$\",\n"
		       "  \"$package$\",\n"
		       "  $n_methods$,\n"
		       "  $lcfullname$__method_descriptors,\n"
		       "  $lcfullname$__method_indices_by_name\n"
		       "};\n");

  delete[] mi_array;
}

void ServiceGenerator::GenerateCallersImplementations(io::Printer* printer)
{
  const char *re = getenv("RIFT_ENFORCEOPTS");
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor *method = descriptor_->method(i);
    string lcname = CamelToLower(method->name());
    string lcfullname = FullNameToLower(descriptor_->full_name());
    vars_["srvname"]  = FullNameToC(descriptor_->full_name());
    vars_["method"] = lcname;
    vars_["metpad"] = ConvertToSpaces(lcname);
    vars_["input_typename"] = FullNameToC(method->input_type()->full_name());
    vars_["output_typename"] = FullNameToC(method->output_type()->full_name());
    vars_["padddddddddddddddddd"] = ConvertToSpaces(lcfullname + "__" + lcname);
    vars_["index"] = SimpleItoa(i);

    struct riftmethopts riftopts = {};
    parseriftopts(method, descriptor_, &riftopts);
    if (riftopts.methno) {
      printer->Print(vars_,
		     "#ifndef RIFT_OMIT_PROTOC_RWMSG_BINDINGS\n"
		     "rw_status_t $lcfullname$__$method$($srvname$_Client *service,\n"
		     "            $padddddddddddddddddd$ rwmsg_destination_t *dest,\n"
		     "            $padddddddddddddddddd$ const $input_typename$ *input,\n"
		     "            $padddddddddddddddddd$ rwmsg_closure_t *closure,\n"
		     "            $padddddddddddddddddd$ rwmsg_request_t **req_out)\n"
		     "{\n"
		     "  PROTOBUF_C_ASSERT (service->base.descriptor == &$lcfullname$__descriptor);\n"
		     "  return rwmsg_clichan_send_protoc_internal(service->base.rw_context, dest, 0, &service->base, $index$, (ProtobufCMessage *)input, closure, req_out);\n"
		     "}\n"
		     "#endif\n");
      if (riftopts.blocking) {
	printer->Print(vars_,
		       "#ifndef RIFT_OMIT_PROTOC_RWMSG_BINDINGS\n"
		       "rw_status_t $lcfullname$__$method$_b($srvname$_Client *service,\n"
 		       "            $padddddddddddddddddd$   rwmsg_destination_t *dest,\n"
		       "            $padddddddddddddddddd$   const $input_typename$ *input,\n"
		       "            $padddddddddddddddddd$   rwmsg_request_t **req_out)\n"
		       "{\n"
		       "  PROTOBUF_C_ASSERT (service->base.descriptor == &$lcfullname$__descriptor);\n"
		       "  return rwmsg_clichan_send_protoc_internal(service->base.rw_context, dest, (0|RWMSG_CLICHAN_PROTOC_FLAG_BLOCKING), &service->base, $index$, (ProtobufCMessage*)input, NULL, req_out);\n"
		       "}\n"
		       "#endif\n");
      }
    }
  }
}


}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
