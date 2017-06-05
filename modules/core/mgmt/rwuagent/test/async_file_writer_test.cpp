/* STANDARD_RW_IO_COPYRIGHT */

/**
 * @file rw_xml_validation_test.cpp
 * @brief Tests for xml dom validation
 */

#include <atomic>
#include <string>
#include <sstream>
#include <list>
#include <fstream>

#include <boost/algorithm/string.hpp>
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS


#include <rwsched.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwtasklet.h>
#include <rwtrace.h>
#include <rwut.h>
#include <rwyangutil.h>

#include "async_file_writer.hpp"

namespace fs = boost::filesystem;

using namespace rw_uagent;

class AsyncWriterTest : public ::testing::Test {
 protected:
  rwsched_instance_ptr_t rwsched_;
  rwsched_tasklet_ptr_t tasklet_;
  rwsched_dispatch_queue_t queue_;

  std::string test_file_;
  AsyncFileWriter *writer_;

  int write_count_ = 0;

  std::atomic<bool> running_;

  std::list<std::string> lines_;

  void delete_test_file()
  {
    fs::remove(test_file_);
  }

  virtual void SetUp() {

    rwsched_ = rwsched_instance_new();
    tasklet_ = rwsched_tasklet_new(rwsched_);
    test_file_ = rwyangutil::get_rift_artifacts() + "/async_file_writer_test_log_file.txt";
    queue_ = rwsched_dispatch_queue_create(
        tasklet_,
        "AsyncFileWriterTestQueue",
        RWSCHED_DISPATCH_QUEUE_SERIAL);
    writer_ = new AsyncFileWriter(test_file_, queue_, rwsched_, tasklet_);
  }

  virtual void TearDown() {
    delete writer_;
    rwsched_tasklet_free(tasklet_);
    rwsched_instance_free(rwsched_);

    delete_test_file();
  }

  static void write_stamped_message(void * context)
  {
    AsyncWriterTest * this_ptr = reinterpret_cast<AsyncWriterTest *>(context);
    RW_ASSERT(this_ptr);

    std::stringstream message;
    message << "stamp: " << this_ptr->write_count_ << std::endl;
    std::string const message_str = message.str();

    this_ptr->lines_.push_back(message_str);
    this_ptr->writer_->write_message(message_str);

    if (this_ptr->write_count_ > 0) {
      this_ptr->write_count_--;
      
      rwsched_dispatch_async_f(this_ptr->tasklet_,
                               this_ptr->queue_,
                               this_ptr,
                               AsyncWriterTest::write_stamped_message);
    } else {
      this_ptr->running_ = false;
    }
  }


};

TEST_F(AsyncWriterTest, test_write_lines)
{
  // test-specific setup
  running_ = true;
  write_count_ = 10;

  // setup producer
  rwsched_dispatch_async_f(tasklet_,
                           queue_,
                           this,
                           AsyncWriterTest::write_stamped_message);
  
  rwsched_dispatch_main_until(tasklet_, .1/*seconds*/, NULL);

  // wait for everything to run
  while(running_ == true){
    ;
  }
  
  // read log file and diff against known produced lines 
  std::ifstream logfile(test_file_);
  
  std::string line;
  
  auto known_itr = lines_.begin();

  size_t count = 0;
  while (std::getline(logfile, line)) {

    std::string expected = *known_itr;

    // getline removes the newline, so we should match that
    boost::algorithm::trim_right(expected);

    EXPECT_EQ(expected, line);

    count++;
    known_itr++;
  }
  ASSERT_EQ(count, lines_.size());


}

