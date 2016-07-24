/**
 * This Vala specification defines rwtypes
 */
namespace RwArrayTypes {
  /**
   * Dynamic uint32 array definition
   */
  [ CCode ( cname = "rw_dynamic_uint_array_t" ) ]
  public class DynamicUintArray {
    public GLib.Array<uint32> _storage = new GLib.Array<uint32>();

    public void append_val(owned uint32 val) {
      _storage.append_val(val);
    }

    public void append_vals(void *data, uint len) {
      _storage.append_vals(data, len);
    }

    public uint32 get(int index) {
      return (uint32) _storage.index(index);
    }

    public void set(int index, uint32 value) {
      _storage.insert_val(index, value);
    }

    public uint length() {
      return _storage.length;
    }
  }

  /**
   * Dynamic int32 array definition
   */
  [ CCode ( cname = "rw_dynamic_int_array_t" ) ]
  public class DynamicIntArray {
    public GLib.Array<int32> _storage = new GLib.Array<int32>();

    public void append_val(owned int32 val) {
      _storage.append_val(val);
    }

    public void append_vals(void *data, uint len) {
      _storage.append_vals(data, len);
    }

    public int32 get(int index) {
      return (int32) _storage.index(index);
    }

    public void set(int index, int32 value) {
      _storage.insert_val(index, value);
    }

    public uint length() {
      return _storage.length;
    }
  }

  /**
   * Dynamic byte array definition
   */
  [ CCode ( cname = "rw_dynamic_byte_array_t" ) ]
  public class DynamicByteArray {
    public GLib.Array<uint8> _storage = new GLib.Array<uint8>();

    public void append_val(owned uint8 val) {
      _storage.append_val(val);
    }

    public void append_vals(void *data, uint len) {
      _storage.append_vals(data, len);
    }

    public uint8 get(int index) {
      return (uint8) _storage.index(index);
    }

    public void set(int index, uint8 value) {
      _storage.insert_val(index, value);
    }

    public uint length() {
      return _storage.length;
    }
  }
}
