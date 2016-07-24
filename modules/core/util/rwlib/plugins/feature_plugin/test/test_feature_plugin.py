#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#



from gi.repository import GObject, Peas, GLib, GIRepository
from gi.repository import MathCommon, FeaturePlugin
import ctypes, inspect, operator
import rw_peas

def test_feature_plugin_api_examples(engine, info):
  test_feature_plugin_api_example1a(engine, info)
  test_feature_plugin_api_example1b(engine, info)
  test_feature_plugin_api_example1c(engine, info)
  test_feature_plugin_api_example1d(engine, info)
  test_feature_plugin_api_example1e(engine, info)
  # test_feature_plugin_api_example1f(engine, info)
  test_feature_plugin_api_example2a(engine, info)
  test_feature_plugin_api_example2b(engine, info)
  test_feature_plugin_api_example2c(engine, info)
  test_feature_plugin_api_example2d(engine, info)
  test_feature_plugin_api_example3a(engine, info)

class foo(GObject.Object, Peas.Activatable, FeaturePlugin.Api):
    object = GObject.property(type=GObject.Object)

def g_assert(expr):
  if not expr:
    caller_frame = inspect.stack()[1]
    caller_file = caller_frame[1]
    caller_line = caller_frame[2]
    caller_func = caller_frame[3]
    GLib.assertion_message("", caller_file, caller_line, caller_func, str(expr))

def g_assert_cmpint(n1, cmp, n2):
  ops = { "==" : operator.eq } 
  op = ops[cmp]
  if not op(n1, n2):
    caller_frame = inspect.stack()[1]
    caller_file = caller_frame[1]
    caller_line = caller_frame[2]
    caller_func = caller_frame[3]
    expr = str(n1) + " " + str(cmp) + " " + str(n2)
    GLib.assertion_message("", caller_file, caller_line, caller_func, str(expr))

def g_assert_cmpstr(n1, cmp, n2):
  ops = { "==" : operator.eq } 
  op = ops[cmp]
  if not op(n1, n2):
    caller_frame = inspect.stack()[1]
    caller_file = caller_frame[1]
    caller_line = caller_frame[2]
    caller_func = caller_frame[3]
    expr = str(n1) + " " + str(cmp) + " " + str(n2)
    GLib.assertion_message("", caller_file, caller_line, caller_func, str(expr))

#
# Example #1a -- method that is invoked with in/out/in-out basic integer types (no return)
#
# Take a number (in/out) then add a number to it (in), output the negative of it (out)
#

def test_feature_plugin_api_example1a(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the number to 1 and the delta to 2
  # After the operation, the number should be to 3 and the negative should be -3
  number = 1
  delta = 2
  negative = 0

  # Call the plugin function using the function pointer
  (number, negative) = extension.example1a(number, delta)
  g_assert_cmpint(number, "==", 3)
  g_assert_cmpint(negative, "==", -3)

#
# Example #1b -- method that is invoked with in/out/in-out basic integer types (with return)
#
# Take a number (in/out) then add a number to it (in), output the negative of it (out)
# Return the sum of all 3 of these
#

def test_feature_plugin_api_example1b(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the number to 1 and the delta to 2
  # After the operation, the number should be to 3 and the negative should be -3
  number = 1
  delta = 2
  negative = 0

  # Call the plugin function using the function pointer
  (result, number, negative) = extension.example1b(number, delta)
  g_assert_cmpint(number, "==", 3)
  g_assert_cmpint(negative, "==", -3)
  g_assert_cmpint(result, "==", number + delta + negative)

#
# Example #1c -- method that is invoked with 3 in and 3 out basic integer types (no return)
#
# Take a number (in/out) then add a number to it (in), output the negative of it (out)
#

def test_feature_plugin_api_example1c(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the numbers to: 1, 2, 3
  # After the operation, the negative of these values should be set in the output variables
  number1 = 1
  number2 = 2
  number3 = 3
  negative1 = negative2 = negative3 = 0

  # Call the plugin function using the function pointer
  (negative1, negative2, negative3) = extension.example1c(number1, number2, number3);
  g_assert_cmpint(negative1, "==", -1);
  g_assert_cmpint(negative2, "==", -2);
  g_assert_cmpint(negative3, "==", -3);

#
# Example #1d -- method that is invoked with in/out/in-out basic integer types (with return)
#
# Take a number (in/out) then add a number to it (in), output the negative of it (out)
# Return the sum of the square of all 6 of these
#

def test_feature_plugin_api_example1d(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the numbers to: 1, 2, 3
  # After the operation, the negative of these values should be set in the output variables
  number1 = 1
  number2 = 2
  number3 = 3
  negative1 = negative2 = negative3 = 0

  # Call the plugin function using the function pointer
  (result, negative1, negative2, negative3) = extension.example1d(number1, number2, number3)
  g_assert_cmpint(negative1, "==", -1)
  g_assert_cmpint(negative2, "==", -2)
  g_assert_cmpint(negative3, "==", -3)
  g_assert_cmpint(result, "==", 1 * 1 + 2 * 2 + 3 * 3 + 1 * 1 + 2 * 2 + 3 * 3)

#
# Example #1e -- method that is invoked with in/out/in-out basic integer types (with return)
#
# Take a string (in/out) then append a String to it (in), and also output the reverse of it (out)
# Return the length of the string
#

def test_feature_plugin_api_example1e(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the value to "abc" and the delta to "def"
  # After the operation, the value should be "abc" and the reverse should be "fedcba"
  value = "abc"
  delta = "def"
  reverse = None

  # Call the plugin function using the function pointer
  (result, value, reverse) = extension.example1e(value, delta)
  g_assert_cmpstr(value, "==", "abcdef");
  g_assert_cmpstr(reverse, "==", "fedcba");
  g_assert_cmpint(result, "==", len("abcdef"));

#
# Example #1f -- method that is invoked with in/out/in-out basic integer types (no return)
#
# Take an vector array (in/out) then add a array to it (in), and also output the origin reflection of it (out)
# Return the sum of the reflection elements
#

def test_feature_plugin_api_example1f(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the value to (0, 1, 2) the delta (3, 4, 5) to it
  # After the operation, the value should be (3, 5, 7) and the reflection should be (-3, -5, -7)
  value1 = ( 0, 1, 2 )
  delta1 = ( 3, 4, 5 )

  # Call the plugin function using the function pointer
  (result, value1, reflection1) = extension.example1f(value1, delta1)

#
# Example #2a -- method that is invoked with a in structure imported from this header file
#
# Compute the modulus of a complex number
#

def test_feature_plugin_api_example2a(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the complex number for to (3, 4i), the modulus for this number is 5
  a = FeaturePlugin._ComplexNumber()
  a.real = 3
  a.imag = 4

  # Call the plugin function using the function pointer
  result = extension.example2a(a)
  g_assert_cmpint(result, "==", 5)

#
# Example #2b -- method that is invoked with a out structure imported from this header file
#
# Initialize a complex number to (0, -i)
#

def test_feature_plugin_api_example2b(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Call the plugin function using the function pointer
  a = extension.example2b();
  g_assert_cmpint(a.real, "==", 0);
  g_assert_cmpint(a.imag, "==", -1);

#
# Example #2c -- method that is invoked with a in structure imported from a common header file
#
# Compute the modulus of a complex number
#

def test_feature_plugin_api_example2c(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Initialize the complex number for to (3, 4i), the modulus for this number is 5
  a = MathCommon._ComplexNumber()
  a.real = 3
  a.imag = 4

  # Call the plugin function using the function pointer
  result = extension.example2c(a);
  g_assert_cmpint(result, "==", 5);

#
# Example #2d -- method that is invoked with a in structure imported from a common header file
#
# Initialize a complex number to (0, -i)
#

def test_feature_plugin_api_example2d(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Call the plugin function using the function pointer
  a = extension.example2d()
  g_assert_cmpint(a.real, "==", 0)
  g_assert_cmpint(a.imag, "==", -1)

#
# Example #3a -- method that is invoked with a in callback function that takes a single argument
#
# Set the memory of an address to the indicated value
#

class example3a_klass():
  variable = 0

  def callback_routine(self, address, value):
    self.variable = value

class ValaCallbackFunctor(GObject.Object, FeaturePlugin.ValaCallbackFunctor):
  _method = None

  def do_callback_function(self, address, value):
    self._method(address, value)

def test_feature_plugin_api_example3a(engine, info):
  # Create an instance of the FEATURE_PLUGIN_API class
  extension = engine.create_extension(info, FeaturePlugin.Api, None)

  # Create an instance of the callback class
  example3a = example3a_klass()

  # Create a ValaCallbackFunctor for the callback function
  functor = ValaCallbackFunctor()
  functor._method = example3a.callback_routine

  # Set the class variable to an int32 with an initial value of zero
  example3a.variable = ctypes.c_int(0)
  address = ctypes.addressof(example3a.variable)
  value = example3a.variable.value

  # Call the plugin function using the function pointer
  extension.example3a(functor, address, 0x12345678)
  g_assert_cmpint(example3a.variable, "==", 0x12345678);

def main():
  # Initialize a PEAS engine
  #engine = rw_peas.create_engine()

  # Load our plugin proxy into the g_irepository namespace
  #default = GIRepository.Repository.get_default()
  #typelib = GIRepository.Repository.require(default, "MathCommon", "1.0", 0)
  #GIRepository.Repository.require(default, "FeaturePlugin", "1.0", 0)

  # Enable every possible language loader
  #engine.enable_loader("python");
  #engine.enable_loader("c");

  # Run a testcase for each possible extension type
  # extension_list = [ "python" ]
  extension_list = [ "c" ]
  for extension in extension_list:
    engine, info, plugin = rw_peas.PeasPlugin('feature_plugin-' + extension, 'FeaturePlugin-1.0')()
    #extension_plugin = "feature_plugin-" + extension
    #info = engine.get_plugin_info(extension_plugin)
    #engine.load_plugin(info)
    test_feature_plugin_api_examples(engine, info)

if __name__ == "__main__":
    main()
