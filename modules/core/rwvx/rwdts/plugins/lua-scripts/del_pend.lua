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



  @file   del_pend.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local db = tonumber(ARGV[1])
local serial = tonumber(ARGV[2])
local reply = 0
local max = 0;
local inter_key = {}
--redis.call("SELECT", db)

local pend_key = {}
table.insert(pend_key, "pending-del-")
table.insert(pend_key, KEYS[1])
local final_pend_key = table.concat(pend_key)
local key  = redis.call("HKEYS", final_pend_key)
if key[1] ~= nil then
  return {reply, max}
else
  local key  = redis.call("HKEYS", KEYS[1])
  if (key[1] == nil) then
    return {reply, max}
  else
    local delimiter = ":"
    local result = {}
    local i = 0
    for match in (key[1]..delimiter):gmatch("(.-)"..delimiter) do
        result[i] = match
        i = i + 1
    end
    local old_serial = tonumber(result[0])
    if ((serial == (old_serial + 1)) or (serial == 0)) then
      table.insert(inter_key, (old_serial + 1))
      table.insert(inter_key, ":")
      table.insert(inter_key, result[1])
      local final_key = table.concat(inter_key)

      local value = redis.call("HGET", KEYS[1], key[1]) 
      max = old_serial + 2
      reply = redis.call("HSET", final_pend_key, final_key, value)
    end
  end
end
return {reply, max}
