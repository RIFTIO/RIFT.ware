--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   get_shash_all.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local val = {}
local key = {}
local db = tonumber(ARGV[1])
local i = 1
local count = 1
--redis.call("SELECT", db)
local hget_result = redis.call("HGETALL", ARGV[2])
while hget_result[i] do
  if (i%2) == 0 then
     val[count] = hget_result[i]
     count = count + 1
  else
     key[count] = hget_result[i]
  end
  i = i + 1
end
return {key, val}
