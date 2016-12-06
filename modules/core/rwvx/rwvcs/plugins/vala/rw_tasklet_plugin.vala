
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
 * @file rwtasklet_plugin.vala
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 03/10/2014
 * @brief RW.Tasklet VALA interface for tasklet components
 * 
 * @abstract
 * This Vala specification defines the RW.Tasklet interface API that all Riftware tasklets implement.
 */

// Every plugin belongs to a namespace that matches the names of the .gir and .typelib files

namespace RwTaskletPlugin {
  /**
   * Declare the handle types used by this interface
   * These are backed by plugin specific stuructures in the plugin implementation
   */
  public class _RWExecURL : GLib.Object {
  }

  public class ComponentHandle: GLib.Object {
  }

  public class InstanceHandle: GLib.Object {
  }

  /**
   * This is the interface that a tasklet plugin implements
   */

  public interface Component: GLib.Object {

    /**
     * init
     *
     * Initializes the rwtasklet component
     * 
     * @return GLib.Object
     * Handle to the rwtasklet component
     */

    public abstract ComponentHandle
    component_init();

    /**
     * component_deinit
     *
     * Unregisters the rwtasklet component
     * 
     * @param component_handle
     * Handle to the rwtasklet component obtained from component_init()
     *
     * @return None
     */

    public abstract void
    component_deinit(ComponentHandle h_component);

    /**
     * instance_alloc
     * 
     * Create a new rwtasklet instance
     *
     * @param h_component
     * Handle to the rwtasklet component obtained from component_init()
     *
     * @param instance_url
     * URL of the rwtasklet instance
     *
     * @return GLib.Object
     * Handle for the newly created rwtasklet
     */

     public abstract InstanceHandle
     instance_alloc(ComponentHandle h_component,
		    owned RwTasklet.Info tasklet_info,
		    _RWExecURL instance_url);

     public abstract void
     instance_free(ComponentHandle h_component,
		   InstanceHandle h_instance);

     public abstract void
     instance_start(ComponentHandle h_component,
		    InstanceHandle h_instance);

     public abstract void
     instance_stop(ComponentHandle h_component,
		    InstanceHandle h_instance);
  }

}
