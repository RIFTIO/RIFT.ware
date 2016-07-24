
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_json.hpp
 * @author Sameer Nayak
 * @date 29/07/2015
 * @brief Json to pb api
 */

#ifndef RW_JSON_HPP_
#define RW_JSON_HPP_

#include "rwlib.h"
#include "yangmodel.h"
#include "rw_keyspec.h"
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#define RW_JSON_STR_MAX_SIZE 2048

__BEGIN_DECLS

int rw_json_pbcm_to_json(const ProtobufCMessage *msg,
                         const ProtobufCMessageDescriptor *desc,
                         char **out_string);
__END_DECLS
/** @} */

#endif /*RW_JSON_HPP_*/
