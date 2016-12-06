
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
 * @file yangtest_common.hpp
 * @author Vinod Kamalaraj
 * @date 03/18/2015
 * @brief Test program for Generic tree iterators
 */

#ifndef __YANGTEST_COMMON_HPP_
#define __YANGTEST_COMMON_HPP_
#include "rw_ut_confd.hpp"
#include "rwut.h"
#include "yangncx.hpp"

#include <boost/scope_exit.hpp>

#include <iostream>
#include "rw_xml.h"
#include "bumpy-conversion.pb-c.h"
#include "flat-conversion.pb-c.h"
#include "other-data_rwvcs.pb-c.h"
#include "rift-cli-test.pb-c.h"
#include "test-ydom-top.pb-c.h"

RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *get_general_containers(int num_list_entries);
void create_dynamic_composite_d(const rw_yang_pb_schema_t** dynamic);

typedef UniquePtrProtobufCMessage<>::uptr_t msg_uptr_t;

rw_status_t rw_test_build_pb_flat_from_file (const char *filename,
                                             ProtobufCMessage **msg);

rw_status_t rw_test_build_pb_bumpy_from_file (const char *filename,
                                              ProtobufCMessage **msg);


std::string get_rift_root();

#endif
