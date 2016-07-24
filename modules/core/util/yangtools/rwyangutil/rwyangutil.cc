
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rw_file_proto_ops.cc
 * @author Arun Muralidharan
 * @date 01/10/2015
 * @brief App library file protocol operations
 * @details App library file protocol operations
 */

#include <iostream>
#include <string>

#include "rwyangutil.h"
#include "rwyangutil_argument_parser.hpp"
#include "rwyangutil_file_proto_ops.hpp"


int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Insufficient number of arguments provided" << std::endl;
    rwyangutil::ArgumentParser(argv, argc).print_help();
    return EXIT_FAILURE;
  }

  rwyangutil::ArgumentParser const arguments(argv, argc);

  if (arguments.exists("-h") || arguments.exists("--help")) {
    arguments.print_help();
    return EXIT_SUCCESS;
  }


  rwyangutil::FileProtoOps fops;

  bool created_lock = false;
  bool success = true;
  for (std::string const & command : arguments) {
    if (fops.validate_command(command)) {
      if ( !fops.execute_command(command, arguments.get_params_list(command))) {
        success = false;
        break;
      } else {
        // success
        if (command == "--lock-file-create") {
          created_lock = true;
        }
      }
    } else {
      std::cerr << "ERROR: invalid option provided: " << command << "\n";
      success = false;
      break;
    }
  }

  if (success) {
    return EXIT_SUCCESS;
  } else {
    if (created_lock) {
      // if we have created the lock file, clean it up
      fops.execute_command("--lock-file-delete", std::vector<std::string>());
    }
    return EXIT_FAILURE;
  }
}
