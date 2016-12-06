
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */



/**
 * @file yangpbc_exe.cpp
 * @author Tom Seidenberg
 * @date 2014/03/19
 * @brief Convert yang schema to google protobuf
 */

#include "rwlib.h"
#include "yangpbc.hpp"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <fstream>
#include <ios>
#include <iostream>
#include <mutex>
#include <sstream>


using namespace rw_yang;

int main(int argc, char *argv[])
{
  if (argc < 3) {
    std::cout << "Usage: yangpbc OUTPUTBASE YANGMODULE" << std::endl;
    exit(1);
  }

  YangModelNcx* ymodel = YangModelNcx::create_model();
  YangModel::ptr_t cleanup_ptr(ymodel);
  PbModel pbmodel(ymodel);

  const char* out_basename = argv[1];
  if (out_basename[0] == '\0') {
    std::cout << "Empty OUTPUTBASE" << std::endl;
    exit(1);
  }
  const char* eop = strrchr(out_basename, '/');
  if (nullptr == eop) {
    eop = out_basename;
  } else {
    ++eop;
    if (eop[0] == '\0') {
      std::cout << "Can't determine basename for OUTPUTBASE" << std::endl;
      exit(1);
    }
  }

  if (argc != 3) {
    std::ostringstream oss;
    pbmodel.error( "<unknown>", "Only one module argument is allowed" );
    exit(1);
  }

  const char* yang_filename = argv[2];
  PbModule* pbmod = pbmodel.module_load(yang_filename);
  if (nullptr == pbmod) {
    std::ostringstream oss;
    pbmodel.error( yang_filename, "Unable to load yang module" );
    exit(1);
  }

  pbmodel.parse_nodes();
  if (pbmodel.is_failed()) {
    std::cout << "Errors found, exiting without writing output" << std::endl;
    exit(1);
  }

  int ret_status = 0;

  parser_control_t pc = pbmodel.output_proto(out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate proto file" << std::endl;
      ret_status = 1;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  pc = pbmodel.output_h_file(out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate include file" << std::endl;
      ret_status = 2;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  pc = pbmodel.output_cpp_file(out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate C source file" << std::endl;
      ret_status = 3;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  pc = pbmodel.output_gi_c_file(out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate GI C source file" << std::endl;
      ret_status = 3;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  pc = pbmodel.output_doc_user_file(doc_file_t::TEXT, out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate user documentation file" << std::endl;
      ret_status = 3;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  pc = pbmodel.output_doc_api_file(doc_file_t::TEXT, out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate API documentation file" << std::endl;
      ret_status = 3;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  pc = pbmodel.output_doc_user_file(doc_file_t::HTML, out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate user documentation file" << std::endl;
      ret_status = 3;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  pc = pbmodel.output_doc_api_file(doc_file_t::HTML, out_basename);
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      std::cout << "Failed to generate API documentation file" << std::endl;
      ret_status = 3;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  return ret_status;
}


// End of yangpbc.cpp
