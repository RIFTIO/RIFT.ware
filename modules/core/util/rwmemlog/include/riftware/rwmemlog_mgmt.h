
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file rwmemlog_mgmt.h
 * @date 2015/09/16
 * @brief In-memory logging management interfaces.
 */

#ifndef RW_RWMEMLOG_MGMT_H_
#define RW_RWMEMLOG_MGMT_H_

#include <sys/cdefs.h>
#include <rwmemlog.h>
#include <rw-memlog.pb-c.h>

__BEGIN_DECLS

/*
 * Output tracking structure.  Used to keep output handling functions
 * concise.
 */
struct rwmemlog_mgmt_output_t
{
  rw_keyspec_instance_t* ks_instance;
  ProtobufCInstance* pbc_instance;

  rwmemlog_instance_t* instance;
  RWPB_T_MSG(RwMemlog_InstanceState)* instance_msg;

  const rwmemlog_buffer_t* buffer;
  RWPB_T_MSG(RwMemlog_InstanceState_Buffer)* buffer_msg;

  uint32_t entry_index;
  const rwmemlog_entry_t* entry;
  RWPB_T_MSG(RwMemlog_InstanceState_Buffer_Entry)* entry_msg;

  rwmemlog_output_t args_output;
};


/*!
 * Convert an instance to a message, for output through the management
 * interfaces.
 *
 * @return
 *  - RW_STATUS_SUCCESS: message created successfully.
 *  - RW_STATUS_FAILURE: message not created successfully.
 */
rw_status_t rwmemlog_instance_get_output(
  /*!
   * [in] The instance to output.
   */
  rwmemlog_instance_t* instance,
  /*!
   * [in] The requested keyspec.  Used to limit the amount of data
   * actually generated.
   */
  const rw_keyspec_path_t* input_ks,
  /*!
   * [out] The output keyspec.  Will match output_data; might not be
   * equal to input_ks.  Will be valid only on RW_STATUS_SUCCESS.
   */
  rw_keyspec_path_t** output_ks,
  /*!
   * [out] The output data.  Will be valid only on RW_STATUS_SUCCESS.
   */
  RWPB_T_MSG(RwMemlog_InstanceState)** output_msg
);


/* ATTN: Doxy */
rw_status_t rwmemlog_instance_config_validate(
  rwmemlog_instance_t* instance,
  RWPB_T_MSG(RwMemlog_InstanceConfig)* config,
  char* error,
  size_t errorlen);

/* ATTN: Doxy */
rw_status_t rwmemlog_instance_config_apply(
  rwmemlog_instance_t* instance,
  RWPB_T_MSG(RwMemlog_InstanceConfig)* config);

/* ATTN: Doxy */
rw_status_t rwmemlog_instance_command(
  rwmemlog_instance_t* instance,
  RWPB_T_MSG(RwMemlog_Command)* command);



/* ATTN: Doxy */
void rwmemlog_instance_to_stdout(
  rwmemlog_instance_t* instance );

/* ATTN: Doxy */
void rwmemlog_buffer_to_stdout(
  rwmemlog_buffer_t* buffer );

/* ATTN: Doxy */
void rwmemlog_buffer_to_stdout_with_chain(
  rwmemlog_buffer_t* buffer );

/* ATTN: Doxy */
void rwmemlog_buffer_to_stdout_with_retired(
  rwmemlog_buffer_t* buffer );


__END_DECLS


#endif /* RW_RWMEMLOG_MGMT_H_ */
