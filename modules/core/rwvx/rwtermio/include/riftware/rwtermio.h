/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwtermio.h
 * @author Balaji Rajappa 
 * @date 08/25/2015
 * @brief Tasklet for providing pseudo terminal services for interactive native
 *        processes
 */


#ifndef __rwtermio_H__
#define __rwtermio_H__

#include "rwtasklet.h"
#include <rw_tasklet_plugin.h>
#include <termios.h>
#include <unistd.h>

struct rwtermio_component_s {
  CFRuntimeBase _base;
  /* ADD ADDITIONAL FIELDS HERE */
};

RW_TYPE_DECL(rwtermio_component);
RW_CF_TYPE_EXTERN(rwtermio_component_ptr_t);


struct rwtermio_instance_s {
  CFRuntimeBase _base;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwtermio_component_ptr_t component;

  /* ADD ADDITIONAL FIELDS HERE */
  int master_fd;
  struct termios orig_term_settings;

  int slave_inotify;
  int slave_watch_handle;

  bool under_gdb;
};

RW_TYPE_DECL(rwtermio_instance);
RW_CF_TYPE_EXTERN(rwtermio_instance_ptr_t);

rwtermio_component_ptr_t rwtermio_component_init(void);

void rwtermio_component_deinit(rwtermio_component_ptr_t component);

rwtermio_instance_ptr_t rwtermio_instance_alloc(
        rwtermio_component_ptr_t component, 
        struct rwtasklet_info_s *rwtasklet_info, 
        RwTaskletPlugin_RWExecURL *instance_url);

void rwtermio_instance_free(
        rwtermio_component_ptr_t component, 
        rwtermio_instance_ptr_t instance);

void rwtermio_instance_start(
        rwtermio_component_ptr_t component, 
        rwtermio_instance_ptr_t instance);

#endif //__rwtermio_H__

