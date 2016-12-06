
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
 *
 */



/**
 * @file rwlogd.c
 * @author Atogenerated
 * @date 11/18/2014
 * @brief Add description here
 */

#include "rwvcs.h"
#include "rwtrace.h"
#include "rwlog.h"
#include "rwmsg.h"
#include "rwlogd.pb-c.h"


void rwlogd_rx_loop();
rwlogd_component_ptr_t rwlogd_component_init(void) 
{
  rwlogd_component_ptr_t *component = NULL;

  /* ADD CODE HERE */

  return compnent;
}

void rwlogd_component_deinit(rwlogd_component_ptr_t component)
{
  /* ADD CODE HERE */
  return;
}

rwlogd_instance_ptr_t rwlogd_instance_alloc(
        rwlogd_component_ptr_t component, 
        struct rwtasklet_info_s * rwtasklet_info, 
        RwTaskletPlugin_RWExecURL *instance_url)
{
  rwlogd_instance_ptr_t *instance = NULL;
  rwlogd_rx_loop();

  /* ADD CODE HERE */

  return instance;
}

void rwlogd_instance_free(
        rwlogd_component_ptr_t component, 
        rwlogd_instance_ptr_t instance)
{
  /* ADD CODE HERE */
  return;
}

void rwlogd_instance_start(
        rwlogd_component_ptr_t component, 
        rwlogd_instance_ptr_t instance)
{
  /* ADD CODE HERE */
  return;
}

