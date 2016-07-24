#!/usr/bin/luajit

-- Provides template for how the 
-- RWLOG bindings can be imported to lua

-- Import the LGI bindings
local lgi = require 'lgi'
local GObject = lgi.GObject

-- Import the RwLog
local RwLog = lgi.RwLog
local RwLogYang = lgi.RwLogYang

-- lua wrapper function for rwlog
function rwlog_lua(info)
  -- Create the log handle
  local logh = RwLog.Ctx("test_rwlog_gi.lua")

  -- Lua function to get the source and line number 
  print ('currentline', info.currentline)
  print ('short_src', info.short_src)

  -- Compose the event
  ev = RwLogYang:EvTemplate();    -- or whatever the event is
  tp = ev.template_params
  tp.filename = info.short_src
  tp.linenumber = info.currentline
  pb = ev:to_ptr()

  -- Send the protobuf
  logh:proto_send(pb)
end

-- debug.getinfo is used to get the filename and linenumber
rwlog_lua(debug.getinfo(1, 'Sl'))
