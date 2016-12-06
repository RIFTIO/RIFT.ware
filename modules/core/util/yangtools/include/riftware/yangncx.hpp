
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
 * @file yangncx.hpp
 *
 * Yang model based on libncx.
 */

#ifndef RW_YANGTOOLS_YANGNCX_HPP_
#define RW_YANGTOOLS_YANGNCX_HPP_

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#ifndef RW_YANGTOOLS_YANGMODEL_H_
#include "yangmodel.h"
#endif

namespace rw_yang {

class YangModelNcx;

/**
 * @ingroup YangModel
 * @{
 */

/**
 * Abstract yang model based on Yuma libncx.
 */
class YangModelNcx
: public YangModel
{
public:
  YangModelNcx() {}
  ~YangModelNcx() {}

  // Cannot copy
  YangModelNcx(const YangModelNcx&) = delete;
  YangModelNcx& operator=(const YangModelNcx&) = delete;

public:
  static YangModelNcx* create_model(void *trace_ctx=nullptr);
};

} /* namespace rw_yang */

#endif // RW_YANGTOOLS_YANGNCX_HPP_

/** @} */
