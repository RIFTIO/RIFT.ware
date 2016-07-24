/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file capture_console.cc
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/18/2013
 * @brief C++ class for capturing console output
 * 
 * @details Helps in writing test cases that output to console
 */

#include "gtest/rw_gtest.h"

char ConsoleCapture::filename[MAX_FILENAME_SZ];
int ConsoleCapture::stdout_backup;
std::string ConsoleCapture::capture_str;

ConsoleCapture::ConsoleCapture()
{
  stdout_backup = 0;
  snprintf(filename, MAX_FILENAME_SZ, "stdout.XXXXXX");
}

ConsoleCapture::~ConsoleCapture()
{
}

int ConsoleCapture::restore_stdout(void)
{
  int copy;
  
  if (stdout_backup <= 0) {
    /* stdout was not redirected, nothing to do */
    return 0;
  }

  /* Flush the current stdout file descriptor */
  fflush(stdout);

  /* Restrore the original stdout */
  copy = dup2(stdout_backup, STDOUT_FILENO);
  if (copy < 0) {
    return -1;
  }
  close(stdout_backup);
  
  stdout_backup = 0;
  
  return copy;
}

int ConsoleCapture::redirect_stdout(void)
{
  int file;

  if (stdout_backup > 0) {
    /* console was already redirected */
    return stdout_backup;
  }
  
  stdout_backup = dup(STDOUT_FILENO);
  if (stdout_backup < 0) {
    fprintf(stdout, "Failed to create a duplicate descriptor for stdout\n");
    return -1;
  }
  

  /* Create a temporary file to redirect stdout */
  file = mkostemp(filename, O_APPEND | O_WRONLY);
  if (file < 0) {
    fprintf(stdout, "Failed to create temporary file %s\n", filename);
    close(stdout_backup);
    stdout_backup = 0;
    return -1;
  }
           

  /* flush the stdout */
  fflush(stdout);

  /* dup the file onto stdout */
  if (dup2(file, STDOUT_FILENO) < 0) {
    fprintf(stdout, "Failed to dup the new file descriptor onto stdout\n");
    close(stdout_backup);
    stdout_backup = 0;
    close(file);
    return -1;
  }

  close(file);

  return stdout_backup;
}

int ConsoleCapture::start_stdout_capture(void)
{
  capture_str = std::string("");
  stdout_backup = 0;
  snprintf(filename, MAX_FILENAME_SZ, "stdout.XXXXXX");
  return redirect_stdout();
}

int ConsoleCapture::stop_stdout_capture(void)
{
  /* restore the stdout to console */
  restore_stdout();

  /* Read the captured content from file into a string */
  std::ifstream in(filename);
  std::stringstream buffer;

  buffer << in.rdbuf();
  capture_str = std::string(buffer.str());

  /* remove the temporary file */
  remove(filename);
  snprintf(filename, MAX_FILENAME_SZ, "stdout.XXXXXX");

  return 0;
}

std::string ConsoleCapture::get_capture_string()
{
  stop_stdout_capture();
  return capture_str;
}
