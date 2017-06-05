/* STANDARD_RIFT_IO_COPYRIGHT */

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
