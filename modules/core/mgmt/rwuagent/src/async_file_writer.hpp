
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file async_file_writer.hpp
 * @date 2016/05/09
 * @brief Allows async file writing
 */

#ifndef RW_ASYNC_FILE_WRITER_HPP
#define RW_ASYNC_FILE_WRITER_HPP

#include <fstream>
#include <string>
#include <memory>

#include <rwsched.h>
#include <rwsched_queue.h>

namespace rw_uagent {

/**
 * This class provides a way of asynchronously writing to a file. 
 */
class AsyncFileWriter
{
 private:
  std::ofstream file_;

  rwsched_dispatch_source_t source_;
  int pipefd_[2];

  rwsched_dispatch_queue_t queue_;
  rwsched_instance_ptr_t rwsched_;
  rwsched_tasklet_ptr_t tasklet_;

  /**
   * Reads a payload from the socket and writes it to a file.
   */
  void read_socket_and_write_file();

  /**
   * Callback when socket has data ready to read.
   */
  static void data_ready(void *info);

  // not copyable
  AsyncFileWriter(AsyncFileWriter const &) = delete;
  void operator=(AsyncFileWriter const &) = delete;

  // not default-constructable
  AsyncFileWriter() = delete;

 public:
  AsyncFileWriter(std::string const &,
                  rwsched_dispatch_queue_t,
                  rwsched_instance_ptr_t,
                  rwsched_tasklet_ptr_t);

  ~AsyncFileWriter();

  /**
   * Asynchronously write the message.
   */
  void write_message(std::string const &);
};

/**
 * Interface for factories that create AsyncFileWriters
 */
class AsyncFileWriterFactory
{
 public:

  virtual ~AsyncFileWriterFactory() {}

  virtual std::unique_ptr<AsyncFileWriter> create_async_file_writer(std::string const & filename) = 0;

};


}
#endif
