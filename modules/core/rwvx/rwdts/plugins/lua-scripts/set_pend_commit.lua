--[[ 
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */



  @file   set_pend_commit.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local db = tonumber(ARGV[1])
local serial = ARGV[2]
local int_serial = tonumber(serial)
local shard = ARGV[3]
local reply = "FAIL"
local inter_key = {}
--redis.call("SELECT", db)
local max = 0

local pend_key = {}
table.insert(pend_key, "pending-add-")
table.insert(pend_key, KEYS[1])
local final_pend_key = table.concat(pend_key)
local key  = redis.call("HKEYS", final_pend_key)
if key[1] ~= nil then
  local delimiter = ":"
  local result = {}
  local i = 0
  for match in (key[1]..delimiter):gmatch("(.-)"..delimiter) do
      result[i] = match
      i = i + 1
  end
  local old_serial = tonumber(result[0])

  if int_serial ~= (old_serial + 1) then
    return {reply, max}
  else
   local value = redis.call("HGET", final_pend_key, key[1])
   table.insert(inter_key, serial)
   table.insert(inter_key, ":")
   table.insert(inter_key, shard)
   local final_key = table.concat(inter_key)
   redis.call("DEL", KEYS[1])
   redis.call("DEL", final_pend_key)
   reply = redis.call("HSET", KEYS[1], final_key, value)
   max = (int_serial + 1)
   return {reply, max}
  end
end

return {reply, max}
