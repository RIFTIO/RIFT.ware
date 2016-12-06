-- This script is invoked through peas interface

-- Load GObject and LuaCoroutine classes
local lgi = require 'lgi'
local GObject = lgi.GObject
local LuaCoroutine = lgi.LuaCoroutine

LuaCoroutineLua = GObject.Object:derive('LuaCoroutineLua', {
    LuaCoroutine.Api
})

-- Example function that adds two numbers, but yields before
-- returning the sum for fun
function LuaCoroutineLua:do_add_coroutine_wrap(a, b)

  -- A very simple coroutine
  local co = coroutine.wrap(function(a, b)
               -- Yeild for fun
               -- This suspends the execution of this function
               -- the arguments to yield become return values
               -- to the caller
               coroutine.yield(a)
               return a + b
             end)

  -- Allocate a functor, and assign the populate the coroutine
  -- in the functor. This functor is returned to the caller.
  -- The caller may invoke the coroutine
  functor = LuaCoroutine.Functor()
  functor:set_function(co)

  -- Invoke the coroutine
  -- This will return as soon as the first yield is hit in the code
  -- The return value is the argument to yield in the coroutine
  local status = co(a, b)

  -- Just return a status code and the functor
  -- The caller may invoke the coroutine through functor
  return status, functor
end

-- Example function that adds two numbers, but yields before
-- returning the sum for fun
function LuaCoroutineLua:do_add_coroutine_create(a, b)

  -- A very simple coroutine using create
  local co = coroutine.create(function(a, b)
               -- Yeild for fun
               -- This suspends the execution of this function
               -- the arguments to yield become return values
               -- to the caller
               coroutine.yield(a)
               return a + b
             end)

  -- Allocate a functor, and assign the populate the coroutine
  -- in the functor. This functor is returned to the caller.
  -- The caller may invoke the coroutine
  functor = LuaCoroutine.Functor()
  functor:set_function(co)

  -- Invoke the coroutine
  -- This will return as soon as the first yield is hit in the code
  -- The return value is the argument to yield in the coroutine
  local status, rval = coroutine.resume(co, a, b)

  -- Just return a status code and the functor
  -- The caller may invoke the coroutine through functor
  return rval, functor
end

return { LuaCoroutineLua }
