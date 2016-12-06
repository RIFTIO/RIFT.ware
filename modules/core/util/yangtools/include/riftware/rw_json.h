
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
