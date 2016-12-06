#!/usr/bin/luajit

require('luaunit')

local lgi = require 'lgi'
local GObject = lgi.GObject
local ExampleGi = lgi.ExampleGi

TestExampleGi = {} --class
  function TestExampleGi:setUp()
    self.boxed = ExampleGi.Boxed()
  end

  function TestExampleGi:tearDown()
  end

  function TestExampleGi:test_string_array()
    self.boxed.string_array = {"hello", "world"}
    assertEquals(self.boxed.string_array, {"hello", "world"})
  end

LuaUnit:run()
