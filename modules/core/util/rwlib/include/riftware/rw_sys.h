
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __RWSYS_H__
#define __RWSYS_H__

#include <stdint.h>

#include <rwtypes.h>

__BEGIN_DECLS

#define RW_MAXIMUM_INSTANCES 64
#define RW_RESERVED_INSTANCES 4

/*!
 * Return the unique identifier for this Riftware instance within the range
 * 0 - RW_MAXIMUM_INSTANCES.  The resolution of the identifier is derived from one of the
 * following:
 *
 * - RIFT_INSTANCE_UID as defined in the environment.
 * - Current user UID.
 *
 * Instance ID's 0 - RW_RESERVED_INSTANCES are reserved for Jenkins builds and can only be
 * used by setting RIFT_INSTANCE_UID.  Automatic calculation using user UID will be in the
 * range RW_RESERVED_INSTANCES - RW_MAXIMUM_INSTANCES
 *
 * @param uid - on success, a unique identifier for the current Riftware instance.
 * @return    - RW_STATUS_SUCCESS
 *              RW_STATUS_OUT_OF_BOUNDS - if RIFT_INSTANCE_UID was defined but is outside
 *                                        the allowed range.
 */
/// @cond GI_SCANNER
/**
 * rw_instance_uid:
 * @uid: (out)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_instance_uid(uint8_t *uid);

/*!
 * Return a unique port number calculated from the base port number and the
 * current Riftware instance.  Note that this does not ensure that there will
 * not be port conflicts between different applications using the same base
 * port number.
 *
 * @param base_port   - base application port number
 * @param unique_port - on success, a unique port based on the Riftware instance.
 * @return            - RW_STATUS_SUCCESS
 *                      any return from rw_instance_uid()
 *                      RW_STATUS_FAILURE if the base_port is invalid.
 */
/// @cond GI_SCANNER
/**
 * rw_unique_port:
 * @unique_port: (out)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_unique_port(uint16_t base_port, uint16_t *unique_port);

/*!
 * Return TRUE of FALSE that tells whether the port -to- port+range-1
 * meeds to be avoided because they might be used by system processes
 *
 * @param port        - base port number
 * @param range       - range of ports to be chaecked
 * @return            - TRUE - if the port -to- port+range-1 is safe
 *                      FALSE - the port -to- port+range-1 falls under
 *                      avoid-list
 */
gboolean rw_port_in_avoid_list(uint16_t base_port, uint16_t range);


/*!
 * Return RW_STATUS_SUCCESS if the operation is successful
 *
 * @param name        - name of the variable to be set
 * @param value       - value of the variable to be set
 * @return            - RW_STATUS_SUCCESS if the operation is successful
 *                      RW_STATUS_FALURE otherwise
 */
/// @cond GI_SCANNER
/**
 * rw_setenv:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_setenv(const char *name, const char *value);

/*!
 * Return the value corresponding to the varable 'name'
 *
 * @param name        - name of the variable to be get
 * @return            - value corresponding to the varable 'name'
 *                      NULL if an error happens on variable not found
 */
const char *rw_getenv(const char *name);

/*!
 * Unset the variable with 'name'
 *
 * @param name        - name of the variable to be unset
 */
void rw_unsetenv(const char *name);

__END_DECLS

#endif
