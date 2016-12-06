
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
 * @file async_file_writer.hpp
 * @date 2016/05/09
 * @brief Allows async file writing
 */

#ifndef RWUAGENT_QUEUE_MANAGER
#define RWUAGENT_QUEUE_MANAGER

#include <map>

#include <rwsched_queue.h>

namespace rw_uagent
{

enum class QueueType
{
  DefaultConcurrent,
  DefaultSerial,
  LogFileWriting,
  Main,
  SchemaLoading,
  XmlAgentEditConfig
};

/**
 * Maps queues for specific purposes to the underlying rwsched queue.
 */
class QueueManager
{
 protected:
  std::map<QueueType, rwsched_dispatch_queue_t> queues_;

  /**
   * Creates the queue associated with the given type and stores it in queues_.
   * @precondition there isnt'a a queue associated with type
   */
  virtual void create_queue(QueueType const & type) = 0;
 public:
  virtual ~QueueManager() {}

  /**
   * Removes the entry for the queue of type in queues_, and releases the queue.
   */
  virtual void release_queue(QueueType const & type) = 0;

  /**
   * Returns the queue of the given type. 
   */
  virtual rwsched_dispatch_queue_t get_queue(QueueType const & type) = 0;

};
}

#endif

