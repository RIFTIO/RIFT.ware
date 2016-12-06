// This is a .vala file which is used to define a GInterface using the GObjectIntrospection mechanism
// This is the feature plugin that shows how to use the various features
// It contains numerous invocation methods and return types to clarify how to define a GObject interface
//
// Examples functions in the feature plugin interface include:
//
// Example #1a -- method that is invoked with in/out/in-out basic integer types (no-return)
// Example #1b -- method that is invoked with in/out/in-out basic integer types (with return)
// Example #1c -- method that is invoked with in/out/in-out basic string types (with return)
// Example #1d -- method that is invoked with in/out/in-out parameters basic integer array types (with return)
// Example #1e -- method that is invoked with in/out/in-out basic String types (with return)
// Example #1f -- method that is invoked with in/out/in-out basic Array types (with return)
// Example #2a -- method that is invoked with a in structure imported from this header file
// Example #2b -- method that is invoked with a out structure imported from this header file
// Example #2c -- method that is invoked with a in structure imported from a common header file
// Example #2d -- method that is invoked with a out structure imported from a common header file
// Example #3a -- method that is invoked with a in callback function that takes a single argument
// Example #4a -- method with one each of in out and in/out using structures from a common header file
// Example #4b -- method with in/out/return parameters using a stucture type from a commmon header file
// Example #4c -- method with two in types and a return type using structure from a common header file
// Example #5a -- method with one in parameter that is a null parameter with method checking for null
// Example #5b -- method with an annotation where caller allocates memory for out parameter (if this is possible)
// Example #5c -- method with an annotation where callee allocates memory for out parameter
// Example #6a -- method that is invoked with in/out/in-out static GArray types (no-return)
// Example #6b -- method that is invoked with in/out/in-out _StaticArrayInt class types (no-return)
// Example #6c -- method that is invoked with in/out/in-out _StaticArray<int> class types (no-return)
// Example #7a -- method that is invoked with in/out/in-out dynamic GArray types (no-return)
// Example #7b -- method that is invoked with in/out/in-out _DynamicArrayInt class types (no-return)
// Example #7c -- method that is invoked with in/out/in-out _DynamicArray<int> class types (no-return)

// Every plugin belongs to a namespace that matches the names of the .gir and .typelib files

namespace FeaturePlugin {
  // These structures are used by some of the example methods

  public class _ComplexNumber : GLib.Object {
    public int real;
    public int imag;
  }

  public class _StaticArrayInt: GLib.Object {
    public int _len;
    public int [] _storage;
    
    public _StaticArrayInt(int size) {
      _len = size;
      _storage = new int[size];
    }
    
    public int get(int index) {
      return _storage[index];
    }

    public void set(int index, int value) {
      _storage[index] = value;
    }
  }

  public class _StaticArray< TYPE > {
    public int _len;
    public TYPE [] _storage;

    public _StaticArray(int size) {
      _len = size;
      _storage = new TYPE[size];
    }

    public TYPE get(int index) {
      return _storage[index];
    }

    public void set(int index, TYPE value) {
      _storage[index] = value;
    }
  }

  public class _DynamicArrayInt: GLib.Object {
    public GLib.Array<int> _storage = new GLib.Array<int>();

    public int get(int index) {
      return _storage.index(index);
    }

    public void set(int index, int value) {
      if (_storage.length >= index + 1) {
        _storage.remove_index(index);
      }
      _storage.insert_val(index, value);
    }
  }

  public class _DynamicArray< TYPE > {
    public GLib.Array<TYPE> _storage = new GLib.Array<TYPE>();

    public int get(int index) {
      return (int) _storage.index(index);
    }

    public void set(int index, TYPE value) {
      _storage.insert_val(index, value);
    }
  }

  public class _VectorInt : GLib.Object {
    public int _array[3];
    public _StaticArrayInt _sarrayint = new _StaticArrayInt(3);
    public _StaticArray<int> _sarrayint2 = new _StaticArray<int>(3);
    public GLib.Array<int> _garray;
    public _DynamicArrayInt _darrayint = new _DynamicArrayInt();
    public _DynamicArray<int> __darrayint = new _DynamicArray<int>();
  }

  // This is the interface we will use in the plugin
  // Putting functions into an interface causes GObject to recognize it as a virtual method

  public interface Api: GLib.Object {

    // Example #1a -- method that is invoked with in/out/in-out basic integer types (no return)
    //
    // Take a number (in/out) then add a number to it (in), output the negative of it (out)
    //
    // feature_plugin_api_example1a:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):

    public abstract void
    example1a(ref int number,
	      int delta,
	      out int negative);

    // Example #1b -- method that is invoked with in/out/in-out basic integer types (with return)
    //
    // Take a number (in/out) then add a number to it (in), output the negative of it (out)
    // Return the sum of all 3 of these
    //
    // feature_plugin_api_example1b:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):

    public abstract int
    example1b(ref int number,
	      int delta,
	      out int negative);

    // Example #1c -- method that is invoked with 3 in and 3 out basic integer types (no return)
    //
    // Take 3 numbers and return the negative of each of them
    //
    // feature_plugin_api_example1c:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):

    public abstract void
    example1c(int number1,
	      int number2,
	      int number3,
	      out int negative1,
	      out int negative2,
	      out int negative3);

    // Example #1d -- method that is invoked with 3 in and 3 out basic integer types (no return)
    //
    // Take 3 numbers and return the negative of each of them
    // Return the sum of the square of all 6 of these
    //
    // feature_plugin_api_example1d:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):

    public abstract int
    example1d(int number1,
	      int number2,
	      int number3,
	      out int negative1,
	      out int negative2,
	      out int negative3);

    // Example #1e -- method that is invoked with in/out/in-out basic String types (with return)
    //
    // Take a string (in/out) then append a String to it (in), output the reverse of it (out)
    // Return the sum of the string
    //
    // feature_plugin_api_example1e:
    // @api:
    // @string: (inout):
    // @append: (in):
    // @reverse: (out):
    // Returns: (transfer none):

    public abstract int
    example1e(ref string string,
	      string append,
	      out string reverse);

    // Example #1f -- method that is invoked with in/out/in-out basic Array types (with return)
    //
    // Take an vector array (in/out) then add a array to it (in), output the origin reflection of it (out)
    //
    // feature_plugin_api_example1f:
    // @api:
    // @array: (inout):
    // @delta: (in):
    // @reflection: (out):
    // Returns: (transfer none):
    public abstract int
    example1f([CCode (array_length = true, array_null_terminated = false)]
			  ref int[] value,
			  [CCode (array_length = true, array_null_terminated = false)]
			  int[] delta,
			  [CCode (array_length = true, array_null_terminated = false)]
			  out int[] reflection);

    // Example #2a -- method that is invoked with a in structure imported from this header file
    //
    // Compute the modulus of a complex number
    // 
    // feature_plugin_api_example2a:
    // @api:
    // @complex: (in):
    // Returns: (transfer none):

    public abstract int
    example2a(owned _ComplexNumber complex);

    // Example #2b -- method that is invoked with a out structure imported from this header file
    //
    // Initialize a complex number to (0, -i)
    //
    // feature_plugin_api_example2b:
    // @api:
    // @complex: (out):
    // Returns: (transfer none):

    public abstract void
    example2b(out _ComplexNumber complex);

    // Example #2c -- method that is invoked with a in structure imported from a common header file
    //
    // Compute the modulus of a complex number
    //
    // feature_plugin_api_example2c:
    // @api:
    // @complex: (in):
    // Returns: (transfer none):

    public abstract int
    example2c(MathCommon._ComplexNumber complex);

    // Example #2d -- method that is invoked with a out structure imported from a common header file
    //
    // Initialize a complex number to (0, -i)
    //
    // feature_plugin_api_example2d:
    // @api:
    // @complex: (out):
    // Returns: (transfer none):

    public abstract void
    example2d(out MathCommon._ComplexNumber complex);

    // Example #4a -- method with one each of in out and in/out using structures from a common header file
    //
    // Take a complex number (in/out) then add a complex number to it (in), output the negative of it (out)
    //
    // feature_plugin_api_example1a:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):

    public abstract void
    example4a(ref MathCommon._ComplexNumber number,
	      MathCommon._ComplexNumber delta,
	      out MathCommon._ComplexNumber negative);

    // Example #4b -- method with in/out/return parameters using a stucture type from a commmon header file
    //
    // Take a complex number (in/out) then add a complex number to it (in), return the negative of it (out)
    //
    // feature_plugin_api_example1a:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // Returns: (transfer none):

    public abstract MathCommon._ComplexNumber
    example4b(ref MathCommon._ComplexNumber number,
	      MathCommon._ComplexNumber delta);

    // Example #4c -- method with two in types and a return type using structure from a common header file
    //
    // Add two complex numbers and return the result
    //
    // feature_plugin_api_example4c:
    // @api:
    // @a: (in):
    // @b: (in):
    // Returns: (transfer none):

    public abstract MathCommon._ComplexNumber
    example4c(MathCommon._ComplexNumber a,
	      MathCommon._ComplexNumber b);

    // Example #6a -- method that is invoked with in/out/in-out static GArray types (no-return)
    //
    // Take a vector (in/out) then add a number to it (in), output the negative of it (out)
    //
    // feature_plugin_api_example6a:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):

    public abstract int
    example6a(ref _VectorInt number,
	      _VectorInt delta,
	      out _VectorInt negative);

    // Example #6b -- method that is invoked with in/out/in-out _StaticArrayInt class types (no-return)
    //
    // Take a vector (in/out) then add a number to it (in), output the negative of it (out)
    //
    // feature_plugin_api_example6b:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):
    public abstract int
    example6b(ref _VectorInt number,
	      _VectorInt delta,
	      out _VectorInt negative);

    // Example #6c -- method that is invoked with in/out/in-out _StaticArray<int> class types (no-return)
    //
    // Take a vector (in/out) then add a number to it (in), output the negative of it (out)
    //
    // feature_plugin_api_example6c:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):
    public abstract int
    example6c(ref _VectorInt number,
	      _VectorInt delta,
	      out _VectorInt negative);

    // Example #7a -- method that is invoked with in/out/in-out dynamic GArray types (no-return)
    //
    // Take a vector (in/out) then add a number to it (in), output the negative of it (out)
    // Return the sum of the reflection elements
    //
    // feature_plugin_api_example7a:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):

    public abstract int
    example7a(ref _VectorInt number,
	      _VectorInt delta,
	      out _VectorInt negative);

    // Example #7b -- method that is invoked with in/out/in-out _DynamicArrayInt class types (no-return)
    //
    // Take a vector (in/out) then add a number to it (in), output the negative of it (out)
    // Return the sum of the reflection elements
    //
    // feature_plugin_api_example7b:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):
    public abstract int
    example7b(ref _VectorInt number,
	      _VectorInt delta,
	      out _VectorInt negative);

    // Example #7c -- method that is invoked with in/out/in-out _DynamicArray<int> class types (no-return)
    //
    // Take a vector (in/out) then add a number to it (in), output the negative of it (out)
    // Return the sum of the reflection elements
    //
    // feature_plugin_api_example7c:
    // @api:
    // @number: (inout):
    // @delta: (in):
    // @negative: (out):
    // Returns: (transfer none):
    public abstract int
    example7c(ref _VectorInt number,
	      _VectorInt delta,
	      out _VectorInt negative);
  }
}
