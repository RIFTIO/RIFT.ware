#!/usr/bin/luajit

local lgi = require 'lgi'
local GObject = lgi.GObject
local BasicPlugin = lgi.BasicPlugin

BasicPluginLua = GObject.Object:derive('BasicPluginLua', {
    BasicPlugin.Api
})

function BasicPluginLua:do_add(a, b)
  return a + b
end

if _NAME == nil then
  --add your test code to execute this as a standalone program
  foo = BasicPluginLua()
  print("Result of add(2, 3)", foo:add(2, 3))
end

return { BasicPluginLua }
