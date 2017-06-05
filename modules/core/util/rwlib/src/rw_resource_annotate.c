/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_resource_track.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/13/2014
 * @brief Memory-Resource Annotation macros
 * @details RIFT Memory-Resource Annotation macros
 */
#include "rw_resource_annotate.h"

addr_nd_annot_t *a_a_a_h;
addr_nd_annot_t *a_a_a_l = NULL;
pthread_mutex_t res_annot_mutex = PTHREAD_MUTEX_INITIALIZER;
