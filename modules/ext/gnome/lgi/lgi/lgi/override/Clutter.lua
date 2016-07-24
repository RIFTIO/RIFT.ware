------------------------------------------------------------------------------
--
--  LGI Clutter override module.
--
--  Copyright (c) 2010, 2011 Pavel Holejsovsky
--  Licensed under the MIT license:
--  http://www.opensource.org/licenses/mit-license.php
--
------------------------------------------------------------------------------

local select, type, pairs, setmetatable, error
   = select, type, pairs, setmetatable, error
local lgi = require 'lgi'
local core = require 'lgi.core'
local Clutter = lgi.Clutter

Clutter.Container._attribute = {}

function Clutter.Container:add(...)
   local args = { ... }
   for i = 1, #args do Clutter.Container.add_actor(self, args[i]) end
end

-- Allow ctor to add widgets from the array part
Clutter.Container._container_add = Clutter.Container.add_actor

-- Provides pseudo-attribute 'meta' for accessing container's
-- child-meta elements.
local container_child_meta_mt = {}
function container_child_meta_mt:__index(child)
   return self._container:get_child_meta(child)
end
Clutter.Container._attribute.meta = {}
function Clutter.Container._attribute.meta:get()
   return setmetatable({ _container = self }, container_child_meta_mt)
end

-- Take over internal Clutter synchronization lock and initialize
-- Clutter's threading.
core.registerlock(core.gi.Clutter.resolve.clutter_threads_set_lock_functions)
Clutter.threads_init()

-- Automatically initialize clutter, avoid continuing if
-- initialization fails.
local status = Clutter.init()
if status ~= 'SUCCESS' then
   error(("Clutter initialization failed: %s"):format(status))
end
