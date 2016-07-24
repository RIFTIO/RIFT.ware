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

namespace BasicPlugin {

  // This is the interface we will use in the plugin
  public interface Api: GLib.Object {

    // This is the only method in our plugin
    // Putting the function into an interface causes 
    // GObject to recognize it as a virtual method
    // A BasicPlugin package must supply this method in the plugin software

    // This function takes two integers and returns their sum.
    public abstract int add(int a, int b);
  }
}
