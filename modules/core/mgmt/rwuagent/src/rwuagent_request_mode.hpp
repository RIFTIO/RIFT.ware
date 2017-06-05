/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwuagent_request_mode.hpp
 *
 * Management agent southbound request type;
 */

#ifndef CORE_MGMT_RWUAGENT_REQUEST_MODE_HPP_
#define CORE_MGMT_RWUAGENT_REQUEST_MODE_HPP_

namespace rw_uagent {

/*!
 * 
 */
enum class RequestMode
{
  CONFD = (1u << 0),
  XML = (1u << 1),
  PBRAW = (1u << 2)
};

}
#endif
