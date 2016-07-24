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

namespace LuaCoroutine {


  public delegate int callback_prototype();

  public class Functor: GLib.Object {
    // remember to set owned tag on set.
    // http://valajournal.blogspot.com/2011/06/vala-0130-released.html
    public callback_prototype function {get; owned set;}
    public int callback() {
      return function();
    }
  }

  // This is the interface we will use in the plugin
  public interface Api: GLib.Object {
    // this function yields in the middle and returns a functor to callback
    public abstract int add_coroutine_wrap(int a, int b, out Functor functor);
    public abstract int add_coroutine_create(int a, int b, out Functor functor);
  }
}
