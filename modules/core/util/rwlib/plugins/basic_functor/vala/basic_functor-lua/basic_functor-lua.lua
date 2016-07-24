local lgi = require 'lgi'
local GObject = lgi.GObject
local BasicFunctor = lgi.BasicFunctor

BasicLuaFunctor = GObject.Object:derive('BasicLuaFunctor', {
    BasicFunctor.Api
})

function BasicLuaFunctor:do_callback_example(functor)
  functor:callback()
  return 0
end

return { BasicLuaFunctor }
