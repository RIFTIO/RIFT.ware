const Peas = imports.gi.Peas;
const FeaturePlugin = imports.gi.FeaturePlugin;
const MathCommon = imports.gi.MathCommon;

function extension_js_plugin() {
  this.read_only = "read-only";
  this.readwrite = "readwrite";
  this.print_done = {};
}

function type(obj){
  return Object.prototype.toString.call(obj).match(/^\[object (.*)\]$/)[1]
}

extension_js_plugin.prototype = {

  activate: function() {
  },

  deactivate: function() {
  },

  get_plugin_info: function() {
    return this.plugin_info;
  },

  get_settings: function() {
    return this.plugin_info.get_settings(null);
  },

  call_with_return: function() {
    return "Hello, World!";
  },

  call_no_args: function() {
  },

  call_single_arg: function() {
    return true;
  },

  call_multi_args: function(in_, inout) {
    return [ in_, inout ];
  },

  log_function: function(funcname) {
    if (this.print_done[funcname] == null) {
      print ("=============================================");
      print ("This is a call to the javascript plugin " + funcname);
      print ("=============================================");
      this.print_done[funcname] = 1;
    }
  },

  example1a: function(number, delta) {
    this.log_function("example1a");

    // Take a number (in/out) then add a number to it (in)
    number += delta;

    // output the negative of it (out)
    negative = -number;

    return [number, negative];
  },

  example1b: function(number, delta) {
    // Log the function entry
    this.log_function("example1b");

    // Take a number (in/out) then add a number to it (in)
    number += delta;

    // output the negative of it (out)
    negative = -number;

    // Return the sum of all 3 of these
    result = number + delta + negative;

    return [result, number, negative];
  },

  example1c: function(number1, number2, number3) {
    // Log the function entry
    this.log_function("example1c");

    // Take 3 numbers and return the negative of each of them
    negative1 = -number1;
    negative2 = -number2;
    negative3 = -number3;

    return [negative1, negative2, negative3];
  },

  example1d: function(number1, number2, number3) {
    // Log the function entry
    this.log_function("example1d");

    // Take 3 numbers and return the negative of each of them
    negative1 = -number1;
    negative2 = -number2;
    negative3 = -number3;

    // Return the sum of the square of all 6 of these
    result = 2 * (number1 * number1 + number2 * number2 + number3 * number3);

    return [result, negative1, negative2, negative3];
  },

  example1e: function(value, delta) {
    // Log the function entry
    this.log_function("example1e");

    // Take a string (in/out) then append a String to it (in), and also output the reverse of it (out)
    value += delta;
    reverse = value.split('').reverse().join('');

    // Return the length of the string
    result = value.length;

    return [result, value, reverse];
  }, 

  example1f: function(value, value_len, delta, delta_len) {
    // Log the function entry
    this.log_function("example1f");

    // Take an vector array (in/out) then add a array to it (in)
    value[0] = value[0] + delta[0];
    value[1] = value[1] + delta[1];
    value[2] = value[2] + delta[2];

    // Output the origin reflection of it (out)
    reflection = [ 0, 0, 0 ];
    reflection[0] = -value[0]; reflection[1] = -value[1]; reflection[2] = -value[2];

    // Return the sum of the reflection elements
    result = reflection[0] + reflection[1] + reflection[2];

    return [result, value, value_len, delta, delta_len, reflection, 3];
  },

  example2a: function(a) {
    // Log the function entry
    this.log_function("example2a");

    print(type(a));

    result = Math.sqrt(a.real * a.real + a.imag * a.imag);

    // print("Result = " + result);
   
    return result;
  },
 
  example2b: function() {
    // Log the function entry
    this.log_function("example2b");

    // Initialize a complex number to (0, -i)
    complex_number = new FeaturePlugin._ComplexNumber();
    complex_number.real = 0;
    complex_number.imag = -1;

    return complex_number;
  },

  example2c: function(a) {
    // Log the function entry
    this.log_function("example2c");

    // Compute the modulus of a complex number
    result = Math.sqrt(a.real * a.real + a.imag * a.imag);

    return result;
  },

  example2d: function() {
    // Log the function entry
    this.log_function("example2d");

    // Initialize a complex number to (0, -i)
    complex_number = new MathCommon._ComplexNumber();
    complex_number.real = 0;
    complex_number.imag = -1;

    return (complex_number);
  },

  example3a: function(callback) {
    // Log the function entry
    this.log_function("example3a")

    // Invoke the Vala callback method
    callback.invoke_callback()
  }

};

function missing_prerequisite_extension() {}

var extensions = {
  "PeasActivatable": extension_js_plugin,
  "FeaturePluginApi": extension_js_plugin,
};
