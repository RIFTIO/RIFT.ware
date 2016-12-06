/**
 * This Vala specification defines a common "plugin" interface API that 
 * all plugins should provide.
 *
 * This is a .vala file which is used to define a 
 * GInterface using the GObjectIntrospection mechanism.
 *
 * Every plugin belongs to a namespace that matches the names of 
 * the .gir and .typelib files
 */
namespace Rwplugin {

 /**
  * Type used to declare the direction on an interface
  */
  public enum PinDirection {
   UNKNOWN  = 0,
   INPUT    = 1, /* i.e. Module Instance needs this interface (API)    */
   OUTPUT   = 2, /* i.e. Module instance provides this interface (API) */
  }

  /**
   * This is the interface we will use in the RiftCommonInterface library
   */
  public interface Api: GLib.Object {
    /**
     * A "Rwplugin" plugin supports the creation of MODULEs.
     * In turn, each module can provide multiple MODULE INSTANCES.
     *
     * The "Rwplugin" API to request the plugin to allocate/init 
     * a module (Conceptually, think of this as starting a driver).
     *
     * NOTE: The implementation of module_init()/module_deinit() 
     * should support being called multiple times per plugin 
     * (where appropriate)
     *
     * Putting the function into an interface causes GObject to recognize 
     * it as a virtual method.
     *
     * Note that it is a good idea to use a prefix on each method name 
     * such as "module" to avoid method name collision in classes that 
     * inherit this interface definition. For example, if we define an 
     * interface derived RiftCommonInterface.Api (such as StandardPlugin.Api)
     * it is not allowed to also have a method with the name "module_init" 
     * (as defined below).
     * In summary, always use unique method names (even across different interface definitions)
     */

    /**
     * Initializes the module
     * 
     * @param module_name  Name of the module.
     * @param alloc_config A configuration string provided to assist with the allocation.
     *
     * @return GLib.Object Handle to the module.
     */
    public abstract GLib.Object 
    module_init(string module_name, string alloc_config);

    /**
     * module_deinit
     * 
     * @param module_handle Handle to the module obtained by calling module_init()
     */
    public abstract void
    module_deinit(GLib.Object module_handle);

    /*
     * The "Common Interface" to allocate a MODULE INSTANCE with 
     * respect to a MODULE.  (Conceptually, think of this as opening a 
     * minor device wrt a driver)
     *
     * @param module_handle Handle to the module obtained from module_init.
     * @param module_instance_name Name of the module instance.
     * @param alloc_config A configuration string provided to assist with the allocation.
     */
    public abstract GLib.Object 
    module_instance_alloc(GLib.Object module_handle, 
                          string module_instance_name, 
                          string alloc_config);

    /**
     * module_instance_free
     *
     * @param module_instance_handle Handle to module instance obtained during
     * the call to module_instance_alloc.
     */
    public abstract void
    module_instance_free(GLib.Object module_instance_handle);

    /**
     * module_instance_prelink
     *
     * @param module_instance_handle Handle to the module instance.
     * @param linkid linkid (useful to correlate multiple link operations (e.g. upper/lower mux links)
     * @param iface_provider GObject interface provider
     * @param iface_mih GObject module instance handle
     * @param apitype The type # of the API
     * @param apistr The name of the API
     */
    public abstract bool        
    module_instance_prelink_check(GLib.Object module_instance_handle, 
                                  uint32 linkid, 
                                  GLib.Object iface_provider, 
                                  GLib.Object iface_mih, 
                                  uint32 apitype, 
                                  string apistr);

    /**
     * module_instance_prelink
     *
     * @param module_instance_handle Handle to the module instance.
     * @param linkid linkid (useful to correlate multiple link operations (e.g. upper/lower mux links)
     * @param iface_provider GObject interface provider
     * @param iface_mih GObject module instance handle
     * @param apitype The type # of the API
     * @param apistr The name of the API
     */
    public abstract void        
    module_instance_prelink(GLib.Object module_instance_handle, 
                            uint32 linkid, 
                            GLib.Object iface_provider, 
                            GLib.Object iface_mih, 
                            uint32 apitype, 
                            string apistr);

    /**
     * module_instance_postlink
     *
     * @param module_instance_handle Handle to the module instance.
     * @param linkid linkid (useful to correlate multiple link operations (e.g. upper/lower mux links)
     * @param iface_provider GObject interface provider
     * @param iface_mih GObject module instance handle
     * @param apitype The type # of the API
     * @param apistr The name of the API
     */
    public abstract void        
    module_instance_postlink(GLib.Object module_instance_handle, 
                             uint32 linkid, 
                             GLib.Object iface_provider, 
                             GLib.Object iface_mih, 
                             uint32 apitype, 
                             string apistr);

    /**
     * module_instance_preunlink
     *
     * @param module_instance_handle Handle to the module instance.
     * @param linkid linkid (useful to correlate multiple link operations (e.g. upper/lower mux links)
     * @param iface_provider GObject interface provider
     * @param iface_mih GObject module instance handle
     * @param apitype The type # of the API
     * @param apistr The name of the API
     */
    public abstract void        
    module_instance_preunlink(GLib.Object module_instance_handle, 
                              uint32 linkid, 
                              GLib.Object iface_provider, 
                              GLib.Object iface_mih, 
                              uint32 apitype, 
                              string apistr);

    /**
     * module_instance_postunlink
     *
     * @param module_instance_handle Handle to the module instance.
     * @param linkid linkid (useful to correlate multiple link operations (e.g. upper/lower mux links)
     * @param iface_provider GObject interface provider
     * @param iface_mih GObject module instance handle
     * @param apitype The type # of the API
     * @param apistr The name of the API
     */
    public abstract void
    module_instance_postunlink(GLib.Object module_instance_handle, 
                               uint32 linkid, 
                               GLib.Object iface_provider, 
                               GLib.Object iface_mih, 
                               uint32 apitype, 
                               string apistr);
  }
}
