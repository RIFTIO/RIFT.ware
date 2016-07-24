/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_gtest.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/18/2013
 * @brief rift specific google test classes
 */

#ifndef __RW_GTEST_H__
#define __RW_GTEST_H__

#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>

#define TEST_DESCRIPTION(desc) ::testing::Test::RecordProperty("description", desc)

#define MAX_FILENAME_SZ 64

class ConsoleCapture {
 private:
  static char filename[MAX_FILENAME_SZ]; 
  static int stdout_backup;
  static std::string capture_str;

  static int restore_stdout(void);
  static int redirect_stdout(void);
  static int stop_stdout_capture(void);

  /* Non public constructor so that class can't be 
   * instantiated */
  ConsoleCapture();
  ~ConsoleCapture();

 public:
  static int start_stdout_capture(void);
  static std::string get_capture_string(void);
};

#endif // __RW_GTEST_H__
