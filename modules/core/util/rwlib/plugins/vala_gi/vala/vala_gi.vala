// This is a .vala file which is used to define a 
// GInterface using the GObjectIntrospection mechanism
// This is the most basic plugin example provided.
// It contains the minimal amount of source code required 
// to create a plugin with one method


// Every plugin belongs to a namespace that matches the 
// names of the .gir and .typelib files.
//
// The following conventions make the practice of writing
// plugins consistent.
// - Always use a new vala file for each new namespace
// - Use CamelCase names for the namespace
// - The name of vala file should be uncamel case version
//   of the namespace. For example if the namespace is FooBar,
//   it should in foo_bar.vala.

namespace ValaGi {

  // This is a basic GObject class with automatic getters and setters.
  // This used to test passing the GObject across the vala interface.
  public class MyObj : GLib.Object {
    public string name {get; set;}
    public string[] string_array {get; set;}
  }

  // This is the interface we will use in the plugin
  public interface Api: GLib.Object {

    // Call this function to get a GBOX from plugin
    public abstract int get_box(out ExampleGi.Boxed boxed);

    // Call this function to have the pkugin populate 
    // the box structure
    public abstract int get_data_in_box(ExampleGi.Boxed boxed);

    // Call this function to get a GObject from plugin
    public abstract int get_my_obj(out MyObj obj);
  }
}
