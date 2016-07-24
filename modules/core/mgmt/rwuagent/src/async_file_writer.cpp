
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file async_file_writer.cpp
 * @date 2016/05/09
 * @brief Allows async file writing
 */

#include <cstring> // for memset
#include <fstream>
#include <sstream>
#include <string>
#include <errno.h>
#include <string.h>

#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_source.h"
#include "rwsched_internal.h"

#include "async_file_writer.hpp"

namespace rw_uagent {

AsyncFileWriter::AsyncFileWriter(std::string const & filename,
                                 rwsched_dispatch_queue_t queue,
                                 rwsched_instance_ptr_t rwsched,
                                 rwsched_tasklet_ptr_t tasklet)
    : file_(),      
      queue_(queue),
      rwsched_(rwsched),
      tasklet_(tasklet)
{
  int rc = ::pipe2(pipefd_, O_NONBLOCK);
  RW_ASSERT(rc == 0);

  source_ = rwsched_dispatch_source_create(tasklet_,
                                             RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                             pipefd_[0],
                                             0,
                                             queue_);
  rwsched_dispatch_resume(tasklet_, source_);
  rwsched_dispatch_set_context(tasklet_, source_, (void *)this);
  rwsched_dispatch_source_set_event_handler_f(tasklet_, source_, AsyncFileWriter::data_ready);

  file_.open (filename, std::ios_base::app | std::ios_base::out);
}

AsyncFileWriter::~AsyncFileWriter()
{
  close(pipefd_[0]);
  close(pipefd_[1]);
}

void AsyncFileWriter::read_socket_and_write_file()
{
  rwsched_dispatch_source_get_data(tasklet_, source_);

  std::stringstream message;
  char buffer[256];

  while (true) {
    std::memset(buffer,
                0,
                sizeof(buffer));
  
    int const size = ::read(pipefd_[0],
                            buffer,
                            255); // leave at least one byte for null terminator
    if (size <= 0) {
      break;
    } 

    message << buffer;
  }
  
  file_ << message.str();
  file_.flush();
}

void AsyncFileWriter::data_ready(void *context)                                    
{

  AsyncFileWriter * this_ptr = reinterpret_cast<AsyncFileWriter *>(context);
  RW_ASSERT(this_ptr);

  this_ptr->read_socket_and_write_file();
}

void AsyncFileWriter::write_message(std::string const & message)
{
  int rc = ::write(pipefd_[1], message.c_str(), message.size());

  if (rc < 0) {
    // ATTN: log error
  } 
}

}

