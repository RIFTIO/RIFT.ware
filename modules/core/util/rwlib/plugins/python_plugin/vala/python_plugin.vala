/*
 * Unit test support utilities for python plugins
 *
 * None of these should be used in production code.
 */
namespace PythonPlugin {

  /*
   * Interface for unit-testing to make sure that the difference between
   * class and instance attributes is respected by the module loading
   * framework.
   */
  public interface PythonValueStore: GLib.Object {
    public abstract string get_class_value(string key);
    public abstract void set_class_value(string key, string value);

    public abstract string get_instance_value(string key);
    public abstract void set_instance_value(string key, string value);
  }
}
