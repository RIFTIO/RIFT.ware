--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   single_command.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local command = ARGV[1]
local key = KEYS[1]
local db = tonumber(ARGV[2])
local serial = tonumber(ARGV[3])
local reply = 0
--redis.call("SELECT", db)

if command == "SET" then
  local value = ARGV[4]
  reply = redis.call(command, key, value)
else
  reply = redis.call(command, key)
end

return reply
