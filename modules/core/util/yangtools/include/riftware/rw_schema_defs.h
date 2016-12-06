
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



/*!
 * @file rw_schema_defs.h
 * @date 2016/04/20
 * @brief RIFT.ware schema directory structure definitions.
 */

#ifndef RW_SCHEMA_DEFS_H_
#define RW_SCHEMA_DEFS_H_

#define RW_INSTALL_YANG_PATH ("usr/data/yang")
#define RW_INSTALL_MANIFEST_PATH ("usr/data/manifest")

#define RW_SCHEMA_ROOT_PATH ("schema")
#define RW_SCHEMA_LOCK_PATH ("schema/lock")
#define RW_SCHEMA_TMP_PATH ("schema/tmp")
#define RW_SCHEMA_TMP_ALL_PATH ("schema/tmp/all")
#define RW_SCHEMA_TMP_NB_PATH ("schema/tmp/northbound")
#define RW_SCHEMA_VER_PATH ("schema/version")
#define RW_SCHEMA_VER_LATEST_PATH ("schema/version/latest")
#define RW_SCHEMA_VER_LATEST_ALL_PATH ("schema/version/latest/all")
#define RW_SCHEMA_VER_LATEST_NB_PATH ("schema/version/latest/northbound")

#define RW_SCHEMA_INIT_TMP_PREFIX ("tmp.")

#define RW_SCHEMA_MGMT_TEST_PREFIX ("unique_ws.")
#define RW_SCHEMA_MGMT_PERSIST_PREFIX ("persist.")
#define RW_SCHEMA_MGMT_ARCHIVE_PREFIX ("ar_persist.")
#define RW_SCHEMA_CONFD_PROTOTYPE_CONF ("etc/rw_confd_prototype.conf")

#define RW_SCHEMA_META_FILE_SUFFIX (".meta_info.txt")

#define RW_SCHEMA_LOCK_FILENAME ("schema.lock")

#endif /* RW_SCHEMA_DEFS_H_ */
