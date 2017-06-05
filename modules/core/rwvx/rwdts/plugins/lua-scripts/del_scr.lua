--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   del_scr.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local db = tonumber(ARGV[1])
local serial = tonumber(ARGV[2])
local reply = 0
--redis.call("SELECT", db)

local key  = redis.call("HKEYS", KEYS[1])
if key[1] == nil then
  return reply
else
  local delimiter = ":"
  local result = {}
  local i = 0
  for match in (key[1]..delimiter):gmatch("(.-)"..delimiter) do
      result[i] = match
      i = i + 1
  end
  local old_serial = tonumber(result[0])
  if serial == (old_serial + 1) then
    reply = redis.call("DEL", KEYS[1])
  end
end
return reply
