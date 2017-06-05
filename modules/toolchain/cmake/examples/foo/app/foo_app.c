/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file foo_app.c
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/22/2013
 * @brief Brief description of the foo_app example
 * 
 * @details Detailed description of the foo_app example
 *
 */
#include <stdio.h>
#include "foo_lib.h"

int main(int argc, char **argv)
{
  int rc;
  rc = foo_lib_hello_world();
  return 0;
}
