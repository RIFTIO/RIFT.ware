/*
 * This Vala specification provides an example "standard" plugin interface specification
 * that supports Rwplugin.Api as well as the plugin-specific
 * methods add(), add3(), and invoke()
 *
 * A StandardPlugin package must supply these method in the actual language-specific
 * plugin software implementation (C, Python, or JavaScript)
 *
 * This is a .vala file which is used to define a GInterface using the GObjectIntrospection mechanism
 * Every plugin belongs to a namespace that matches the names of the .gir and .typelib files
 */

namespace StandardPlugin {

  public class module : GLib.Object {
    public string name;
    public string alloc_cfg;
  }

  public class moduleInstance : GLib.Object {
    public string name;
    public string alloc_cfg;
  }

  // This is the interface we will use in the plugin
  public interface Api: Rwplugin.Api, GLib.Object {
    // Putting the function into an interface causes GObject to recognize it as a virtual method

    // A few sample methods - note that these are not module or module-instance specific
    public abstract int add(int a, int b);
    public abstract void add3(int a, int b, out int c);

    // Sample methods that is module-specific
    // "module" is allocated via Rwplugin.Api (see common_interface.vala)
    public abstract void invoke_module(GLib.Object module);

    // Sample methods that is module-instance-specific
    // "module_instance" is allocated via Rwplugin.Api (see common_interface.vala)
    public abstract void invoke_module_instance(GLib.Object module_instance);
  }
}
