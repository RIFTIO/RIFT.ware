/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwuagent_netconf_error.cpp
 *
 * NETCONF error support
 */

#include "rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;


NetconfError::NetconfError(
  RwNetconf_ErrorType errType,
  IetfNetconf_ErrorTagType errTag )
{
  RWPB_F_MSG_INIT( RwMgmtagt_data_Uagent_LastError_RpcError, &error_ );
  set_type(errType);
  set_tag(errTag);
}

NetconfError::~NetconfError()
{
  protobuf_c_message_free_unpacked_usebody( nullptr, &error_.base );
}

NetconfError& NetconfError::set_rw_error_tag(
  rw_yang_netconf_op_status_t status )
{
  if (status >= RW_YANG_NETCONF_OP_STATUS_IN_USE &&
      status <= RW_YANG_NETCONF_OP_STATUS_MALFORMED_MESSAGE) {
    set_tag((IetfNetconf_ErrorTagType)(status - RW_YANG_NETCONF_OP_STATUS_IN_USE));
  }
  else {
    RW_ASSERT(status != RW_YANG_NETCONF_OP_STATUS_OK);
    set_tag(IETF_NETCONF_ERROR_TAG_TYPE_OPERATION_FAILED);
  }

  return *this;
}

NetconfError& NetconfError::set_rw_error_tag(
  rw_status_t rwstatus)
{
  rw_yang_netconf_op_status_t status = RW_YANG_NETCONF_OP_STATUS_OK;
  switch (rwstatus) {
    case RW_STATUS_SUCCESS:
      status = RW_YANG_NETCONF_OP_STATUS_OK;
      break;
    case RW_STATUS_NOTFOUND:
      status =  RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
      break;
    case RW_STATUS_OUT_OF_BOUNDS:
      status = RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT;
      break;
    case RW_STATUS_FAILURE:
    default:
      status = RW_YANG_NETCONF_OP_STATUS_FAILED;
      break;
  }
  return set_rw_error_tag(status);
}

rw_yang_netconf_op_status_t NetconfError::get_rw_nc_error_tag() const
{
  rw_yang_netconf_op_status_t status = RW_YANG_NETCONF_OP_STATUS_FAILED;
  if (error_.has_error_tag_type) {
    status = static_cast<rw_yang_netconf_op_status_t>(static_cast<int>(error_.error_tag_type) 
            + static_cast<int>(RW_YANG_NETCONF_OP_STATUS_IN_USE));
  }
  return status;
}

NetconfError& NetconfError::set_type(RwNetconf_ErrorType type)
{
  error_.has_error_type = 1;
  error_.error_type = type;
  return *this;
}

NetconfError& NetconfError::set_tag(IetfNetconf_ErrorTagType tag)
{
  error_.has_error_tag_type = 1;
  error_.error_tag_type = tag;
  return *this;
}

IetfNetconf_ErrorTagType NetconfError::get_tag() const
{
  if (error_.has_error_tag_type) {
    return error_.error_tag_type;
  }
  return IETF_NETCONF_ERROR_TAG_TYPE_OPERATION_FAILED;
}

NetconfError& NetconfError::set_severity(IetfNetconf_ErrorSeverityType severity)
{
  error_.has_error_severity_type = 1;
  error_.error_severity_type = severity;
  return *this;
}

NetconfError& NetconfError::set_app_tag(const char* app_tag)
{
  if (error_.error_app_tag) {
    free(error_.error_app_tag);
  }
  error_.error_app_tag = strdup(app_tag);
  return *this;
}

const char* NetconfError::get_app_tag() const
{
  return error_.error_app_tag;
}

NetconfError& NetconfError::set_error_path(const char* err_path)
{
  if (error_.error_path) {
    free(error_.error_path);
  }
  error_.error_path = strdup(err_path);
  return *this;
}

const char* NetconfError::get_error_path() const
{
  if (error_.error_path) {
    return error_.error_path;
  }
  return NULL;
}

NetconfError& NetconfError::set_error_message(const char* err_msg)
{
  if (error_.error_message) {
    free(error_.error_message);
  }
  error_.error_message = strdup(err_msg);
  return *this;
}

NetconfError& NetconfError::set_errno(const char* sysc)
{
  if (error_.error_message) {
    free(error_.error_message);
  }
  std::stringstream errm;
  errm << sysc << " failure, error = " << strerror(errno);
  error_.error_message = strdup(errm.str().c_str());
  return *this;
}

const char* NetconfError::get_error_message() const
{
  if (error_.error_message) {
    return error_.error_message;
  }
  return NULL;
}

NetconfError& NetconfError::set_error_info(const char* err_info)
{
  if (error_.error_info) {
    free(error_.error_info);
  }
  error_.error_info = strdup(err_info);
  return *this;
}

IetfNetconfErrorReply* NetconfError::get_pb_error()
{
  return &error_;
}


NetconfErrorList::NetconfErrorList()
{
}

NetconfError& NetconfErrorList::add_error(
  RwNetconf_ErrorType errType,
  IetfNetconf_ErrorTagType errTag )
{
  errors_.emplace_back( errType, errTag );
  return errors_.back();
}

/* ATTN: Do NOT pass out args by mutable reference! */
bool NetconfErrorList::to_xml(
  rw_yang::XMLManager* xml_mgr,
  std::string& xml )
{
  IetfNetconfErrorReply* rpc_errors[errors_.size()];
  RWPB_T_MSG(RwMgmtagt_data_Uagent_LastError) reply;

  memset(rpc_errors, 0, sizeof(IetfNetconfErrorReply*) * errors_.size());
  rw_mgmtagt__yang_data__rw_mgmtagt__uagent__last_error__init(&reply);

  reply.n_rpc_error = errors_.size();
  reply.rpc_error = rpc_errors;

  size_t idx = 0;
  for (auto& error : errors_) {
    reply.rpc_error[idx++] = error.get_pb_error();
  }

  rw_status_t ret = xml_mgr->pb_to_xml_unrooted(&(reply.base), xml);

  return (ret == RW_STATUS_SUCCESS);
}

const std::list<NetconfError>& NetconfErrorList::get_errors() const
{
  return errors_;
}
