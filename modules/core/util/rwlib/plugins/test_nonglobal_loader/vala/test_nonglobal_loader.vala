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

// This interface used test non-global loaders, one thread will call
// add with a long sleep interval and the other thread will call with
// sero sleep interval.
//
// If using the nonglobal loaders, the thread calling add with sleep
// shouldn't block the thread calling add without sleep.
namespace TestNonglobalLoader {

  public interface Api: GLib.Object {
    public abstract int add(int secs, int a, int b);
  }
}
