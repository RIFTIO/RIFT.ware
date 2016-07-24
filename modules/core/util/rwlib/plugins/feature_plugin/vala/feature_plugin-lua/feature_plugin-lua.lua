local lgi = require 'lgi'
local GObject = lgi.GObject
local FeaturePlugin = lgi.FeaturePlugin

FeaturePluginLua = GObject.Object:derive('FeaturePluginLua', {
    FeaturePlugin.Api
})

function FeaturePluginLua:do_example1a(number, delta)
  -- Take a number (in/out) then add a number to it (in)
  number = number + delta
  -- output the negative of it (out)
  negative = -number
  return number, negative
end

function FeaturePluginLua:do_example1b(number, delta)
  -- Take a number (in/out) then add a number to it (in)
  number = number + delta

  -- output the negative of it (out)
  negative = -number
  result = number + delta + negative
  return result, number, negative
end

function FeaturePluginLua:do_example1c(number1, number2, number3)
  -- Take 3 numbers and return the negative of each of them
  negative1 = -number1
  negative2 = -number2
  negative3 = -number3
  return negative1, negative2, negative3
end

function FeaturePluginLua:do_example1d(number1, number2, number3)
  -- Take 3 numbers and return the negative of each of them
  negative1 = -number1
  negative2 = -number2
  negative3 = -number3

  -- Return the sum of the square of all 6 of these
  result = 2 * (number1 * number1 + number2 * number2 + number3 * number3);
  return result, negative1, negative2, negative3
end

function FeaturePluginLua:do_example1e(value, delta)
  -- Take a string (in/out) then append a String to it (in), and also output the reverse of it (out)
  value = value .. delta
  reverse = string.reverse(value)
  result = string.len(value)
  return result, value, reverse
end

function FeaturePluginLua:do_example1f(value, delta)

  print(table.getn(delta))
  print("delta");

  -- Take an vector array (in/out) then add a array to it (in)
  value[1] = value[1] + delta[1]
  value[2] = value[2] + delta[2]
  value[3] = value[3] + delta[3]

  -- Output the origin reflection of it (out)
  reflection = {}
  reflection[1] = -value[1]
  reflection[2] = -value[2]
  reflection[3] = -value[3]

  -- Return the sum of the reflection elements
  result = reflection[1] + reflection[2] + reflection[3];

  return result, value, reflection
end


function FeaturePluginLua:do_example2a(a)
  print(a)
  -- Compute the modulus of a complex number
  result = math.sqrt(a.real * a.real + a.imag * a.imag)
  print(a.real, a.imag, result)

  return result
end


return { FeaturePluginLua }
