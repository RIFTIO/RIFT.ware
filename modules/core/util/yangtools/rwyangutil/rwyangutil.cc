/* STANDARD_RIFT_IO_COPYRIGHT */
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
  bool created_lock = false;
  bool success = true;
  rwyangutil::FileProtoOps fops;

  try {
    if (argc < 2) {
      std::cerr << "Insufficient number of arguments provided" << std::endl;
      rwyangutil::ArgumentParser(argv, argc).print_help();
      exit(EXIT_FAILURE);
    }

    rwyangutil::ArgumentParser const arguments(argv, argc);

    if (arguments.exists("-h") || arguments.exists("--help")) {
      arguments.print_help();
      exit(EXIT_SUCCESS);
    }

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
  } catch(const std::string& e) {
    std::cerr << "ERROR: " << e << std::endl;
    success = false;
  } catch(const char* e) {
    std::cerr << "ERROR: " << e << std::endl;
    success = false;
  }

  if (!success && created_lock) {
    // if we have created the lock file, clean it up
    fops.execute_command("--lock-file-delete", std::vector<std::string>());
  }

  if (success) {
    exit(EXIT_SUCCESS);
  }

  return EXIT_FAILURE;
}
