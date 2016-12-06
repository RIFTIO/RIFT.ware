// This file provides a simple example of how a callback from
// the plugin can be asyncronously dispatched.

namespace AsyncCb {

  public delegate void dispatch_func(AsyncFunctorInstance functor);
  public delegate void callback_func(AsyncFunctorInstance functor);

  public interface Functor: GLib.Object {
    public abstract void invoke_callback();
    public abstract void dispatch();
  }

  // This class is used to illustrate passing a gobject
  public class AsyncFunctorInstance: GLib.Object, Functor {
    // without the get/set mechanism on delegate
    // python code will have issues
    // introspecting this object
    public callback_func _callback_method {get; set;}
    public dispatch_func _dispatch_method {get; set;}
    public void *context;
    public uint int_variable;
    public string string_variable;

    // To manipulate string arrays in python 
    // get/set mechanism is needed.
    public string[] string_array {get; set;}

    // After the functor is dequeued this method will be invoked
    public void invoke_callback() {
      _callback_method(this);
    }

    // Plugins call this function to dispatch the functor on to queue
    public void dispatch() {
      _dispatch_method(this);
    }
  }

  // This is the interface we will use in the plugin
  public interface Api: GLib.Object {
    public abstract RwTypes.RwStatus async_request(AsyncFunctorInstance cb);
  }
}
