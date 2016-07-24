------------------------------------------------------------------------------
--
--  LGI Gst override module.
--
--  Copyright (c) 2010-2013 Pavel Holejsovsky
--  Licensed under the MIT license:
--  http://www.opensource.org/licenses/mit-license.php
--
------------------------------------------------------------------------------

local ipairs, tonumber = ipairs, tonumber
local os = require 'os'

local lgi = require 'lgi'
local gi = require('lgi.core').gi
local GLib = lgi.GLib
local Gst = lgi.Gst

if tonumber(Gst._version) < 1.0 then
   -- GstObject has special ref_sink mechanism, make sure that lgi
   -- core is aware of it, otherwise refcounting is screwed.
   Gst.Object._refsink = gi.Gst.Object.methods.ref_sink
end

-- Gst.Element; GstElement uses ugly macro accessors instead of proper
-- GObject properties, so add attributes for assorted Gst.Element
-- properties.
Gst.Element._attribute = {}
for _, name in ipairs {
   'name', 'parent', 'bus', 'clock', 'base_time', 'start_time',
   'factory', 'index', 'state' } do
   Gst.Element._attribute[name] = {
      get = Gst.Element['get_' .. name],
      set = Gst.Element['set_' .. name],
   }
end

function Gst.Element:link_many(...)
   local target = self
   for _, source in ipairs {...} do
      if not target:link(source) then
	 return false
      end
      target = source
   end
   return true
end

-- Gst.Bin adjustments
if tonumber(Gst._version) < 1.0 then
   function Gst.Bus:add_watch(priority, callback)
      if not callback then
	 callback = priority
	 priority = GLib.PRIORITY_DEFAULT
      end
      return self:add_watch_full(priority, callback)
   end
end

function Gst.Bin:add_many(...)
   local args = {...}
   for i = 1, #args do self:add(args[i]) end
end

-- Gst.TagList adjustments
if not Gst.TagList.copy_value then
   Gst.TagList._method.copy_value = Gst.tag_list_copy_value
end
function Gst.TagList:get(tag)
   local gvalue = self:copy_value(tag)
   return gvalue and gvalue.value
end

-- Load additional Gst modules.
if tonumber(Gst._version) < 1.0 then
   local GstInterfaces = lgi.require('GstInterfaces', Gst._version)
end

-- Initialize gstreamer.
Gst.init()

-- Undo unfortunate gstreamer's setlocale(LC_ALL, ""), which breaks
-- Lua's tonumber() implementation for some locales (e.g. pl_PL, pt_BR
-- and probably many others).
os.setlocale ('C', 'numeric')
