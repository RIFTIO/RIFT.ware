#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:

#need to export GI_TYPELIB_PATH and LD_LIBARARY_PATH 
#for this script to run standalone

from gi.repository import GObject, FeaturePlugin, MathCommon
import math

class FeaturePluginPython(GObject.Object, FeaturePlugin.Api):
    object = GObject.property(type=GObject.Object)

    construct_only = GObject.property(type=str)
    read_only = GObject.property(type=str, default="read-only")
    write_only = GObject.property(type=str)
    readwrite = GObject.property(type=str, default="readwrite")
    prerequisite = GObject.property(type=str)

    print_done = {}

    def log_gobject_version(self):
        print("GObject version => " + str(GObject._version))
        print("PYGObject version => " + str(GObject.pygobject_version))

    def log_function(self, funcname):
        if not funcname in self.print_done:
            print ("=============================================")
            print ("This is a call to the python plugin " + funcname)
            self.log_gobject_version()
            print ("=============================================")
            self.print_done[funcname] = 1

    def do_example1a(self, number, delta):
        # Log the function entry
        self.log_function("example1a")

	# Take a number (in/out) then add a number to it (in)
        number += delta

        # output the negative of it (out)
        negative = -number

        return (number, negative)

    def do_example1b(self, number, delta):
        # Log the function entry
        self.log_function("example1b")

	# Take a number (in/out) then add a number to it (in)
        number += delta

        # output the negative of it (out)
        negative = -number

        # Return the sum of all 3 of these
        result = number + delta + negative

        return (result, number, negative)

    def do_example1c(self, number1, number2, number3):
        # Log the function entry
        self.log_function("example1c")

        # Take 3 numbers and return the negative of each of them
        negative1 = -number1;
        negative2 = -number2;
        negative3 = -number3;

        return (negative1, negative2, negative3)

    def do_example1d(self, number1, number2, number3):
        # Log the function entry
        self.log_function("example1d")

        # Take 3 numbers and return the negative of each of them
        negative1 = -number1;
        negative2 = -number2;
        negative3 = -number3;

        # Return the sum of the square of all 6 of these
        result = 2 * (number1 * number1 + number2 * number2 + number3 * number3);

        return (result, negative1, negative2, negative3)

    def do_example1e(self, value, delta):
        # Log the function entry
        self.log_function("example1e")

        # Take a string (in/out) then append a String to it (in), and also output the reverse of it (out)
        value += delta
        reverse = value[::-1]

        # Return the length of the string
        result = len(value)

        return (result, value, reverse)

    def do_example1f(self, value, value_len, delta, delta_len):
        # Log the function entry
        self.log_function("example1f")

        # Take an vector array (in/out) then add a array to it (in)
        value[0] = value[0] + delta[0]
        value[1] = value[1] + delta[1]
        value[2] = value[2] + delta[2]

        # Output the origin reflection of it (out)
        reflection = [ 0, 0, 0 ]
        reflection[0] = -value[0]; reflection[1] = -value[1]; reflection[2] = -value[2];

        # Return the sum of the reflection elements
        result = reflection[0] + reflection[1] + reflection[2];

        return (result, value, value_len, reflection, 3)

    def do_example2a(self, a):
        # Log the function entry
        self.log_function("example2a")

        # Compute the modulus of a complex number
        result = math.sqrt(a.real * a.real + a.imag * a.imag)

        return result

    def do_example2b(self):
        # Log the function entry
        self.log_function("example2b")

        # Initialize a complex number to (0, -i)
        complex_number = FeaturePlugin._ComplexNumber()
        complex_number.real = 0
        complex_number.imag = -1

        return (complex_number)

    def do_example2c(self, a):
        # Log the function entry
        self.log_function("example2c")

        # Compute the modulus of a complex number
        result = math.sqrt(a.real * a.real + a.imag * a.imag)

        return result

    def do_example2d(self):
        # Log the function entry
        self.log_function("example2d")

        # Initialize a complex number to (0, -i)
        complex_number = MathCommon._ComplexNumber()
        complex_number.real = 0
        complex_number.imag = -1

        return (complex_number)

    def do_example6b(self, value, delta):
        # Log the function entry
        self.log_function("example6b")

        # Take an vector array (in/out) then add a array to it (in)
        value._sarrayint.set(0, value._sarrayint.get(0) + delta._sarrayint.get(0))
        value._sarrayint.set(1, value._sarrayint.get(1) + delta._sarrayint.get(1))
        value._sarrayint.set(2, value._sarrayint.get(2) + delta._sarrayint.get(2))

        # Output the origin reflection of it (out)
        reflection = FeaturePlugin._VectorInt()
        reflection._sarrayint.set(0, -value._sarrayint.get(0))
        reflection._sarrayint.set(1, -value._sarrayint.get(1))
        reflection._sarrayint.set(2, -value._sarrayint.get(2))

        # Return the sum of the reflection elements
        result = reflection._sarrayint.get(0) + reflection._sarrayint.get(1) + reflection._sarrayint.get(2)

        return (result, value, reflection)

    def do_example6c(self, value, delta):
        # Log the function entry
        self.log_function("example6c")

        # Take an vector array (in/out) then add a array to it (in)
        value._sarrayint2.set(0, value._sarrayint2.get(0) + delta._sarrayint2.get(0))
        value._sarrayint2.set(1, value._sarrayint2.get(1) + delta._sarrayint2.get(1))
        value._sarrayint2.set(2, value._sarrayint2.get(2) + delta._sarrayint2.get(2))

        # Output the origin reflection of it (out)
        reflection = FeaturePlugin._VectorInt()
        reflection._sarrayint2.set(0, -value._sarrayint2.get(0))
        reflection._sarrayint2.set(1, -value._sarrayint2.get(1))
        reflection._sarrayint2.set(2, -value._sarrayint2.get(2))

        # Return the sum of the reflection elements
        result = reflection._sarrayint2.get(0) + reflection._sarrayint2.get(1) + reflection._sarrayint2.get(2)

        return (result, value, reflection)

    def do_example7b(self, value, delta):
        # Log the function entry
        self.log_function("example7b")

        # Take an vector array (in/out) then add a array to it (in)
        value._darrayint.set(0, value._darrayint.get(0) + delta._darrayint.get(0))
        value._darrayint.set(1, value._darrayint.get(1) + delta._darrayint.get(1))
        value._darrayint.set(2, value._darrayint.get(2) + delta._darrayint.get(2))

        # Output the origin reflection of it (out)
        reflection = FeaturePlugin._VectorInt()
        reflection._darrayint.set(0, -value._darrayint.get(0))
        reflection._darrayint.set(1, -value._darrayint.get(1))
        reflection._darrayint.set(2, -value._darrayint.get(2))

        # Return the sum of the reflection elements
        result = reflection._darrayint.get(0) + reflection._darrayint.get(1) + reflection._darrayint.get(2)

        return (result, value, reflection)

#if __name__ == "__main__":
    #add your test code to execute this as a standalone program
    #foo = BasicPlugin.Person()
    #print foo
    #foo.setAge(22)
    #print foo.getAge()
