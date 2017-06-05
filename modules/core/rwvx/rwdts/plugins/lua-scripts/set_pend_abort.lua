--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   set_pend_abort.lua
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
   reply = redis.call("DEL", final_pend_key)
   local old_key = redis.call("HKEYS", KEYS[1])
   if old_key[1] ~= nil then
       local delimiter = ":"
       local result = {}
       local i = 0
       for match in (old_key[1]..delimiter):gmatch("(.-)"..delimiter) do
         result[i] = match
         i = i + 1
       end
       local old_serial = tonumber(result[0])
       max = old_serial + 1
       return {reply, max}
    end
  end
end

return {reply, max}
