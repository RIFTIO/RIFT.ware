--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   get_all.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local val = {}
local key = {}
local db = tonumber(ARGV[1])
local i = 1
local count = 1
--redis.call("SELECT", db)
local keys_result = redis.call("KEYS", "*")
while keys_result[i] do
  local hget_result = redis.call("HGETALL", keys_result[i])
  val[count] = hget_result[2]
  key[count] = keys_result[i]
  count = count + 1
  i = i + 1
end
return {key, val}
