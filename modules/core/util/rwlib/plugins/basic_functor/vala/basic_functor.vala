// This is example illustrates the use of functor objects
// through the plugin interface. The functor objects are 
// used for implementing callbacks.

namespace BasicFunctor {

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
    public abstract int callback_example(Functor functor);
  }
}
