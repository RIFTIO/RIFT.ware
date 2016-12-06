/*!
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
 * Creation Date: 9/11/15
 * 
 */

#ifndef RWMEMLOGDTS_H_
#define RWMEMLOGDTS_H_

#include <rwmemlog.h>
#include <sys/cdefs.h>
#include <glib.h>
#include <glib-object.h>
#include <rwtypes_gi.h>

#ifndef __GI_SCANNER__
#include <rwtasklet.h>
#include <rwdts.h>

__BEGIN_DECLS

/*!
 * Register a memlog instance with DTS.
 */
rw_status_t rwmemlog_instance_dts_register(
  rwmemlog_instance_t* instance,
  rwtasklet_info_ptr_t tasklet_info,
  rwdts_api_t* apih );

/*!
 * Deregister a memlog instance with DTS.
 */
void rwmemlog_instance_dts_deregister(
  rwmemlog_instance_t* instance,
  bool dts_internal );
#endif

GType rwmemlog_instance_gi_get_type(void);
GType rwmemlog_buffer_gi_get_type(void);

typedef struct rwmemlog_instance_gi_t {
  void *memlog_inst; // pointer to C struct rwmemlog_instance_t declared void to avoid GI warning
  int ref_cnt;
} rwmemlog_instance_gi_t;

typedef struct rwmemlog_buffer_gi_t {
  void *memlog_buf; // pointer to C struct rwmemlog_buffer_t declared void to avoid GI warning
  rwmemlog_instance_gi_t *gi_inst;
  char *gi_object_name;
  int ref_cnt;
} rwmemlog_buffer_gi_t;

/*!
 * Allocate and initialize a new RW.Memlog instance.
 *
 * @return The new instance.  Crashes on memory alloc failure.
 */
/// @cond GI_SCANNER
/**
 * rwmemlog_instance_new : (method):
 * @ descr : (type const char *) : (transfer none):
 * @ minimum_retired_count
 * @ maximum_keep_count
 **/
/// @endcond

rwmemlog_instance_gi_t* rwmemlog_instance_new(
  /*!
   * [in] Description string.  Copied into instance, so does not need
   * to survive this call.
   */
  const char* descr,

  /*!
   * [in] Initial number of buffers to allocate.  If 0, a default value
   * will be used.
   */
  uint32_t minimum_retired_count,

  /*!
   * [in] The maximum number of buffers to keep retired.  Once this
   * limit is reached, new keeps will release older keeps back to the
   * regular retired pool.
   */
  uint32_t maximum_keep_count );




/*!
 * Get an rwmemlog buffer from the rwmemlog instnace 
 * Gi wrapper function
 * @return The newly activated buffer.
 */
/// @cond GI_SCANNER
/**
 * rwmemlog_instance_get_buffer_gi : (method):
 * @ instance
 * @ object_name : (transfer none) (nullable) (optional)
 * @ object_id   : (transfer none) (nullable) (optional)
 **/
/// @endcond
rwmemlog_buffer_gi_t* rwmemlog_instance_gi_get_buffer(
  /*!
   * [in] The memlog Gi instance got from rwmemlog_instance_gi_new
   */
  rwmemlog_instance_gi_t* gi_inst,
  /*!
   * [in] A text string to refer this buffer object
   */
  const char* object_name,
  /*!
   * [in] App handle
   */
  gpointer object_id );


/*!
 * Log the string into memlog buffer
 * Gi wrapper function
 */
/// @cond GI_SCANNER
/**
 * rwmemlog_buffer_log: (method):
 * @ buf :
 * @ string : 
 **/
/// @endcond

void rwmemlog_buffer_log(
  /*!
   * [in] The memlog gi buffer instance
   */
  rwmemlog_buffer_gi_t *buf,
  /*!
   * [in] Text string  
   */
  const char *string);

/*!
 * Print the memlog buffer contents to stdout
 * Gi wrapper function
 */
/// @cond GI_SCANNER
/**
 * rwmemlog_buffer_output: (method):
 * @ buf :
 */
/// @endcond
void rwmemlog_buffer_output(
  /*!
   * [in] memlog gi buffer 
   */
  rwmemlog_buffer_gi_t *buf);

/*!
 * Print all buffers in the memlog instance
 * Gi wrapper function
 */
/// @cond GI_SCANNER
 /**
  * rwmemlog_instance_output: (method)
  * @ inst:
  **/ 
/// @endcond
void rwmemlog_instance_output(
  /*!
   * [in] memlog instance
   */
  rwmemlog_instance_gi_t *inst);

/*!
 * stringize the memlog buffer and return the string
 */
/// @cond GI_SCANNER
 /**
  * rwmemlog_buffer_string: (method)
  * @ buf:
  * Returns: (transfer full):
  **/ 
/// @endcond
char * rwmemlog_buffer_string(
  /*!
   * [in] buffer
   */
  rwmemlog_buffer_gi_t *buf);

/*!
 * stringize the memlog instance contents
 */
/// @cond GI_SCANNER
 /**
  * rwmemlog_instance_string: (method)
  * @ inst:
  * Returns: (transfer full):
  **/ 
/// @endcond
char * rwmemlog_instance_string(
  /*!
   * [in] inst
   */
  rwmemlog_instance_gi_t *inst);


__END_DECLS

#endif /* ndef RWMEMLOGDTS_H_ */
